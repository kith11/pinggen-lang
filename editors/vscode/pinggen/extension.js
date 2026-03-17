const vscode = require("vscode");
const cp = require("child_process");

class LspConnection {
  constructor(command) {
    this.command = command;
    this.process = null;
    this.buffer = Buffer.alloc(0);
    this.nextId = 1;
    this.pending = new Map();
    this.handlers = new Map();
  }

  start() {
    this.process = cp.spawn(this.command, ["lsp"], {
      shell: process.platform === "win32",
    });
    this.process.stdout.on("data", (chunk) => this.onData(chunk));
    this.process.stderr.on("data", () => {});
    this.process.on("exit", () => {
      for (const [, reject] of this.pending.values()) {
        reject(new Error("puff lsp exited"));
      }
      this.pending.clear();
    });
  }

  stop() {
    if (this.process) {
      this.notify("shutdown", {});
      this.notify("exit", {});
      this.process.kill();
      this.process = null;
    }
  }

  onNotification(method, handler) {
    this.handlers.set(method, handler);
  }

  request(method, params) {
    const id = this.nextId++;
    const payload = { jsonrpc: "2.0", id, method, params };
    return new Promise((resolve, reject) => {
      this.pending.set(id, [resolve, reject]);
      this.send(payload);
    });
  }

  notify(method, params) {
    this.send({ jsonrpc: "2.0", method, params });
  }

  send(message) {
    if (!this.process) {
      return;
    }
    const json = JSON.stringify(message);
    const frame = `Content-Length: ${Buffer.byteLength(json, "utf8")}\r\n\r\n${json}`;
    this.process.stdin.write(frame, "utf8");
  }

  onData(chunk) {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    while (true) {
      const headerEnd = this.buffer.indexOf("\r\n\r\n");
      if (headerEnd === -1) {
        return;
      }
      const header = this.buffer.slice(0, headerEnd).toString("utf8");
      const match = /Content-Length:\s*(\d+)/i.exec(header);
      if (!match) {
        this.buffer = Buffer.alloc(0);
        return;
      }
      const length = Number(match[1]);
      const total = headerEnd + 4 + length;
      if (this.buffer.length < total) {
        return;
      }
      const body = this.buffer.slice(headerEnd + 4, total).toString("utf8");
      this.buffer = this.buffer.slice(total);
      const message = JSON.parse(body);
      this.handleMessage(message);
    }
  }

  handleMessage(message) {
    if (Object.prototype.hasOwnProperty.call(message, "id")) {
      const pending = this.pending.get(message.id);
      if (!pending) {
        return;
      }
      this.pending.delete(message.id);
      if (message.error) {
        pending[1](new Error(message.error.message));
        return;
      }
      pending[0](message.result);
      return;
    }
    if (message.method && this.handlers.has(message.method)) {
      this.handlers.get(message.method)(message.params);
    }
  }
}

function asPosition(position) {
  return { line: position.line, character: position.character };
}

function asRange(range) {
  return new vscode.Range(range.start.line, range.start.character, range.end.line, range.end.character);
}

function asLocation(location) {
  return new vscode.Location(vscode.Uri.parse(location.uri), asRange(location.range));
}

function asCompletionItem(item) {
  const completion = new vscode.CompletionItem(
    item.label,
    item.kind ?? vscode.CompletionItemKind.Text
  );
  if (item.detail) {
    completion.detail = item.detail;
  }
  if (item.insertText) {
    completion.insertText = item.insertText;
  }
  return completion;
}

async function activate(context) {
  const config = vscode.workspace.getConfiguration("pinggen");
  const puffPath = config.get("puffPath", "puff");
  const client = new LspConnection(puffPath);
  const diagnostics = vscode.languages.createDiagnosticCollection("pinggen");
  context.subscriptions.push(diagnostics);

  client.onNotification("textDocument/publishDiagnostics", (params) => {
    const uri = vscode.Uri.parse(params.uri);
    const items = (params.diagnostics || []).map(
      (item) =>
        new vscode.Diagnostic(
          asRange(item.range),
          item.message,
          vscode.DiagnosticSeverity.Error
        )
    );
    diagnostics.set(uri, items);
  });

  client.start();
  await client.request("initialize", {
    processId: process.pid,
    rootUri: vscode.workspace.workspaceFolders?.[0]?.uri.toString() ?? null,
    capabilities: {},
  });
  client.notify("initialized", {});

  function syncOpenDocument(document) {
    if (document.languageId !== "pinggen") {
      return;
    }
    client.notify("textDocument/didOpen", {
      textDocument: {
        uri: document.uri.toString(),
        languageId: "pinggen",
        version: document.version,
        text: document.getText(),
      },
    });
  }

  vscode.workspace.textDocuments.forEach(syncOpenDocument);
  context.subscriptions.push(vscode.workspace.onDidOpenTextDocument(syncOpenDocument));
  context.subscriptions.push(
    vscode.workspace.onDidChangeTextDocument((event) => {
      if (event.document.languageId !== "pinggen") {
        return;
      }
      client.notify("textDocument/didChange", {
        textDocument: {
          uri: event.document.uri.toString(),
          version: event.document.version,
        },
        contentChanges: [{ text: event.document.getText() }],
      });
    })
  );
  context.subscriptions.push(
    vscode.workspace.onDidSaveTextDocument((document) => {
      if (document.languageId !== "pinggen") {
        return;
      }
      client.notify("textDocument/didSave", {
        textDocument: { uri: document.uri.toString() },
      });
    })
  );
  context.subscriptions.push(
    vscode.workspace.onDidCloseTextDocument((document) => {
      if (document.languageId !== "pinggen") {
        return;
      }
      client.notify("textDocument/didClose", {
        textDocument: { uri: document.uri.toString() },
      });
    })
  );

  context.subscriptions.push(
    vscode.languages.registerDefinitionProvider({ language: "pinggen" }, {
      async provideDefinition(document, position) {
        const result = await client.request("textDocument/definition", {
          textDocument: { uri: document.uri.toString() },
          position: asPosition(position),
        });
        if (!result) {
          return null;
        }
        return asLocation(result);
      },
    })
  );

  context.subscriptions.push(
    vscode.languages.registerHoverProvider({ language: "pinggen" }, {
      async provideHover(document, position) {
        const result = await client.request("textDocument/hover", {
          textDocument: { uri: document.uri.toString() },
          position: asPosition(position),
        });
        if (!result || !result.contents) {
          return null;
        }
        return new vscode.Hover(new vscode.MarkdownString(result.contents.value));
      },
    })
  );

  context.subscriptions.push(
    vscode.languages.registerCompletionItemProvider(
      { language: "pinggen" },
      {
        async provideCompletionItems(document, position) {
          const result = await client.request("textDocument/completion", {
            textDocument: { uri: document.uri.toString() },
            position: asPosition(position),
          });
          if (!result) {
            return [];
          }
          const items = Array.isArray(result) ? result : result.items || [];
          return items.map(asCompletionItem);
        },
      },
      ".",
      ":"
    )
  );

  context.subscriptions.push({ dispose: () => client.stop() });
}

function deactivate() {}

module.exports = { activate, deactivate };

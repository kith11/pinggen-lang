# pinggen VS Code support

This folder contains a minimal VS Code extension for `.pg` files.

Current support:

- syntax highlighting
- `#` line comments
- bracket and quote auto-closing
- live diagnostics via `puff lsp`
- completions
- go-to-definition
- hover

The extension launches `puff lsp`, so make sure `puff` is installed and available on your `PATH`, or set the `pinggen.puffPath` VS Code setting.

Install manually by copying this folder to your VS Code extensions directory with a versioned name, for example:

- Windows: `%USERPROFILE%\\.vscode\\extensions\\pinggen-language-0.1.0`

Then reload VS Code.

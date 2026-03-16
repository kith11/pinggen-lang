#include "pinggen/lexer.hpp"

#include <cctype>
#include <unordered_map>

#include "pinggen/diagnostics.hpp"

namespace pinggen {

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skip_whitespace();
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;
        const char c = peek();
        if (c == '\0') {
            tokens.push_back(make_token(TokenKind::EndOfFile, start_line, start_column, ""));
            return tokens;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(lex_identifier(start_line, start_column));
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(lex_number(start_line, start_column));
            continue;
        }

        switch (advance()) {
            case '{': tokens.push_back(make_token(TokenKind::LBrace, start_line, start_column, "{")); break;
            case '}': tokens.push_back(make_token(TokenKind::RBrace, start_line, start_column, "}")); break;
            case '(': tokens.push_back(make_token(TokenKind::LParen, start_line, start_column, "(")); break;
            case ')': tokens.push_back(make_token(TokenKind::RParen, start_line, start_column, ")")); break;
            case ',': tokens.push_back(make_token(TokenKind::Comma, start_line, start_column, ",")); break;
            case ';': tokens.push_back(make_token(TokenKind::Semicolon, start_line, start_column, ";")); break;
            case '+': tokens.push_back(make_token(TokenKind::Plus, start_line, start_column, "+")); break;
            case '-':
                if (match('>')) {
                    tokens.push_back(make_token(TokenKind::Arrow, start_line, start_column, "->"));
                } else {
                    tokens.push_back(make_token(TokenKind::Minus, start_line, start_column, "-"));
                }
                break;
            case '*': tokens.push_back(make_token(TokenKind::Star, start_line, start_column, "*")); break;
            case '/': tokens.push_back(make_token(TokenKind::Slash, start_line, start_column, "/")); break;
            case '.': tokens.push_back(make_token(TokenKind::Dot, start_line, start_column, ".")); break;
            case '=':
                if (match('=')) {
                    tokens.push_back(make_token(TokenKind::EqualEqual, start_line, start_column, "=="));
                } else {
                    tokens.push_back(make_token(TokenKind::Equal, start_line, start_column, "="));
                }
                break;
            case '!':
                if (!match('=')) {
                    fail({start_line, start_column}, "unexpected character");
                }
                tokens.push_back(make_token(TokenKind::BangEqual, start_line, start_column, "!="));
                break;
            case ':':
                if (match(':')) {
                    tokens.push_back(make_token(TokenKind::ColonColon, start_line, start_column, "::"));
                } else {
                    tokens.push_back(make_token(TokenKind::Colon, start_line, start_column, ":"));
                }
                break;
            case '"':
                --index_;
                --column_;
                tokens.push_back(lex_string(start_line, start_column));
                break;
            default:
                fail({start_line, start_column}, "unexpected character");
        }
    }
}

char Lexer::peek(std::size_t offset) const {
    if (index_ + offset >= source_.size()) {
        return '\0';
    }
    return source_[index_ + offset];
}

char Lexer::advance() {
    const char c = peek();
    if (c == '\0') {
        return c;
    }
    ++index_;
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (peek() != expected) {
        return false;
    }
    advance();
    return true;
}

void Lexer::skip_whitespace() {
    while (true) {
        const char c = peek();
        if (c == '#') {
            while (peek() != '\n' && peek() != '\0') {
                advance();
            }
            continue;
        }
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return;
        }
        advance();
    }
}

Token Lexer::make_token(TokenKind kind, std::size_t start_line, std::size_t start_column, std::string lexeme) {
    return Token{kind, std::move(lexeme), {start_line, start_column}};
}

Token Lexer::lex_identifier(std::size_t start_line, std::size_t start_column) {
    std::string value;
    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
        value.push_back(advance());
    }

    static const std::unordered_map<std::string, TokenKind> keywords = {
        {"import", TokenKind::KwImport},
        {"func", TokenKind::KwFunc},
        {"let", TokenKind::KwLet},
        {"mut", TokenKind::KwMut},
        {"return", TokenKind::KwReturn},
        {"true", TokenKind::KwTrue},
        {"false", TokenKind::KwFalse},
        {"if", TokenKind::KwIf},
        {"else", TokenKind::KwElse},
        {"while", TokenKind::KwWhile},
        {"struct", TokenKind::KwStruct},
    };

    const auto it = keywords.find(value);
    if (it != keywords.end()) {
        return make_token(it->second, start_line, start_column, value);
    }
    return make_token(TokenKind::Identifier, start_line, start_column, value);
}

Token Lexer::lex_number(std::size_t start_line, std::size_t start_column) {
    std::string value;
    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        value.push_back(advance());
    }
    return make_token(TokenKind::Integer, start_line, start_column, value);
}

Token Lexer::lex_string(std::size_t start_line, std::size_t start_column) {
    std::string value;
    advance();
    while (peek() != '"' && peek() != '\0') {
        const char c = advance();
        if (c == '\\') {
            const char escaped = advance();
            switch (escaped) {
                case 'n': value.push_back('\n'); break;
                case 't': value.push_back('\t'); break;
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                default: fail({line_, column_}, "unsupported escape sequence");
            }
        } else {
            value.push_back(c);
        }
    }
    if (!match('"')) {
        fail({start_line, start_column}, "unterminated string literal");
    }
    return make_token(TokenKind::String, start_line, start_column, value);
}

}  // namespace pinggen

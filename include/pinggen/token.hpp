#pragma once

#include <string>

namespace pinggen {

enum class TokenKind {
    EndOfFile,
    Identifier,
    Integer,
    String,
    KwImport,
    KwFunc,
    KwLet,
    KwMut,
    KwReturn,
    LBrace,
    RBrace,
    LParen,
    RParen,
    Comma,
    Semicolon,
    Plus,
    Minus,
    Star,
    Slash,
    Equal,
    Colon,
    ColonColon,
    Arrow
};

struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
};

struct Token {
    TokenKind kind = TokenKind::EndOfFile;
    std::string lexeme;
    SourceLocation location;
};

std::string token_kind_name(TokenKind kind);

}  // namespace pinggen

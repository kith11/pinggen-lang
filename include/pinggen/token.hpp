#pragma once

#include <string>

namespace pinggen {

enum class TokenKind {
    EndOfFile,
    Identifier,
    Integer,
    String,
    KwTrue,
    KwFalse,
    KwImport,
    KwStruct,
    KwFunc,
    KwLet,
    KwMut,
    KwReturn,
    KwIf,
    KwElse,
    KwWhile,
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
    EqualEqual,
    BangEqual,
    Dot,
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

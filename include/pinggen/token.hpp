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
    KwEnum,
    KwStruct,
    KwImpl,
    KwFunc,
    KwLet,
    KwMut,
    KwReturn,
    KwIf,
    KwElse,
    KwWhile,
    KwFor,
    KwIn,
    KwMatch,
    KwBreak,
    KwContinue,
    LBrace,
    RBrace,
    LParen,
    RParen,
    LBracket,
    RBracket,
    Comma,
    Semicolon,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Equal,
    Bang,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    EqualEqual,
    BangEqual,
    AndAnd,
    OrOr,
    Dot,
    DotDot,
    Colon,
    ColonColon,
    FatArrow,
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

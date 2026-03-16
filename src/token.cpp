#include "pinggen/token.hpp"

namespace pinggen {

std::string token_kind_name(TokenKind kind) {
    switch (kind) {
        case TokenKind::EndOfFile: return "end of file";
        case TokenKind::Identifier: return "identifier";
        case TokenKind::Integer: return "integer";
        case TokenKind::String: return "string";
        case TokenKind::KwTrue: return "true";
        case TokenKind::KwFalse: return "false";
        case TokenKind::KwImport: return "import";
        case TokenKind::KwStruct: return "struct";
        case TokenKind::KwImpl: return "impl";
        case TokenKind::KwFunc: return "func";
        case TokenKind::KwLet: return "let";
        case TokenKind::KwMut: return "mut";
        case TokenKind::KwReturn: return "return";
        case TokenKind::KwIf: return "if";
        case TokenKind::KwElse: return "else";
        case TokenKind::KwWhile: return "while";
        case TokenKind::KwBreak: return "break";
        case TokenKind::KwContinue: return "continue";
        case TokenKind::LBrace: return "{";
        case TokenKind::RBrace: return "}";
        case TokenKind::LParen: return "(";
        case TokenKind::RParen: return ")";
        case TokenKind::LBracket: return "[";
        case TokenKind::RBracket: return "]";
        case TokenKind::Comma: return ",";
        case TokenKind::Semicolon: return ";";
        case TokenKind::Plus: return "+";
        case TokenKind::Minus: return "-";
        case TokenKind::Star: return "*";
        case TokenKind::Slash: return "/";
        case TokenKind::Percent: return "%";
        case TokenKind::Equal: return "=";
        case TokenKind::Bang: return "!";
        case TokenKind::Less: return "<";
        case TokenKind::LessEqual: return "<=";
        case TokenKind::Greater: return ">";
        case TokenKind::GreaterEqual: return ">=";
        case TokenKind::EqualEqual: return "==";
        case TokenKind::BangEqual: return "!=";
        case TokenKind::AndAnd: return "&&";
        case TokenKind::OrOr: return "||";
        case TokenKind::Dot: return ".";
        case TokenKind::Colon: return ":";
        case TokenKind::ColonColon: return "::";
        case TokenKind::Arrow: return "->";
    }
    return "unknown";
}

}  // namespace pinggen

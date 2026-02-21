#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include <cstdint>

enum class TokenType {
    TOK_DEC,        // [0-9]+
    TOK_HEX,        // 0[xX][0-9A-Fa-f]+
    TOK_BITS,       // 0[bB][01]+
    TOK_STRING,     // "..."
    TOK_CHAR,       // '.'
    TOK_TRUE,       // true
    TOK_FALSE,      // false

    TOK_DEF,
    TOK_END,
    TOK_IF,
    TOK_THEN,
    TOK_ELSE,
    TOK_WHILE,
    TOK_UNTIL,
    TOK_BREAK,
    TOK_BEGIN,
    TOK_OF,
    TOK_BOOL,
    TOK_BYTE,
    TOK_INT,
    TOK_UINT,
    TOK_LONG,
    TOK_ULONG,
    TOK_CHARTYPE,
    TOK_STRINGTYPE,
    TOK_ARRAY,

    TOK_IDENT,

    TOK_PLUS,       // +
    TOK_MINUS,      // -
    TOK_STAR,       // *
    TOK_SLASH,      // /
    TOK_PERCENT,    // %
    TOK_AMP,        // &
    TOK_PIPE,       // |
    TOK_CARET,      // ^
    TOK_TILDE,      // ~
    TOK_BANG,       // !
    TOK_LT,         // <
    TOK_GT,         // >
    TOK_LE,         // <=
    TOK_GE,         // >=
    TOK_EQ,         // ==
    TOK_NE,         // !=
    TOK_AND,        // &&
    TOK_OR,         // ||
    TOK_SHL,        // <<
    TOK_SHR,        // >>
    TOK_ASSIGN,     // =
    TOK_INC,        // ++
    TOK_DEC_OP,     // --
    TOK_DOTDOT,     // ..

    TOK_LPAREN,     // (
    TOK_RPAREN,     // )
    TOK_LBRACKET,   // [
    TOK_RBRACKET,   // ]
    TOK_LBRACE,     // {
    TOK_RBRACE,     // }
    TOK_COMMA,      // ,
    TOK_SEMICOLON,  // ;

    TOK_EOF,
    TOK_ERROR
};

struct SourceLocation {
    int line;
    int column;
    int offset;
};

struct Token {
    TokenType type;
    std::string text;
    SourceLocation loc;
};

struct LexerError {
    std::string message;
    SourceLocation loc;
};

class Lexer {
public:
    explicit Lexer(const std::string& source);

    std::vector<Token> tokenize();
    const std::vector<LexerError>& errors() const { return errors_; }

private:
    char peek() const;
    char peekNext() const;
    char advance();
    bool isAtEnd() const;
    void skipWhitespaceAndComments();

    Token makeToken(TokenType type, const std::string& text, SourceLocation loc);
    Token readString();
    Token readChar();
    Token readNumber();
    Token readIdentOrKeyword();

    static TokenType keywordType(const std::string& word);

    std::string source_;
    size_t pos_;
    int line_;
    int col_;
    std::vector<LexerError> errors_;
};

#endif

#include "../include/lexer.h"
#include <cctype>
#include <unordered_map>

Lexer::Lexer(const std::string& source)
    : source_(source), pos_(0), line_(1), col_(1) {}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[pos_];
}

char Lexer::peekNext() const {
    if (pos_ + 1 >= source_.size()) return '\0';
    return source_[pos_ + 1];
}

char Lexer::advance() {
    char c = source_[pos_];
    pos_++;
    if (c == '\n') {
        line_++;
        col_ = 1;
    } else {
        col_++;
    }
    return c;
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

void Lexer::skipWhitespaceAndComments() {
    for (;;) {
        if (isAtEnd()) return;
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peekNext() == '/') {
            while (!isAtEnd() && peek() != '\n') advance();
        } else if (c == '/' && peekNext() == '*') {
            advance(); advance();
            int depth = 1;
            while (!isAtEnd() && depth > 0) {
                if (peek() == '/' && peekNext() == '*') {
                    advance(); advance();
                    depth++;
                } else if (peek() == '*' && peekNext() == '/') {
                    advance(); advance();
                    depth--;
                } else {
                    advance();
                }
            }
        } else {
            return;
        }
    }
}

Token Lexer::makeToken(TokenType type, const std::string& text, SourceLocation loc) {
    return Token{type, text, loc};
}

Token Lexer::readString() {
    SourceLocation loc{line_, col_, static_cast<int>(pos_)};
    advance();
    std::string val = "\"";
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\\') {
            val += advance();
            if (!isAtEnd()) val += advance();
        } else {
            val += advance();
        }
    }
    if (!isAtEnd()) {
        val += advance();
    } else {
        errors_.push_back({"Unterminated string literal", loc});
        return makeToken(TokenType::TOK_ERROR, val, loc);
    }
    return makeToken(TokenType::TOK_STRING, val, loc);
}

Token Lexer::readChar() {
    SourceLocation loc{line_, col_, static_cast<int>(pos_)};
    std::string val;
    val += advance();
    if (!isAtEnd() && peek() != '\'') {
        val += advance();
    }
    if (!isAtEnd() && peek() == '\'') {
        val += advance();
    } else {
        errors_.push_back({"Unterminated char literal", loc});
        return makeToken(TokenType::TOK_ERROR, val, loc);
    }
    return makeToken(TokenType::TOK_CHAR, val, loc);
}

Token Lexer::readNumber() {
    SourceLocation loc{line_, col_, static_cast<int>(pos_)};
    std::string val;

    if (peek() == '0' && (peekNext() == 'x' || peekNext() == 'X')) {
        val += advance(); // 0
        val += advance(); // x
        while (!isAtEnd() && std::isxdigit(static_cast<unsigned char>(peek()))) {
            val += advance();
        }
        return makeToken(TokenType::TOK_HEX, val, loc);
    }

    if (peek() == '0' && (peekNext() == 'b' || peekNext() == 'B')) {
        val += advance(); // 0
        val += advance(); // b
        while (!isAtEnd() && (peek() == '0' || peek() == '1')) {
            val += advance();
        }
        return makeToken(TokenType::TOK_BITS, val, loc);
    }

    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        val += advance();
    }
    return makeToken(TokenType::TOK_DEC, val, loc);
}

TokenType Lexer::keywordType(const std::string& word) {
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"def",    TokenType::TOK_DEF},
        {"end",    TokenType::TOK_END},
        {"if",     TokenType::TOK_IF},
        {"then",   TokenType::TOK_THEN},
        {"else",   TokenType::TOK_ELSE},
        {"while",  TokenType::TOK_WHILE},
        {"until",  TokenType::TOK_UNTIL},
        {"break",  TokenType::TOK_BREAK},
        {"begin",  TokenType::TOK_BEGIN},
        {"of",     TokenType::TOK_OF},
        {"bool",   TokenType::TOK_BOOL},
        {"byte",   TokenType::TOK_BYTE},
        {"int",    TokenType::TOK_INT},
        {"uint",   TokenType::TOK_UINT},
        {"long",   TokenType::TOK_LONG},
        {"ulong",  TokenType::TOK_ULONG},
        {"char",   TokenType::TOK_CHARTYPE},
        {"string", TokenType::TOK_STRINGTYPE},
        {"array",  TokenType::TOK_ARRAY},
        {"true",   TokenType::TOK_TRUE},
        {"false",  TokenType::TOK_FALSE},
    };
    auto it = keywords.find(word);
    if (it != keywords.end()) return it->second;
    return TokenType::TOK_IDENT;
}

Token Lexer::readIdentOrKeyword() {
    SourceLocation loc{line_, col_, static_cast<int>(pos_)};
    std::string val;
    while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
        val += advance();
    }
    return makeToken(keywordType(val), val, loc);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    tokens.reserve(source_.size() / 4);

    for (;;) {
        skipWhitespaceAndComments();
        if (isAtEnd()) {
            tokens.push_back(makeToken(TokenType::TOK_EOF, "", {line_, col_, static_cast<int>(pos_)}));
            break;
        }

        SourceLocation loc{line_, col_, static_cast<int>(pos_)};
        char c = peek();

        if (c == '"') { tokens.push_back(readString()); continue; }
        if (c == '\'') { tokens.push_back(readChar()); continue; }
        if (std::isdigit(static_cast<unsigned char>(c))) { tokens.push_back(readNumber()); continue; }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') { tokens.push_back(readIdentOrKeyword()); continue; }

        char n = peekNext();
        if (c == '=' && n == '=') { advance(); advance(); tokens.push_back(makeToken(TokenType::TOK_EQ, "==", loc)); continue; }
        if (c == '!' && n == '=') { advance(); advance(); tokens.push_back(makeToken(TokenType::TOK_NE, "!=", loc)); continue; }
        if (c == '<' && n == '=') { advance(); advance(); tokens.push_back(makeToken(TokenType::TOK_LE, "<=", loc)); continue; }
        if (c == '>' && n == '=') { advance(); advance(); tokens.push_back(makeToken(TokenType::TOK_GE, ">=", loc)); continue; }
        if (c == '<' && n == '<') { advance(); advance(); tokens.push_back(makeToken(TokenType::TOK_SHL, "<<", loc)); continue; }
        if (c == '>' && n == '>') { advance(); advance(); tokens.push_back(makeToken(TokenType::TOK_SHR, ">>", loc)); continue; }
        if (c == '&' && n == '&') { advance(); advance(); tokens.push_back(makeToken(TokenType::TOK_AND, "&&", loc)); continue; }
        if (c == '|' && n == '|') { advance(); advance(); tokens.push_back(makeToken(TokenType::TOK_OR, "||", loc)); continue; }
        if (c == '+' && n == '+') { advance(); advance(); tokens.push_back(makeToken(TokenType::TOK_INC, "++", loc)); continue; }
        if (c == '-' && n == '-') { advance(); advance(); tokens.push_back(makeToken(TokenType::TOK_DEC_OP, "--", loc)); continue; }
        if (c == '.' && n == '.') { advance(); advance(); tokens.push_back(makeToken(TokenType::TOK_DOTDOT, "..", loc)); continue; }

        advance();
        switch (c) {
            case '+': tokens.push_back(makeToken(TokenType::TOK_PLUS, "+", loc)); break;
            case '-': tokens.push_back(makeToken(TokenType::TOK_MINUS, "-", loc)); break;
            case '*': tokens.push_back(makeToken(TokenType::TOK_STAR, "*", loc)); break;
            case '/': tokens.push_back(makeToken(TokenType::TOK_SLASH, "/", loc)); break;
            case '%': tokens.push_back(makeToken(TokenType::TOK_PERCENT, "%", loc)); break;
            case '&': tokens.push_back(makeToken(TokenType::TOK_AMP, "&", loc)); break;
            case '|': tokens.push_back(makeToken(TokenType::TOK_PIPE, "|", loc)); break;
            case '^': tokens.push_back(makeToken(TokenType::TOK_CARET, "^", loc)); break;
            case '~': tokens.push_back(makeToken(TokenType::TOK_TILDE, "~", loc)); break;
            case '!': tokens.push_back(makeToken(TokenType::TOK_BANG, "!", loc)); break;
            case '<': tokens.push_back(makeToken(TokenType::TOK_LT, "<", loc)); break;
            case '>': tokens.push_back(makeToken(TokenType::TOK_GT, ">", loc)); break;
            case '=': tokens.push_back(makeToken(TokenType::TOK_ASSIGN, "=", loc)); break;
            case '(': tokens.push_back(makeToken(TokenType::TOK_LPAREN, "(", loc)); break;
            case ')': tokens.push_back(makeToken(TokenType::TOK_RPAREN, ")", loc)); break;
            case '[': tokens.push_back(makeToken(TokenType::TOK_LBRACKET, "[", loc)); break;
            case ']': tokens.push_back(makeToken(TokenType::TOK_RBRACKET, "]", loc)); break;
            case '{': tokens.push_back(makeToken(TokenType::TOK_LBRACE, "{", loc)); break;
            case '}': tokens.push_back(makeToken(TokenType::TOK_RBRACE, "}", loc)); break;
            case ',': tokens.push_back(makeToken(TokenType::TOK_COMMA, ",", loc)); break;
            case ';': tokens.push_back(makeToken(TokenType::TOK_SEMICOLON, ";", loc)); break;
            default:
                errors_.push_back({"Unexpected character: " + std::string(1, c), loc});
                tokens.push_back(makeToken(TokenType::TOK_ERROR, std::string(1, c), loc));
                break;
        }
    }
    return tokens;
}

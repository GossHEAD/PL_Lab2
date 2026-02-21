#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"
#include <vector>
#include <string>

struct ParseError {
    std::string message;
    SourceLocation loc;
};

struct ParseResult {
    ASTNodePtr tree;
    std::vector<ParseError> errors;
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);

    ParseResult parse();

private:
    const Token& current() const;
    const Token& peekToken() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token& expect(TokenType type, const std::string& context);
    bool isAtEnd() const;

    void error(const std::string& msg);
    void error(const std::string& msg, SourceLocation loc);
    void synchronize();

    ASTNodePtr parseSource();
    ASTNodePtr parseSourceItem();
    ASTNodePtr parseFuncDef();
    ASTNodePtr parseFuncSignature();
    ASTNodePtr parseFuncArg();
    ASTNodePtr parseTypeRef();
    ASTNodePtr parseStatement();
    ASTNodePtr parseIfStatement();
    ASTNodePtr parseLoopStatement();
    ASTNodePtr parseBlock();
    ASTNodePtr parseExpressionOrAssign();
    ASTNodePtr parseExpression();

    ASTNodePtr parseExprOr();
    ASTNodePtr parseExprAnd();
    ASTNodePtr parseExprComparison();
    ASTNodePtr parseExprBitOr();
    ASTNodePtr parseExprBitXor();
    ASTNodePtr parseExprBitAnd();
    ASTNodePtr parseExprShift();
    ASTNodePtr parseExprAdd();
    ASTNodePtr parseExprMul();
    ASTNodePtr parseExprUnary();
    ASTNodePtr parseExprPostfix();
    ASTNodePtr parseExprPrimary();

    bool isStatementStart() const;
    bool isExprStart() const;

    const std::vector<Token>& tokens_;
    size_t pos_;
    std::vector<ParseError> errors_;
};

#endif

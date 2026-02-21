#include "../include/parser.h"
#include <vector>
#include <stdexcept>

Parser::Parser(const std::vector<Token>& tokens)
    : tokens_(tokens), pos_(0) {}

const Token& Parser::current() const {
    return tokens_[pos_];
}

const Token& Parser::peekToken() const {
    if (pos_ + 1 < tokens_.size()) return tokens_[pos_ + 1];
    return tokens_.back(); // EOF
}

const Token& Parser::advance() {
    const Token& tok = tokens_[pos_];
    if (pos_ + 1 < tokens_.size()) pos_++;
    return tok;
}

bool Parser::check(TokenType type) const {
    return current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& context) {
    if (check(type)) {
        return advance();
    }
    error("expected '" + context + "', got '" + current().text + "'");
    return current();
}

bool Parser::isAtEnd() const {
    return current().type == TokenType::TOK_EOF;
}

void Parser::error(const std::string& msg) {
    errors_.push_back({msg, current().loc});
}

void Parser::error(const std::string& msg, SourceLocation loc) {
    errors_.push_back({msg, loc});
}

void Parser::synchronize() {
    while (!isAtEnd()) {
        if (check(TokenType::TOK_SEMICOLON)) { advance(); return; }
        if (check(TokenType::TOK_DEF) || check(TokenType::TOK_IF) ||
            check(TokenType::TOK_WHILE) || check(TokenType::TOK_UNTIL) ||
            check(TokenType::TOK_BREAK) || check(TokenType::TOK_END) ||
            check(TokenType::TOK_BEGIN) || check(TokenType::TOK_LBRACE)) {
            return;
        }
        advance();
    }
}

ParseResult Parser::parse() {
    auto tree = parseSource();
    return {std::move(tree), std::move(errors_)};
}

// source: sourceItem*
ASTNodePtr Parser::parseSource() {
    auto node = makeNode(ASTNode::SOURCE, current().loc);
    while (!isAtEnd()) {
        auto item = parseSourceItem();
        if (item) {
            node->addChild(std::move(item));
        } else {
            synchronize();
        }
    }
    return node;
}

// sourceItem: funcDef
ASTNodePtr Parser::parseSourceItem() {
    if (check(TokenType::TOK_DEF)) {
        return parseFuncDef();
    }
    error("expected function definition ('def')");
    return nullptr;
}

// funcDef: 'def' funcSignature (statement* 'end')?
ASTNodePtr Parser::parseFuncDef() {
    auto loc = current().loc;
    expect(TokenType::TOK_DEF, "def");

    auto node = makeNode(ASTNode::FUNC_DEF, loc);
    node->addChild(parseFuncSignature());

    if (!check(TokenType::TOK_END) && !isAtEnd() && !check(TokenType::TOK_DEF)) {
        while (!check(TokenType::TOK_END) && !isAtEnd()) {
            if (check(TokenType::TOK_DEF)) break;
            auto stmt = parseStatement();
            if (stmt) {
                node->addChild(std::move(stmt));
            } else {
                synchronize();
            }
        }
    }

    if (check(TokenType::TOK_END)) {
        advance();
    }

    return node;
}

// funcSignature: identifier '(' list<arg> ')' ('of' typeRef)?
ASTNodePtr Parser::parseFuncSignature() {
    auto loc = current().loc;
    auto node = makeNode(ASTNode::FUNC_SIGNATURE, loc);

    const Token& name = expect(TokenType::TOK_IDENT, "function name");
    node->value = name.text;

    expect(TokenType::TOK_LPAREN, "(");
    if (!check(TokenType::TOK_RPAREN)) {
        node->addChild(parseFuncArg());
        while (match(TokenType::TOK_COMMA)) {
            node->addChild(parseFuncArg());
        }
    }
    expect(TokenType::TOK_RPAREN, ")");

    if (match(TokenType::TOK_OF)) {
        node->addChild(parseTypeRef());
    }

    return node;
}

// arg: identifier ('of' typeRef)?
ASTNodePtr Parser::parseFuncArg() {
    auto loc = current().loc;
    const Token& name = expect(TokenType::TOK_IDENT, "argument name");
    auto node = makeNode(ASTNode::FUNC_ARG, loc, name.text);
    if (match(TokenType::TOK_OF)) {
        node->addChild(parseTypeRef());
    }
    return node;
}

// typeRef: builtin | custom | typeRef 'array' '[' dec ']'
ASTNodePtr Parser::parseTypeRef() {
    auto loc = current().loc;
    ASTNodePtr baseType;

    switch (current().type) {
        case TokenType::TOK_BOOL:
        case TokenType::TOK_BYTE:
        case TokenType::TOK_INT:
        case TokenType::TOK_UINT:
        case TokenType::TOK_LONG:
        case TokenType::TOK_ULONG:
        case TokenType::TOK_CHARTYPE:
        case TokenType::TOK_STRINGTYPE: {
            auto tok = advance();
            baseType = makeNode(ASTNode::TYPE_BUILTIN, loc, tok.text);
            break;
        }
        case TokenType::TOK_IDENT: {
            auto tok = advance();
            baseType = makeNode(ASTNode::TYPE_CUSTOM, loc, tok.text);
            break;
        }
        default:
            error("expected type name");
            return makeNode(ASTNode::TYPE_BUILTIN, loc, "<error>");
    }

    // Check for 'array' '[' dec ']' suffix
    while (check(TokenType::TOK_ARRAY)) {
        advance();
        expect(TokenType::TOK_LBRACKET, "[");
        const Token& dim = expect(TokenType::TOK_DEC, "array dimension");
        expect(TokenType::TOK_RBRACKET, "]");
        auto arrNode = makeNode(ASTNode::TYPE_ARRAY, loc, dim.text);
        arrNode->addChild(std::move(baseType));
        baseType = std::move(arrNode);
    }

    return baseType;
}

bool Parser::isStatementStart() const {
    switch (current().type) {
        case TokenType::TOK_IF:
        case TokenType::TOK_WHILE:
        case TokenType::TOK_UNTIL:
        case TokenType::TOK_BREAK:
        case TokenType::TOK_BEGIN:
        case TokenType::TOK_LBRACE:
            return true;
        default:
            return isExprStart();
    }
}

bool Parser::isExprStart() const {
    switch (current().type) {
        case TokenType::TOK_IDENT:
        case TokenType::TOK_DEC:
        case TokenType::TOK_HEX:
        case TokenType::TOK_BITS:
        case TokenType::TOK_STRING:
        case TokenType::TOK_CHAR:
        case TokenType::TOK_TRUE:
        case TokenType::TOK_FALSE:
        case TokenType::TOK_LPAREN:
        case TokenType::TOK_MINUS:
        case TokenType::TOK_TILDE:
        case TokenType::TOK_BANG:
        case TokenType::TOK_INC:
        case TokenType::TOK_DEC_OP:
            return true;
        default:
            return false;
    }
}

// statement: if | loop | repeat | break | block | expression/assign
ASTNodePtr Parser::parseStatement() {
    switch (current().type) {
        case TokenType::TOK_IF:
            return parseIfStatement();

        case TokenType::TOK_WHILE:
        case TokenType::TOK_UNTIL:
            return parseLoopStatement();

        case TokenType::TOK_BREAK: {
            auto loc = current().loc;
            advance();
            expect(TokenType::TOK_SEMICOLON, ";");
            return makeNode(ASTNode::STMT_BREAK, loc);
        }

        case TokenType::TOK_BEGIN:
        case TokenType::TOK_LBRACE:
            return parseBlock();

        default:
            return parseExpressionOrAssign();
    }
}

// if: 'if' expr 'then' statement ('else' statement)?
ASTNodePtr Parser::parseIfStatement() {
    auto loc = current().loc;
    expect(TokenType::TOK_IF, "if");

    auto node = makeNode(ASTNode::STMT_IF, loc);
    node->addChild(parseExpression());
    expect(TokenType::TOK_THEN, "then");
    node->addChild(parseStatement()); 

    if (match(TokenType::TOK_ELSE)) {
        node->addChild(parseStatement());
    }
    return node;
}

// loop: ('while'|'until') expr statement* 'end'
ASTNodePtr Parser::parseLoopStatement() {
    auto loc = current().loc;
    const Token& kw = advance();

    auto node = makeNode(ASTNode::STMT_LOOP, loc, kw.text);
    node->addChild(parseExpression());

    while (!check(TokenType::TOK_END) && !isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) {
            node->addChild(std::move(stmt));
        } else {
            synchronize();
        }
    }
    expect(TokenType::TOK_END, "end");
    return node;
}

// block: ('begin'|'{') (statement|sourceItem)* ('end'|'}')
ASTNodePtr Parser::parseBlock() {
    auto loc = current().loc;
    bool isBrace = check(TokenType::TOK_LBRACE);
    advance();

    auto node = makeNode(ASTNode::STMT_BLOCK, loc);

    TokenType closer = isBrace ? TokenType::TOK_RBRACE : TokenType::TOK_END;
    while (!check(closer) && !isAtEnd()) {
        if (check(TokenType::TOK_DEF)) {
            auto item = parseSourceItem();
            if (item) node->addChild(std::move(item));
            else synchronize();
        } else {
            auto stmt = parseStatement();
            if (stmt) node->addChild(std::move(stmt));
            else synchronize();
        }
    }
    expect(closer, isBrace ? "}" : "end");
    return node;
}

ASTNodePtr Parser::parseExpressionOrAssign() {
    auto loc = current().loc;
    auto expr = parseExpression();

    if (match(TokenType::TOK_ASSIGN)) {
        auto rhs = parseExpression();

        auto assignNode = makeNode(ASTNode::STMT_ASSIGN, loc);
        assignNode->addChild(std::move(expr));
        assignNode->addChild(std::move(rhs));

        if (check(TokenType::TOK_WHILE) || check(TokenType::TOK_UNTIL)) {
            const Token& kw = advance();
            auto repeatNode = makeNode(ASTNode::STMT_REPEAT, loc, kw.text);
            repeatNode->addChild(std::move(assignNode));
            repeatNode->addChild(parseExpression());    
            expect(TokenType::TOK_SEMICOLON, ";");
            return repeatNode;
        }

        expect(TokenType::TOK_SEMICOLON, ";");
        return assignNode;
    }

    // Check for repeat with plain expression body: expr ('while'|'until') expr ';'
    if (check(TokenType::TOK_WHILE) || check(TokenType::TOK_UNTIL)) {
        const Token& kw = advance();
        auto repeatNode = makeNode(ASTNode::STMT_REPEAT, loc, kw.text);

        auto bodyStmt = makeNode(ASTNode::STMT_EXPR, expr->loc);
        bodyStmt->addChild(std::move(expr));
        repeatNode->addChild(std::move(bodyStmt));

        repeatNode->addChild(parseExpression()); 
        expect(TokenType::TOK_SEMICOLON, ";");
        return repeatNode;
    }

    expect(TokenType::TOK_SEMICOLON, ";");
    auto stmtNode = makeNode(ASTNode::STMT_EXPR, loc);
    stmtNode->addChild(std::move(expr));
    return stmtNode;
}

ASTNodePtr Parser::parseExpression() {
    return parseExprOr();
}

ASTNodePtr Parser::parseExprOr() {
    auto left = parseExprAnd();
    while (check(TokenType::TOK_OR)) {
        auto loc = current().loc;
        auto op = advance();
        auto right = parseExprAnd();
        auto node = makeNode(ASTNode::EXPR_BINARY, loc, op.text);
        node->addChild(std::move(left));
        node->addChild(std::move(right));
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parseExprAnd() {
    auto left = parseExprComparison();
    while (check(TokenType::TOK_AND)) {
        auto loc = current().loc;
        auto op = advance();
        auto right = parseExprComparison();
        auto node = makeNode(ASTNode::EXPR_BINARY, loc, op.text);
        node->addChild(std::move(left));
        node->addChild(std::move(right));
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parseExprComparison() {
    auto left = parseExprBitOr();
    while (check(TokenType::TOK_LT) || check(TokenType::TOK_GT) ||
           check(TokenType::TOK_LE) || check(TokenType::TOK_GE) ||
           check(TokenType::TOK_EQ) || check(TokenType::TOK_NE)) {
        auto loc = current().loc;
        auto op = advance();
        auto right = parseExprBitOr();
        auto node = makeNode(ASTNode::EXPR_BINARY, loc, op.text);
        node->addChild(std::move(left));
        node->addChild(std::move(right));
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parseExprBitOr() {
    auto left = parseExprBitXor();
    while (check(TokenType::TOK_PIPE)) {
        auto loc = current().loc;
        auto op = advance();
        auto right = parseExprBitXor();
        auto node = makeNode(ASTNode::EXPR_BINARY, loc, op.text);
        node->addChild(std::move(left));
        node->addChild(std::move(right));
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parseExprBitXor() {
    auto left = parseExprBitAnd();
    while (check(TokenType::TOK_CARET)) {
        auto loc = current().loc;
        auto op = advance();
        auto right = parseExprBitAnd();
        auto node = makeNode(ASTNode::EXPR_BINARY, loc, op.text);
        node->addChild(std::move(left));
        node->addChild(std::move(right));
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parseExprBitAnd() {
    auto left = parseExprShift();
    while (check(TokenType::TOK_AMP)) {
        auto loc = current().loc;
        auto op = advance();
        auto right = parseExprShift();
        auto node = makeNode(ASTNode::EXPR_BINARY, loc, op.text);
        node->addChild(std::move(left));
        node->addChild(std::move(right));
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parseExprShift() {
    auto left = parseExprAdd();
    while (check(TokenType::TOK_SHL) || check(TokenType::TOK_SHR)) {
        auto loc = current().loc;
        auto op = advance();
        auto right = parseExprAdd();
        auto node = makeNode(ASTNode::EXPR_BINARY, loc, op.text);
        node->addChild(std::move(left));
        node->addChild(std::move(right));
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parseExprAdd() {
    auto left = parseExprMul();
    while (check(TokenType::TOK_PLUS) || check(TokenType::TOK_MINUS)) {
        auto loc = current().loc;
        auto op = advance();
        auto right = parseExprMul();
        auto node = makeNode(ASTNode::EXPR_BINARY, loc, op.text);
        node->addChild(std::move(left));
        node->addChild(std::move(right));
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parseExprMul() {
    auto left = parseExprUnary();
    while (check(TokenType::TOK_STAR) || check(TokenType::TOK_SLASH) || check(TokenType::TOK_PERCENT)) {
        auto loc = current().loc;
        auto op = advance();
        auto right = parseExprUnary();
        auto node = makeNode(ASTNode::EXPR_BINARY, loc, op.text);
        node->addChild(std::move(left));
        node->addChild(std::move(right));
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parseExprUnary() {
    if (check(TokenType::TOK_MINUS) || check(TokenType::TOK_TILDE) ||
        check(TokenType::TOK_BANG) || check(TokenType::TOK_INC) || check(TokenType::TOK_DEC_OP)) {
        auto loc = current().loc;
        auto op = advance();
        auto operand = parseExprUnary();
        auto node = makeNode(ASTNode::EXPR_UNARY, loc, op.text);
        node->addChild(std::move(operand));
        return node;
    }
    return parseExprPostfix();
}

ASTNodePtr Parser::parseExprPostfix() {
    auto expr = parseExprPrimary();

    for (;;) {
        if (check(TokenType::TOK_LPAREN)) {
            auto loc = current().loc;
            advance(); // '('
            auto callNode = makeNode(ASTNode::EXPR_CALL, loc);
            callNode->addChild(std::move(expr));
            if (!check(TokenType::TOK_RPAREN)) {
                callNode->addChild(parseExpression());
                while (match(TokenType::TOK_COMMA)) {
                    callNode->addChild(parseExpression());
                }
            }
            expect(TokenType::TOK_RPAREN, ")");
            expr = std::move(callNode);
        } else if (check(TokenType::TOK_LBRACKET)) {
            auto loc = current().loc;
            advance(); // '['
            auto sliceNode = makeNode(ASTNode::EXPR_SLICE, loc);
            sliceNode->addChild(std::move(expr));

            auto parseRange = [this]() -> ASTNodePtr {
                auto from = parseExpression();
                if (match(TokenType::TOK_DOTDOT)) {
                    auto to = parseExpression();
                    auto rangeNode = makeNode(ASTNode::EXPR_RANGE, from->loc);
                    rangeNode->addChild(std::move(from));
                    rangeNode->addChild(std::move(to));
                    return rangeNode;
                }
                return from;
            };

            sliceNode->addChild(parseRange());
            while (match(TokenType::TOK_COMMA)) {
                sliceNode->addChild(parseRange());
            }
            expect(TokenType::TOK_RBRACKET, "]");
            expr = std::move(sliceNode);
        } else if (check(TokenType::TOK_INC) || check(TokenType::TOK_DEC_OP)) {
            auto loc = current().loc;
            auto op = advance();
            auto node = makeNode(ASTNode::EXPR_UNARY, loc, "post" + op.text);
            node->addChild(std::move(expr));
            expr = std::move(node);
        } else {
            break;
        }
    }
    return expr;
}

ASTNodePtr Parser::parseExprPrimary() {
    auto loc = current().loc;

    if (match(TokenType::TOK_LPAREN)) {
        auto inner = parseExpression();
        expect(TokenType::TOK_RPAREN, ")");
        auto node = makeNode(ASTNode::EXPR_BRACES, loc);
        node->addChild(std::move(inner));
        return node;
    }

    if (check(TokenType::TOK_DEC) || check(TokenType::TOK_HEX) || check(TokenType::TOK_BITS) ||
        check(TokenType::TOK_STRING) || check(TokenType::TOK_CHAR) ||
        check(TokenType::TOK_TRUE) || check(TokenType::TOK_FALSE)) {
        auto tok = advance();
        return makeNode(ASTNode::EXPR_LITERAL, loc, tok.text);
    }

    if (check(TokenType::TOK_IDENT)) {
        auto tok = advance();
        return makeNode(ASTNode::EXPR_PLACE, loc, tok.text);
    }

    error("expected expression, got '" + current().text + "'");
    advance();
    return makeNode(ASTNode::EXPR_LITERAL, loc, "<error>");
}

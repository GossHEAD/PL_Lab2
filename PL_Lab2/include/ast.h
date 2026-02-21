#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <memory>
#include "lexer.h"

struct ASTNode;
using ASTNodePtr = std::unique_ptr<ASTNode>;

struct ASTNode {
    enum Kind {
        SOURCE,
        FUNC_DEF,
        FUNC_SIGNATURE,
        FUNC_ARG,

        TYPE_BUILTIN,
        TYPE_CUSTOM,
        TYPE_ARRAY,

        STMT_IF,
        STMT_LOOP,       // while/until ... end
        STMT_REPEAT,     // statement while/until expr;
        STMT_BREAK,
        STMT_EXPR,
        STMT_BLOCK,
        STMT_ASSIGN,

        EXPR_BINARY,
        EXPR_UNARY,
        EXPR_BRACES,
        EXPR_CALL,
        EXPR_SLICE,
        EXPR_RANGE,
        EXPR_PLACE,
        EXPR_LITERAL,
    };

    Kind kind;
    SourceLocation loc;
    std::string value;
    std::vector<ASTNodePtr> children;

    ASTNode(Kind k, SourceLocation l, const std::string& v = "")
        : kind(k), loc(l), value(v) {}

    void addChild(ASTNodePtr child) {
        children.push_back(std::move(child));
    }

    static const char* kindName(Kind k) {
        switch (k) {
            case SOURCE:          return "Source";
            case FUNC_DEF:        return "FuncDef";
            case FUNC_SIGNATURE:  return "FuncSignature";
            case FUNC_ARG:        return "FuncArg";
            case TYPE_BUILTIN:    return "TypeBuiltin";
            case TYPE_CUSTOM:     return "TypeCustom";
            case TYPE_ARRAY:      return "TypeArray";
            case STMT_IF:         return "If";
            case STMT_LOOP:       return "Loop";
            case STMT_REPEAT:     return "Repeat";
            case STMT_BREAK:      return "Break";
            case STMT_EXPR:       return "ExprStmt";
            case STMT_BLOCK:      return "Block";
            case STMT_ASSIGN:     return "Assign";
            case EXPR_BINARY:     return "BinaryExpr";
            case EXPR_UNARY:      return "UnaryExpr";
            case EXPR_BRACES:     return "Braces";
            case EXPR_CALL:       return "Call";
            case EXPR_SLICE:      return "Slice";
            case EXPR_RANGE:      return "Range";
            case EXPR_PLACE:      return "Place";
            case EXPR_LITERAL:    return "Literal";
        }
        return "Unknown";
    }

    const char* kindStr() const { return kindName(kind); }
};

inline ASTNodePtr makeNode(ASTNode::Kind k, SourceLocation loc, const std::string& val = "") {
    return std::make_unique<ASTNode>(k, loc, val);
}

#endif

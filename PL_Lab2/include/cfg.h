#ifndef CFG_H
#define CFG_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include "ast.h"


struct Operation {
    enum Kind {
        OP_ASSIGN,      
        OP_BINARY, 
        OP_UNARY,  
        OP_CALL,   
        OP_INDEX, 
        OP_SLICE, 
        OP_PLACE, 
        OP_LITERAL,
        OP_RETURN, 
    };
    Kind kind;
    std::string value;
    SourceLocation loc;
    std::vector<std::unique_ptr<Operation>> operands;

    Operation(Kind k, const std::string& v, SourceLocation l) : kind(k), value(v), loc(l) {}

    void addOperand(std::unique_ptr<Operation> op) {
        operands.push_back(std::move(op));
    }
    static const char* kindName(Kind k) {
        switch (k) {
            case OP_ASSIGN: return "assign";
            case OP_BINARY: return "binary";
            case OP_UNARY: return "unary";
            case OP_CALL: return "call";
            case OP_INDEX: return "index";
            case OP_SLICE: return "slice";
            case OP_PLACE: return "place";
            case OP_LITERAL: return "literal";
            case OP_RETURN: return "return";
        }
    };
};

using OperationPtr = std::unique_ptr<Operation>;

struct BasicBlock {
    int id;
    std::string label;
    std::vector<OperationPtr> operations;

    int unconditionalNext;
    int conditionalTrue;  
    int conditionalFalse; 
    OperationPtr condition;

    BasicBlock(int id_, const std::string& label_) : id(id_), label(label_), unconditionalNext(-1), conditionalTrue(-1), conditionalFalse(-1) {}
};

struct ControlFlowGraph {
    std::vector<std::unique_ptr<BasicBlock>> blocks;
    int entryBlockId; 

    BasicBlock* createBlock(const std::string& label) {
        int id = static_cast<int>(blocks.size());
        blocks.push_back(std::make_unique<BasicBlock>(id, label));
        return blocks.back().get();
    }
    BasicBlock* getBlock(int id) {
        if (id >= 0 && id < static_cast<int>(blocks.size())) {
            return blocks[id].get();
        }
        return nullptr;
    }
};

struct ArgInfo {
    std::string name;
    std::string typeName;
};

struct FunctionSignature {
    std::string name;
    std::vector<ArgInfo> args;
    std::string returnType;
};

struct FunctionInfo {
    FunctionSignature signature;
    std::string sourceFile;
    SourceLocation loc;
    ControlFlowGraph cfg;
};

struct ProgramInfo {
    std::vector<std::unique_ptr<FunctionInfo>> functions;

    std::map<std::string, std::set<std::string>> callGraph;
};


struct AnalysisError {
    std::string message;
    std::string sourceFile;
    SourceLocation loc;
};


struct SourceFileInput {
    std::string fileName;
    const ASTNode* tree; 
};

struct CFGBuildResult {
    std::unique_ptr<ProgramInfo> program;
    std::vector<AnalysisError> errors;
};

#endif
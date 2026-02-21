#ifndef CFG_BUILDER_H
#define CFG_BUILDER_H

#include "cfg.h"
#include <vector>

class CFGBuilder {
public:
    CFGBuildResult build(const std::vector<SourceFileInput>& inputs);

private:
    void processFile(const SourceFileInput& input);
    void extractFunctions(const ASTNode* node, const std::string& fileName);
    void buildFunctionCFG(FunctionInfo& func, const ASTNode* funcDef);
    FunctionSignature extractSignature(const ASTNode* sigNode);
    OperationPtr convertExpr(const ASTNode* node);
    int processStatement(const ASTNode* stmt, ControlFlowGraph& cfg,
        int currentBlockId, int loopExitBlockId);
    int processStatements(const std::vector<ASTNodePtr>& stmts, size_t startIdx,
        ControlFlowGraph& cfg, int currentBlockId, int loopExitBlockId);
    void collectCalls(const Operation* op, const std::string& callerName);

    ProgramInfo* program_;
    std::vector<AnalysisError> errors_;
    std::string currentFile_;

    void addError(const std::string& msg, SourceLocation loc);
};

#endif
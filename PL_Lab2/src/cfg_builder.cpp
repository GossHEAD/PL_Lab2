#include "../include/cfg_builder.h"

void CFGBuilder::addError(const std::string& msg, SourceLocation loc) {
    errors_.push_back({ msg, currentFile_, loc });
}

CFGBuildResult CFGBuilder::build(const std::vector<SourceFileInput>& inputs) {
    auto program = std::make_unique<ProgramInfo>();
    program_ = program.get();
    errors_.clear();

    for (const auto& input : inputs) {
        processFile(input);
    }

    return { std::move(program), std::move(errors_) };
}

void CFGBuilder::processFile(const SourceFileInput& input) {
    currentFile_ = input.fileName;
    if (input.tree == nullptr) {
        addError("null AST for file", { 0, 0, 0 });
        return;
    }
    extractFunctions(input.tree, input.fileName);
}

void CFGBuilder::extractFunctions(const ASTNode* node, const std::string& fileName) {
    if (node->kind == ASTNode::SOURCE) {
        for (const auto& child : node->children) {
            extractFunctions(child.get(), fileName);
        }
        return;
    }

    if (node->kind == ASTNode::FUNC_DEF) {
        auto func = std::make_unique<FunctionInfo>();
        func->sourceFile = fileName;
        func->loc = node->loc;

        if (!node->children.empty() && node->children[0]->kind == ASTNode::FUNC_SIGNATURE) {
            func->signature = extractSignature(node->children[0].get());
        }
        else {
            addError("function definition without signature", node->loc);
            return;
        }

        buildFunctionCFG(*func, node);
        program_->functions.push_back(std::move(func));
        return;
    }

    if (node->kind == ASTNode::STMT_BLOCK) {
        for (const auto& child : node->children) {
            if (child->kind == ASTNode::FUNC_DEF) {
                extractFunctions(child.get(), fileName);
            }
        }
    }
}

FunctionSignature CFGBuilder::extractSignature(const ASTNode* sigNode) {
    FunctionSignature sig;
    sig.name = sigNode->value;

    for (const auto& child : sigNode->children) {
        if (child->kind == ASTNode::FUNC_ARG) {
            ArgInfo arg;
            arg.name = child->value;
            if (!child->children.empty()) {
                const ASTNode* typeNode = child->children[0].get();
                if (typeNode->kind == ASTNode::TYPE_BUILTIN || typeNode->kind == ASTNode::TYPE_CUSTOM) {
                    arg.typeName = typeNode->value;
                }
                else if (typeNode->kind == ASTNode::TYPE_ARRAY) {
                    arg.typeName = typeNode->children.empty() ? "?" : typeNode->children[0]->value;
                    arg.typeName += " array[" + typeNode->value + "]";
                }
            }
            sig.args.push_back(std::move(arg));
        }
        else if (child->kind == ASTNode::TYPE_BUILTIN || child->kind == ASTNode::TYPE_CUSTOM ||
            child->kind == ASTNode::TYPE_ARRAY) {
            if (child->kind == ASTNode::TYPE_ARRAY) {
                sig.returnType = child->children.empty() ? "?" : child->children[0]->value;
                sig.returnType += " array[" + child->value + "]";
            }
            else {
                sig.returnType = child->value;
            }
        }
    }
    return sig;
}

OperationPtr CFGBuilder::convertExpr(const ASTNode* node) {
    if (node == nullptr) return nullptr;

    switch (node->kind) {
    case ASTNode::EXPR_LITERAL: {
        return std::make_unique<Operation>(Operation::OP_LITERAL, node->value, node->loc);
    }

    case ASTNode::EXPR_PLACE: {
        return std::make_unique<Operation>(Operation::OP_PLACE, node->value, node->loc);
    }

    case ASTNode::EXPR_BINARY: {
        auto op = std::make_unique<Operation>(Operation::OP_BINARY, node->value, node->loc);
        if (node->children.size() >= 2) {
            op->addOperand(convertExpr(node->children[0].get()));
            op->addOperand(convertExpr(node->children[1].get()));
        }
        return op;
    }

    case ASTNode::EXPR_UNARY: {
        auto op = std::make_unique<Operation>(Operation::OP_UNARY, node->value, node->loc);
        if (!node->children.empty()) {
            op->addOperand(convertExpr(node->children[0].get()));
        }
        return op;
    }

    case ASTNode::EXPR_BRACES: {
        if (!node->children.empty()) {
            return convertExpr(node->children[0].get());
        }
        return nullptr;
    }

    case ASTNode::EXPR_CALL: {
        auto op = std::make_unique<Operation>(Operation::OP_CALL, "", node->loc);
        if (!node->children.empty()) {
            op->addOperand(convertExpr(node->children[0].get()));
            for (size_t i = 1; i < node->children.size(); ++i) {
                op->addOperand(convertExpr(node->children[i].get()));
            }
        }
        return op;
    }

    case ASTNode::EXPR_SLICE: {
        if (node->children.size() >= 2) {
            bool hasRange = false;
            for (size_t i = 1; i < node->children.size(); ++i) {
                if (node->children[i]->kind == ASTNode::EXPR_RANGE) {
                    hasRange = true;
                    break;
                }
            }

            auto op = std::make_unique<Operation>(
                hasRange ? Operation::OP_SLICE : Operation::OP_INDEX,
                "", node->loc);
            for (const auto& child : node->children) {
                op->addOperand(convertExpr(child.get()));
            }
            return op;
        }
        return nullptr;
    }

    case ASTNode::EXPR_RANGE: {
        auto op = std::make_unique<Operation>(Operation::OP_BINARY, "..", node->loc);
        if (node->children.size() >= 2) {
            op->addOperand(convertExpr(node->children[0].get()));
            op->addOperand(convertExpr(node->children[1].get()));
        }
        return op;
    }

    default: {
        addError("unexpected AST node kind in expression: " +
            std::string(ASTNode::kindName(node->kind)), node->loc);
        return std::make_unique<Operation>(Operation::OP_LITERAL, "<error>", node->loc);
    }
    }
}

void CFGBuilder::buildFunctionCFG(FunctionInfo& func, const ASTNode* funcDef) {
    ControlFlowGraph& cfg = func.cfg;

    BasicBlock* entry = cfg.createBlock("entry");
    cfg.entryBlockId = entry->id;

    BasicBlock* exitBlock = cfg.createBlock("exit");

    if (funcDef->children.size() <= 1) {
        entry->unconditionalNext = exitBlock->id;
        return;
    }

    int currentBlockId = entry->id;

    for (size_t i = 1; i < funcDef->children.size(); ++i) {
        const ASTNode* stmt = funcDef->children[i].get();
        currentBlockId = processStatement(stmt, cfg, currentBlockId, -1);
        if (currentBlockId < 0) {
            break;
        }
    }

    if (currentBlockId >= 0) {
        BasicBlock* lastBlock = cfg.getBlock(currentBlockId);
        if (lastBlock && lastBlock->unconditionalNext < 0 &&
            lastBlock->conditionalTrue < 0) {
            lastBlock->unconditionalNext = exitBlock->id;
        }
    }

    for (const auto& block : cfg.blocks) {
        for (const auto& op : block->operations) {
            collectCalls(op.get(), func.signature.name);
        }
        if (block->condition) {
            collectCalls(block->condition.get(), func.signature.name);
        }
    }
}

int CFGBuilder::processStatement(const ASTNode* stmt, ControlFlowGraph& cfg,
    int currentBlockId, int loopExitBlockId) {
    if (currentBlockId < 0) return -1;
    BasicBlock* currentBlock = cfg.getBlock(currentBlockId);
    if (currentBlock == nullptr) return -1;

    switch (stmt->kind) {
    case ASTNode::STMT_EXPR: {
        if (!stmt->children.empty()) {
            auto op = convertExpr(stmt->children[0].get());
            if (op) currentBlock->operations.push_back(std::move(op));
        }
        return currentBlockId;
    }

    case ASTNode::STMT_ASSIGN: {
        if (stmt->children.size() >= 2) {
            auto assignOp = std::make_unique<Operation>(
                Operation::OP_ASSIGN, "=", stmt->loc);
            assignOp->addOperand(convertExpr(stmt->children[0].get()));
            assignOp->addOperand(convertExpr(stmt->children[1].get()));
            currentBlock->operations.push_back(std::move(assignOp));
        }
        return currentBlockId;
    }

    case ASTNode::STMT_IF: {
        if (stmt->children.empty()) {
            addError("if statement without condition", stmt->loc);
            return currentBlockId;
        }

        auto condOp = convertExpr(stmt->children[0].get());

        BasicBlock* thenBlock = cfg.createBlock("if.then");
        BasicBlock* mergeBlock = cfg.createBlock("if.merge");

        currentBlock->condition = std::move(condOp);
        currentBlock->conditionalTrue = thenBlock->id;

        bool hasElse = (stmt->children.size() >= 3);
        BasicBlock* elseBlock = nullptr;
        if (hasElse) {
            elseBlock = cfg.createBlock("if.else");
            currentBlock->conditionalFalse = elseBlock->id;
        }
        else {
            currentBlock->conditionalFalse = mergeBlock->id;
        }

        int thenExit = processStatement(stmt->children[1].get(), cfg,
            thenBlock->id, loopExitBlockId);
        if (thenExit >= 0) {
            BasicBlock* thenExitBlock = cfg.getBlock(thenExit);
            if (thenExitBlock && thenExitBlock->unconditionalNext < 0 &&
                thenExitBlock->conditionalTrue < 0) {
                thenExitBlock->unconditionalNext = mergeBlock->id;
            }
        }

        if (hasElse) {
            int elseExit = processStatement(stmt->children[2].get(), cfg,
                elseBlock->id, loopExitBlockId);
            if (elseExit >= 0) {
                BasicBlock* elseExitBlock = cfg.getBlock(elseExit);
                if (elseExitBlock && elseExitBlock->unconditionalNext < 0 &&
                    elseExitBlock->conditionalTrue < 0) {
                    elseExitBlock->unconditionalNext = mergeBlock->id;
                }
            }
        }

        return mergeBlock->id;
    }

    case ASTNode::STMT_LOOP: {
        if (stmt->children.empty()) {
            addError("loop without condition", stmt->loc);
            return currentBlockId;
        }

        bool isUntil = (stmt->value == "until");

        BasicBlock* condBlock = cfg.createBlock("loop.cond");
        BasicBlock* bodyBlock = cfg.createBlock("loop.body");
        BasicBlock* exitLoopBlock = cfg.createBlock("loop.exit");

        currentBlock->unconditionalNext = condBlock->id;

        auto condOp = convertExpr(stmt->children[0].get());
        condBlock->condition = std::move(condOp);

        if (isUntil) {
            condBlock->conditionalTrue = exitLoopBlock->id;
            condBlock->conditionalFalse = bodyBlock->id;
        }
        else {
            condBlock->conditionalTrue = bodyBlock->id;
            condBlock->conditionalFalse = exitLoopBlock->id;
        }

        int bodyExitId = bodyBlock->id;
        for (size_t i = 1; i < stmt->children.size(); ++i) {
            bodyExitId = processStatement(stmt->children[i].get(), cfg,
                bodyExitId, exitLoopBlock->id);
            if (bodyExitId < 0) break;
        }

        if (bodyExitId >= 0) {
            BasicBlock* bodyExitBlock = cfg.getBlock(bodyExitId);
            if (bodyExitBlock && bodyExitBlock->unconditionalNext < 0 &&
                bodyExitBlock->conditionalTrue < 0) {
                bodyExitBlock->unconditionalNext = condBlock->id;
            }
        }

        return exitLoopBlock->id;
    }

    case ASTNode::STMT_REPEAT: {
        if (stmt->children.size() < 2) {
            addError("repeat statement without body or condition", stmt->loc);
            return currentBlockId;
        }

        bool isUntil = (stmt->value == "until");

        BasicBlock* bodyBlock = cfg.createBlock("repeat.body");
        BasicBlock* condBlock = cfg.createBlock("repeat.cond");
        BasicBlock* exitRepeatBlock = cfg.createBlock("repeat.exit");

        currentBlock->unconditionalNext = bodyBlock->id;

        int bodyExitId = processStatement(stmt->children[0].get(), cfg,
            bodyBlock->id, exitRepeatBlock->id);

        if (bodyExitId >= 0) {
            BasicBlock* bodyExitBlock = cfg.getBlock(bodyExitId);
            if (bodyExitBlock && bodyExitBlock->unconditionalNext < 0 &&
                bodyExitBlock->conditionalTrue < 0) {
                bodyExitBlock->unconditionalNext = condBlock->id;
            }
        }

        auto condOp = convertExpr(stmt->children[1].get());
        condBlock->condition = std::move(condOp);

        if (isUntil) {
            condBlock->conditionalTrue = exitRepeatBlock->id;
            condBlock->conditionalFalse = bodyBlock->id;
        }
        else {
            condBlock->conditionalTrue = bodyBlock->id;
            condBlock->conditionalFalse = exitRepeatBlock->id;
        }

        return exitRepeatBlock->id;
    }

    case ASTNode::STMT_BREAK: {
        if (loopExitBlockId < 0) {
            addError("break outside of loop", stmt->loc);
            return currentBlockId;
        }
        currentBlock->unconditionalNext = loopExitBlockId;
        return -1;
    }

    case ASTNode::STMT_BLOCK: {
        int blockCurrent = currentBlockId;
        for (const auto& child : stmt->children) {
            if (child->kind == ASTNode::FUNC_DEF) {
                continue;
            }
            blockCurrent = processStatement(child.get(), cfg,
                blockCurrent, loopExitBlockId);
            if (blockCurrent < 0) break;
        }
        return blockCurrent;
    }

    default: {
        addError("unexpected statement kind: " +
            std::string(ASTNode::kindName(stmt->kind)), stmt->loc);
        return currentBlockId;
    }
    }
}

void CFGBuilder::collectCalls(const Operation* op, const std::string& callerName) {
    if (op == nullptr) return;

    if (op->kind == Operation::OP_CALL) {
        if (!op->operands.empty() && op->operands[0] != nullptr) {
            if (op->operands[0]->kind == Operation::OP_PLACE) {
                program_->callGraph[callerName].insert(op->operands[0]->value);
            }
        }
    }

    for (const auto& operand : op->operands) {
        collectCalls(operand.get(), callerName);
    }
}
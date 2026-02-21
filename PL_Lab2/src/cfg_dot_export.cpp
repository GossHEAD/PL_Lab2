#include "../include/cfg_dot_export.h"
#include <sstream>

std::string CFGDotExporter::escape(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '"':  result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '<':  result += "\\<"; break;
        case '>':  result += "\\>"; break;
        case '{':  result += "\\{"; break;
        case '}':  result += "\\}"; break;
        case '|':  result += "\\|"; break;
        case '\n': result += "\\n"; break;
        default:   result += c; break;
        }
    }
    return result;
}

std::string CFGDotExporter::operationToString(const Operation* op, int depth) {
    if (op == nullptr) return "<null>";
    if (depth > 20) return "...";

    std::string result;

    switch (op->kind) {
    case Operation::OP_LITERAL:
        result = op->value;
        break;

    case Operation::OP_PLACE:
        result = op->value;
        break;

    case Operation::OP_ASSIGN:
        if (op->operands.size() >= 2) {
            result = operationToString(op->operands[0].get(), depth + 1)
                + " = "
                + operationToString(op->operands[1].get(), depth + 1);
        }
        break;

    case Operation::OP_BINARY:
        if (op->operands.size() >= 2) {
            result = "("
                + operationToString(op->operands[0].get(), depth + 1)
                + " " + op->value + " "
                + operationToString(op->operands[1].get(), depth + 1)
                + ")";
        }
        break;

    case Operation::OP_UNARY:
        if (!op->operands.empty()) {
            if (op->value.substr(0, 4) == "post") {
                result = operationToString(op->operands[0].get(), depth + 1)
                    + op->value.substr(4);
            }
            else {
                result = op->value
                    + operationToString(op->operands[0].get(), depth + 1);
            }
        }
        break;

    case Operation::OP_CALL:
        if (!op->operands.empty()) {
            result = operationToString(op->operands[0].get(), depth + 1) + "(";
            for (size_t i = 1; i < op->operands.size(); ++i) {
                if (i > 1) result += ", ";
                result += operationToString(op->operands[i].get(), depth + 1);
            }
            result += ")";
        }
        break;

    case Operation::OP_INDEX:
        if (op->operands.size() >= 2) {
            result = operationToString(op->operands[0].get(), depth + 1) + "[";
            for (size_t i = 1; i < op->operands.size(); ++i) {
                if (i > 1) result += ", ";
                result += operationToString(op->operands[i].get(), depth + 1);
            }
            result += "]";
        }
        break;

    case Operation::OP_SLICE:
        if (op->operands.size() >= 2) {
            result = operationToString(op->operands[0].get(), depth + 1) + "[";
            for (size_t i = 1; i < op->operands.size(); ++i) {
                if (i > 1) result += ", ";
                result += operationToString(op->operands[i].get(), depth + 1);
            }
            result += "]";
        }
        break;

    case Operation::OP_RETURN:
        result = "return";
        if (!op->operands.empty()) {
            result += " " + operationToString(op->operands[0].get(), depth + 1);
        }
        break;
    }

    return result;
}

std::string CFGDotExporter::blockLabel(const BasicBlock& block) {
    std::ostringstream oss;
    oss << block.label << " (BB" << block.id << ")";

    if (!block.operations.empty()) {
        oss << "\\l";
        for (const auto& op : block.operations) {
            oss << escape(operationToString(op.get())) << "\\l";
        }
    }

    if (block.condition) {
        oss << "---\\lcond: " << escape(operationToString(block.condition.get())) << "\\l";
    }

    return oss.str();
}

void CFGDotExporter::exportCFG(const FunctionInfo& func, std::ostream& out) {
    out << "digraph \"" << escape(func.signature.name) << "\" {\n";
    out << "  label=\"" << escape(func.signature.name) << "\";\n";
    out << "  labelloc=t;\n";
    out << "  node [shape=record, fontname=\"monospace\", fontsize=9];\n";
    out << "  edge [fontname=\"monospace\", fontsize=8];\n\n";

    const ControlFlowGraph& cfg = func.cfg;
    for (const auto& block : cfg.blocks) {
        std::string style;
        if (block->id == cfg.entryBlockId) {
            style = ", style=filled, fillcolor=lightgreen";
        }
        else if (block->label == "exit") {
            style = ", style=filled, fillcolor=lightyellow";
        }

        out << "  bb" << block->id
            << " [label=\"" << blockLabel(*block) << "\"" << style << "];\n";
    }

    out << "\n";

    for (const auto& block : cfg.blocks) {
        if (block->unconditionalNext >= 0) {
            out << "  bb" << block->id << " -> bb" << block->unconditionalNext;
            if (block->condition) {
            }
            out << ";\n";
        }
        if (block->conditionalTrue >= 0) {
            out << "  bb" << block->id << " -> bb" << block->conditionalTrue
                << " [label=\"true\", color=green];\n";
        }
        if (block->conditionalFalse >= 0) {
            out << "  bb" << block->id << " -> bb" << block->conditionalFalse
                << " [label=\"false\", color=red];\n";
        }
    }

    out << "}\n";
}

void CFGDotExporter::exportCallGraph(const ProgramInfo& program, std::ostream& out) {
    out << "digraph CallGraph {\n";
    out << "  label=\"Call Graph\";\n";
    out << "  labelloc=t;\n";
    out << "  node [shape=box, fontname=\"monospace\", fontsize=10];\n\n";

    std::set<std::string> allFunctions;
    for (const auto& func : program.functions) {
        allFunctions.insert(func->signature.name);
    }

    for (const auto& name : allFunctions) {
        out << "  \"" << escape(name) << "\";\n";
    }

    for (const auto& [caller, callees] : program.callGraph) {
        for (const auto& callee : callees) {
            if (allFunctions.find(callee) == allFunctions.end()) {
                out << "  \"" << escape(callee)
                    << "\" [style=dashed, color=gray];\n";
                allFunctions.insert(callee); // avoid duplicates
            }
        }
    }

    out << "\n";

    for (const auto& [caller, callees] : program.callGraph) {
        for (const auto& callee : callees) {
            out << "  \"" << escape(caller) << "\" -> \"" << escape(callee) << "\";\n";
        }
    }

    out << "}\n";
}
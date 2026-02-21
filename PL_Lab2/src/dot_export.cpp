#include "../include/dot_export.h"
#include <sstream>

std::string DotExporter::escape(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            default:   result += c; break;
        }
    }
    return result;
}

void DotExporter::visitNode(const ASTNode* node, int& nextId, int parentId, std::ostream& out) {
    int myId = nextId++;

    std::string label = node->kindStr();
    if (!node->value.empty()) {
        label += "\\n" + escape(node->value);
    }
    label += "\\n[" + std::to_string(node->loc.line) + ":" + std::to_string(node->loc.column) + "]";

    out << "  n" << myId << " [label=\"" << label << "\"];\n";

    if (parentId >= 0) {
        out << "  n" << parentId << " -> n" << myId << ";\n";
    }

    for (const auto& child : node->children) {
        if (child) {
            visitNode(child.get(), nextId, myId, out);
        }
    }
}

void DotExporter::exportTree(const ASTNode* root, std::ostream& out) {
    out << "digraph AST {\n";
    out << "  node [shape=box, fontname=\"monospace\", fontsize=10];\n";
    out << "  edge [arrowsize=0.7];\n";

    if (root) {
        int nextId = 0;
        visitNode(root, nextId, -1, out);
    }

    out << "}\n";
}

std::string DotExporter::exportTree(const ASTNode* root) {
    std::ostringstream oss;
    exportTree(root, oss);
    return oss.str();
}

#include "../include/json_export.h"
#include <sstream>

std::string JsonExporter::escape(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            case '\r': result += "\\r"; break;
            default:   result += c; break;
        }
    }
    return result;
}

void JsonExporter::writeIndent(std::ostream& out, int indent) {
    for (int i = 0; i < indent; ++i) out << "  ";
}

void JsonExporter::visitNode(const ASTNode* node, std::ostream& out, int indent) {
    writeIndent(out, indent);
    out << "{\n";

    writeIndent(out, indent + 1);
    out << "\"kind\": \"" << node->kindStr() << "\"";

    if (!node->value.empty()) {
        out << ",\n";
        writeIndent(out, indent + 1);
        out << "\"value\": \"" << escape(node->value) << "\"";
    }

    out << ",\n";
    writeIndent(out, indent + 1);
    out << "\"loc\": {\"line\": " << node->loc.line
        << ", \"col\": " << node->loc.column << "}";

    if (!node->children.empty()) {
        out << ",\n";
        writeIndent(out, indent + 1);
        out << "\"children\": [\n";
        for (size_t i = 0; i < node->children.size(); ++i) {
            if (node->children[i]) {
                visitNode(node->children[i].get(), out, indent + 2);
            }
            if (i + 1 < node->children.size()) out << ",";
            out << "\n";
        }
        writeIndent(out, indent + 1);
        out << "]";
    }

    out << "\n";
    writeIndent(out, indent);
    out << "}";
}

void JsonExporter::exportTree(const ASTNode* root, std::ostream& out) {
    if (root) {
        visitNode(root, out, 0);
        out << "\n";
    }
}

std::string JsonExporter::exportTree(const ASTNode* root) {
    std::ostringstream oss;
    exportTree(root, oss);
    return oss.str();
}

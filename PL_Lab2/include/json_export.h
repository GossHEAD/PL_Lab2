#ifndef JSON_EXPORT_H
#define JSON_EXPORT_H

#include "ast.h"
#include <string>
#include <ostream>

class JsonExporter {
public:
    static std::string exportTree(const ASTNode* root);
    static void exportTree(const ASTNode* root, std::ostream& out);

private:
    static void visitNode(const ASTNode* node, std::ostream& out, int indent);
    static std::string escape(const std::string& s);
    static void writeIndent(std::ostream& out, int indent);
};

#endif

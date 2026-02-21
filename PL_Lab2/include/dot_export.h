#ifndef DOT_EXPORT_H
#define DOT_EXPORT_H

#include "ast.h"
#include <string>
#include <ostream>

class DotExporter {
public:
    static std::string exportTree(const ASTNode* root);
    static void exportTree(const ASTNode* root, std::ostream& out);

private:
    static void visitNode(const ASTNode* node, int& nextId, int parentId, std::ostream& out);
    static std::string escape(const std::string& s);
};

#endif

#ifndef CFG_DOT_EXPORT_H
#define CFG_DOT_EXPORT_H

#include "cfg.h"
#include <string>
#include <ostream>

class CFGDotExporter {
public:
    static void exportCFG(const FunctionInfo& func, std::ostream& out);

    static void exportCallGraph(const ProgramInfo& program, std::ostream& out);

private:
    static std::string escape(const std::string& s);
    static std::string operationToString(const Operation* op, int depth = 0);
    static std::string blockLabel(const BasicBlock& block);
};

#endif
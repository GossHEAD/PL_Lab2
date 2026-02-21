#include "../include/lexer.h"
#include "parser.h"
#include "cfg.h"
#include "../include/cfg_builder.h"
#include "../include/cfg_dot_export.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include "../include/parser.h"

static const size_t READ_BLOCK_SIZE = 4096;

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        throw std::runtime_error("Cannot open input file: " + path);
    }
    std::string content;
    char buffer[READ_BLOCK_SIZE];
    while (in.read(buffer, READ_BLOCK_SIZE)) {
        content.append(buffer, in.gcount());
    }
    content.append(buffer, in.gcount());
    return content;
}

static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::out | std::ios::binary);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open output file: " + path);
    }
    size_t written = 0;
    while (written < content.size()) {
        size_t chunk = std::min(READ_BLOCK_SIZE, content.size() - written);
        out.write(content.data() + written, static_cast<std::streamsize>(chunk));
        written += chunk;
    }
}

static std::string dirName(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

static std::string baseName(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    std::string name = (pos == std::string::npos) ? path : path.substr(pos + 1);
    auto dot = name.find_last_of('.');
    if (dot != std::string::npos) {
        name = name.substr(0, dot);
    }
    return name;
}

static void printUsage(const char* progName) {
    std::cerr << "Usage: " << progName << " [options] <input-file1> [input-file2 ...]\n"
        << "\n"
        << "Options:\n"
        << "  -o <dir>      Output directory (default: same as input file)\n"
        << "  --help, -h    Show this help\n"
        << "\n"
        << "Parses source files (Variant 4), builds control flow graphs,\n"
        << "and outputs DOT files per function + a call graph.\n";
}

int main(int argc, char* argv[]) {
    std::string outputDir;
    std::vector<std::string> inputFiles;

    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            outputDir = argv[i + 1];
            i += 2;
        }
        else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
        else {
            inputFiles.push_back(arg);
            i++;
        }
    }

    if (inputFiles.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    bool hasErrors = false;

    struct ParsedFile {
        std::string fileName;
        ASTNodePtr tree;
    };
    std::vector<ParsedFile> parsedFiles;

    for (const auto& inputPath : inputFiles) {
        std::string source;
        try {
            source = readFile(inputPath);
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            hasErrors = true;
            continue;
        }

        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();

        for (const auto& err : lexer.errors()) {
            std::cerr << inputPath << ":" << err.loc.line << ":" << err.loc.column
                << ": lexer error: " << err.message << "\n";
            hasErrors = true;
        }

        Parser parser(tokens);
        ParseResult result = parser.parse();

        for (const auto& err : result.errors) {
            std::cerr << inputPath << ":" << err.loc.line << ":" << err.loc.column
                << ": parse error: " << err.message << "\n";
            hasErrors = true;
        }

        if (result.tree) {
            parsedFiles.push_back({ inputPath, std::move(result.tree) });
        }
    }

    if (parsedFiles.empty()) {
        std::cerr << "No files were successfully parsed.\n";
        return 1;
    }

    std::vector<SourceFileInput> cfgInputs;
    cfgInputs.reserve(parsedFiles.size());
    for (const auto& pf : parsedFiles) {
        cfgInputs.push_back({ pf.fileName, pf.tree.get() });
    }

    CFGBuilder builder;
    CFGBuildResult cfgResult = builder.build(cfgInputs);

    for (const auto& err : cfgResult.errors) {
        std::cerr << err.sourceFile << ":" << err.loc.line << ":" << err.loc.column
            << ": analysis error: " << err.message << "\n";
        hasErrors = true;
    }

    if (!cfgResult.program) {
        std::cerr << "CFG construction failed.\n";
        return 1;
    }

    const ProgramInfo& program = *cfgResult.program;

    std::string mainFileDir;

    for (const auto& func : program.functions) {
        std::string srcBase = baseName(func->sourceFile);
        std::string outDir = outputDir.empty() ? dirName(func->sourceFile) : outputDir;

        std::string outPath = outDir + "/" + srcBase + "." + func->signature.name + ".dot";

        std::ostringstream oss;
        CFGDotExporter::exportCFG(*func, oss);

        try {
            writeFile(outPath, oss.str());
            std::cout << "CFG: " << outPath << "\n";
        }
        catch (const std::exception& e) {
            std::cerr << "Error writing " << outPath << ": " << e.what() << "\n";
            hasErrors = true;
        }

        if (func->signature.name == "main") {
            mainFileDir = outDir;
        }
    }

    if (mainFileDir.empty() && !program.functions.empty()) {
        mainFileDir = outputDir.empty()
            ? dirName(program.functions[0]->sourceFile)
            : outputDir;
    }

    if (!mainFileDir.empty()) {
        std::string callGraphPath = mainFileDir + "/callgraph.dot";
        std::ostringstream oss;
        CFGDotExporter::exportCallGraph(program, oss);

        try {
            writeFile(callGraphPath, oss.str());
            std::cout << "Call graph: " << callGraphPath << "\n";
        }
        catch (const std::exception& e) {
            std::cerr << "Error writing " << callGraphPath << ": " << e.what() << "\n";
            hasErrors = true;
        }
    }

    return hasErrors ? 1 : 0;
}
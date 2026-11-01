#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "semantics/type_checker.hpp"
#include "codegen/codegen.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>

using namespace mcxx;

void printUsage(const char* progName) {
    std::cout << "Micro C++ Compiler (mcxx) v1.0\n";
    std::cout << "Usage: " << progName << " [options] <input_file.cpp>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o <file>        Specify output executable name (default: a.exe)\n";
    std::cout << "  -S               Generate assembly (.s) file only; do not link\n";
    std::cout << "  -run             Compile, link, and run immediately\n";
    std::cout << "  --dump-tokens    Print lexical tokens and exit\n";
    std::cout << "  --dump-ast       Print Abstract Syntax Tree\n";
    std::cout << "  -h, --help       Show this help message\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string inputFile;
    std::string outputFile;
    bool onlyAssemble = false;
    bool runImmediately = false;
    bool dumpTokens = false;
    bool dumpAst = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                outputFile = argv[++i];
            } else {
                std::cerr << "error: missing argument for -o\n";
                return 1;
            }
        } else if (arg == "-S") {
            onlyAssemble = true;
        } else if (arg == "-run") {
            runImmediately = true;
        } else if (arg == "--dump-tokens") {
            dumpTokens = true;
        } else if (arg == "--dump-ast") {
            dumpAst = true;
        } else if (arg.empty() || arg[0] == '-') {
            std::cerr << "error: unrecognized command line option '" << arg << "'\n";
            return 1;
        } else {
            inputFile = arg;
        }
    }

    if (inputFile.empty()) {
        std::cerr << "error: no input file specified\n";
        return 1;
    }

    std::ifstream file(inputFile);
    if (!file.is_open()) {
        std::cerr << "error: could not open file '" << inputFile << "'\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sourceCode = buffer.str();

    // 1. Lexical Analysis
    Lexer lexer(sourceCode, inputFile);
    std::vector<Token> tokens = lexer.tokenize();

    if (dumpTokens) {
        std::cout << "=== TOKENS (" << inputFile << ") ===\n";
        for (const auto& tok : tokens) {
            std::cout << tok.toString() << "\n";
        }
        return 0;
    }

    // 2. Syntax Analysis (Parsing)
    Parser parser(tokens);
    std::unique_ptr<Program> program;
    try {
        program = parser.parseProgram();
    } catch (const std::exception& e) {
        std::cerr << "error during parsing: " << e.what() << "\n";
        return 1;
    }

    if (!program) {
        std::cerr << "error: failed to generate AST\n";
        return 1;
    }

    if (dumpAst) {
        std::cout << "=== ABSTRACT SYNTAX TREE (" << inputFile << ") ===\n";
        program->dump(0);
    }

    // 3. Semantic Analysis & Type Checking
    TypeChecker typeChecker;
    if (!typeChecker.analyze(*program)) {
        std::cerr << "compilation terminated with " << typeChecker.getErrorsCount() << " semantic errors.\n";
        return 1;
    }

    // 4. Code Generation
    CodeGenerator codegen(typeChecker.getSymbolTable());
    std::string asmCode = codegen.generate(*program);

    if (outputFile.empty()) {
        if (onlyAssemble) {
            outputFile = "output.s";
        } else {
            outputFile = "a.exe";
        }
    }

    if (onlyAssemble) {
        std::ofstream asmFile(outputFile);
        if (!asmFile.is_open()) {
            std::cerr << "error: could not open output file '" << outputFile << "' for writing\n";
            return 1;
        }
        asmFile << asmCode;
        std::cout << "[mcxx] Generated assembly: " << outputFile << "\n";
        return 0;
    }

    // Write temporary assembly file
    std::string tempAsmFile = outputFile + ".s";
    {
        std::ofstream asmFile(tempAsmFile);
        if (!asmFile.is_open()) {
            std::cerr << "error: could not create temporary assembly file '" << tempAsmFile << "'\n";
            return 1;
        }
        asmFile << asmCode;
    }

    // 5. Invoke GCC to assemble and link
    std::string gccCmd = "gcc -m32 \"" + tempAsmFile + "\" -o \"" + outputFile + "\"";
    int ret = std::system(gccCmd.c_str());
    if (ret != 0) {
        std::cerr << "error: gcc assembler/linker failed with code " << ret << "\n";
        return 1;
    }

    // Remove temp file
    std::remove(tempAsmFile.c_str());
    std::cout << "[mcxx] Successfully built: " << outputFile << "\n";

    // 6. Run if requested
    if (runImmediately) {
        std::cout << "\n--- Running " << outputFile << " ---\n";
        int runRet = std::system(outputFile.c_str());
        std::cout << "\n--- Program exited with code " << runRet << " ---\n";
    }

    return 0;
}

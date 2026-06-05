#include <iostream>
#include <cstdlib>
#include <fstream>

using namespace std;

bool fileExists(const string& filename) {
    ifstream file(filename);
    return file.is_open();
}

void cleanOldFiles() {
    cout << "Cleaning old generated files...\n";
    
    const char* files[] = {
        "test_cleaned.txt",
        "tokens.txt",
        "ast_output.txt",
        "triads.txt"
    };
    
    for (const char* file : files) {
        if (remove(file) == 0) {
            cout << "  Removed: " << file << "\n";
        }
    }
    cout << "Cleanup completed.\n\n";
}

// Функция для выполнения внешней программы
int runModule(const string& moduleName, const string& exeName) {
    cout << "\n\nRunning: " << moduleName << "\n";
    string cmd = exeName + ".exe";
    int result = system(cmd.c_str());
    
    if (result != 0) {
        cerr << "Error: " << moduleName << " failed with code " << result << endl;
        return result;
    }
    
    cout << moduleName << " completed successfully.\n\n";
    return 0;
}

int main() {
    cout << "FULL COMPILER (LR1-LR4)\n";
    cout << "Starting compilation process...\n";
    cleanOldFiles();
    // Проверка входного файла
    if (!fileExists("test.cpp")) {
        cerr << "Error: Input file 'test.cpp' not found!\n";
        return 1;
    }
    
    // Препроцессов
    if (runModule("Preprocessor (LR1)", "preprocessor") != 0) {
        cerr << "Compilation terminated at preprocessing stage.\n";
        return 1;
    }
    
    // Проверка выходного файла препроцессора
    if (!fileExists("test_cleaned.txt")) {
        cerr << "Error: Preprocessor did not create 'test_cleaned.txt'\n";
        return 1;
    }
    
    // Лексический анализатор
    if (runModule("Lexical Analyzer (LR2)", "lexicalAnalyzer") != 0) {
        cerr << "Compilation terminated at lexical analysis stage.\n";
        return 1;
    }
    
    // Проверка выходного файла лексического анализатора
    if (!fileExists("tokens.txt")) {
        cerr << "Error: Lexical analyzer did not create 'tokens.txt'\n";
        return 1;
    }
    
    // Синтаксический анализатор
    if (runModule("Syntax Analyzer (LR3)", "syntaxAnalyzer") != 0) {
        cerr << "Compilation terminated at syntax analysis stage.\n";
        return 1;
    }
    
    // Проверка выходного файла синтаксического анализатора
    if (!fileExists("ast_output.txt")) {
        cerr << "Error: Syntax analyzer did not create 'ast_output.txt'\n";
        return 1;
    }
    
    // Семантический анализатор
    if (runModule("Semantic Analyzer (LR4)", "semanticAnalyzer") != 0) {
        cerr << "Compilation terminated at semantic analysis stage.\n";
        return 1;
    }
    
    // Проверка выходного файла семантического анализатора
    if (fileExists("triads.txt")) {
        cout << "\nTriads saved to 'triads.txt'\n";
    }
    
    cout << "\n\nCOMPILATION COMPLETED SUCCESSFULLY";
    cout << "\nGenerated files:\n";
    cout << "  - test_cleaned.txt\n";
    cout << "  - tokens.txt\n";
    cout << "  - ast_output.txt\n";
    cout << "  - triads.txt\n";
    
    return 0;
}
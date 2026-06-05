#include "Preprocessor.h"
#include <iostream>
#include <regex>
#include <sstream>
#include <cctype>
#include <fstream>
using namespace std;

void Preprocessor::addError(const string& type, const string& msg, int line) {
    errors.push_back({type, msg, line});
}

vector<Preprocessor::PreprocessError> Preprocessor::checkErrors(const string& code) {
    errors.clear();
    
    vector<string> lines;
    stringstream ss(code);
    string line;
    while (getline(ss, line)) {
        lines.push_back(line);
    }
    
    bool inMultiComment = false;
    int commentStartLine = 0;
    int commentDepth = 0;
    
    for (size_t i = 0; i < lines.size(); i++) {
        const string& line = lines[i];
        int lineNum = i + 1;
        int len = line.length();
        bool inSingleComment = false;
        
        for (int j = 0; j < len; j++) {
            // Однострочный комментарий
            if (!inMultiComment && !inSingleComment && j + 1 < len && 
                line[j] == '/' && line[j + 1] == '/') {
                inSingleComment = true;
                j++;
                continue;
            }
            
            // Открытие многострочного комментария
            if (!inSingleComment && j + 1 < len && 
                line[j] == '/' && line[j + 1] == '*') {
                if (!inMultiComment) {
                    commentStartLine = lineNum;
                }
                inMultiComment = true;
                commentDepth++;
                
                if (commentDepth > 1) {
                    addError("NESTED_COMMENT", 
                        "Nested comment detected at line " + to_string(lineNum) + 
                        ", depth " + to_string(commentDepth), lineNum);
                }
                j++;
                continue;
            }
            
            // Закрытие многострочного комментария
            if (j + 1 < len && line[j] == '*' && line[j + 1] == '/') {
                if (inMultiComment) {
                    commentDepth--;
                    if (commentDepth == 0) {
                        inMultiComment = false;
                    }
                } else if (!inSingleComment) {
                    addError("UNMATCHED_COMMENT", 
                        "*/ without opening /* at line " + to_string(lineNum), lineNum);
                }
                j++;
                continue;
            }
        }
    }
    
    if (inMultiComment) {
        addError("UNCLOSED_COMMENT", 
            "Unclosed multi-line comment started at line " + to_string(commentStartLine), 
            commentStartLine);
    }
    
    return errors;
}

string Preprocessor::cleanCode(const string& code) {
    // 1. Удаляем многострочные комментарии
    regex multiline(R"(/\*[\s\S]*?\*/)");
    string withoutMultiline = regex_replace(code, multiline, "");
    
    // 2. Удаляем однострочные комментарии
    regex singleline(R"(//[^\n]*)");
    string withoutComments = regex_replace(withoutMultiline, singleline, "");
    
    // 3. Удаляем пробелы в начале и конце строк, заменяем табуляции
    regex tabs(R"(\t)");
    regex leadingSpaces(R"(^[ \t]+)");
    regex trailingSpaces(R"([ \t]+$)");
    
    vector<string> lines;
    stringstream ss(withoutComments);
    string line;
    
    while (getline(ss, line)) {
        line = regex_replace(line, tabs, "    ");
        line = regex_replace(line, leadingSpaces, "");
        line = regex_replace(line, trailingSpaces, "");
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    
    // 4. Объединяем строки
    string result;
    for (size_t i = 0; i < lines.size(); i++) {
        result += lines[i];
        if (i < lines.size() - 1) {
            result += "\n";
        }
    }
    
    // 5. Сжимаем множественные пробелы
    regex multipleSpaces(R"( +)");
    result = regex_replace(result, multipleSpaces, " ");
    
    return result;
}

void Preprocessor::printErrors() const {
    if (errors.empty()) {
        cout << "No preprocessing errors found.\n";
    } else {
        cout << "\n=== PREPROCESSING ERRORS ===\n";
        for (const auto& e : errors) {
            cout << "[" << e.type << "] Line " << e.lineNum << ": " << e.message << "\n";
        }
        cout << "Total errors: " << errors.size() << "\n";
    }
}
int main() {
    string inputFile = "test.cpp";
    string outputFile = "test_cleaned.txt";
    
    // Проверка существования файла
    ifstream testFile(inputFile);
    if (!testFile.is_open()) {
        cerr << "Error: Cannot open " << inputFile << endl;
        return 1;
    }
    
    // Чтение файла
    string code((istreambuf_iterator<char>(testFile)), istreambuf_iterator<char>());
    testFile.close();
    
    // Проверка ошибок препроцессинга
    Preprocessor preproc;
    auto errors = preproc.checkErrors(code);
    
    if (!errors.empty()) {
        preproc.printErrors();
        cout << "\nCleaning will continue, but result may be incorrect.\n";
    } else {
        cout << "No preprocessing errors found.\n";
    }
    
    // Очистка кода
    string cleaned = preproc.cleanCode(code);
    
    // Сохранение результата
    ofstream outFile(outputFile);
    outFile << cleaned;
    outFile.close();
    
    cout << "Preprocessing completed. Output saved to " << outputFile << endl;
    
    cout << "\n=== PREVIEW ===\n";
    cout << cleaned << endl;
    
    return 0;
}
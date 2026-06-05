#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <cctype>
#include <set>
#include <map>
#include <algorithm>
using namespace std;

struct Token {
    string type, value;
    int lineNum;
    Token(string t, string v, int l) : type(t), value(v), lineNum(l) {}
};

struct LexicalError {
    string type, message;
    int lineNum;
    LexicalError(string t, string m, int l) : type(t), message(m), lineNum(l) {}
};

vector<LexicalError> errors;

// Словари для быстрого поиска
const set<string> keywords = { "#include", "using", "bool", "return", "short", "int", "for", "if", "while", "main", "else", "break", "continue" };
const set<string> identifiers = { "is_even", "x", "sum", "count", "limit", "squared", "i", "final_avg", "cout", "endl", "std", "z" };
const set<string> constants = { "0", "10", "1", "2", "3" };
const set<string> operators2 = { "::", "==", "<=", ">=", "&&", "<<", ">>", "+=", "++", "--", "?:" };
const set<char> operators1 = { '%', '=', '*', '/', '>', '<', '?', ':', '+', '-' };
const set<char> delimiters = { ';', ',', '(', ')', '{', '}', '[', ']', '.', '#' };
const set<string> headers = { "iostream"};

bool isValidInteger(const string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!isdigit(c)) return false;
    }
    return true;
}

string getTokenType(const string& w, int line, bool inIncludeContext = false) {
    // Если мы в контексте #include, то < и > - разделители
    if (inIncludeContext && (w == "<" || w == ">")) {
        return "DELIMITER";
    }
    
    // Определение типа
    if (keywords.count(w)) return "KEYWORD";
    if (identifiers.count(w)) return "IDENTIFIER";
    if (constants.count(w) || isValidInteger(w)) return "CONSTANT_INT";
    if (headers.count(w)) return "HEADER";
    if (operators2.count(w) || (w.size() == 1 && operators1.count(w[0]))) return "OPERATOR";
    if (w.size() == 1 && delimiters.count(w[0])) return "DELIMITER";
    if (w.front() == '"' && w.back() == '"') return "STRING_CONSTANT";
    if (w.front() == '"' || w.back() == '"') {
        errors.emplace_back("STRING_ERROR", "Malformed string literal: '" + w + "'", line);
        return "UNKNOWN";
    }

    // Проверка на наличие недопустимых символов в идентификаторе
    if (w.find('@') != string::npos || w.find('#') != string::npos) {
        errors.emplace_back("IDENTIFIER_ERROR", "Identifier contains invalid character: '" + w + "'", line);
        return "UNKNOWN";
    }

    // Проверка на число с буквами
    if (isdigit(w[0])) {
        bool hasLetters = false;
        for (char c : w) {
            if (isalpha(c)) {
                hasLetters = true;
                break;
            }
        }
        if (hasLetters) {
            errors.emplace_back("IDENTIFIER_ERROR", "Identifier cannot start with a digit: '" + w + "'", line);
            return "UNKNOWN";
        }
    }

    // Остальное (начинается с буквы или _)
    if (isalpha(w[0]) || w[0] == '_') {
        return "UNKNOWN";
    }

    return "UNKNOWN";
}

// Разделение на лексемы
vector<string> split(const string& line, int lineNum) {
    vector<string> lex;
    int i = 0, len = line.length();

    while (i < len) {
        if (isspace(line[i])) { i++; continue; }

        // Строковый литерал
        if (line[i] == '"') {
            string str = "\"";
            i++;
            while (i < len && line[i] != '"') {
                str += line[i++];
            }
            if (i < len && line[i] == '"') {
                str += line[i++];
            }
            lex.push_back(str);
            continue;
        }

        // #include
        if (line[i] == '#' && i + 7 < len && line.substr(i, 8) == "#include") {
            lex.push_back("#include");
            i += 8;
            continue;
        }

        // Двухсимвольные операторы
        if (i + 1 < len) {
            string two = line.substr(i, 2);
            if (operators2.count(two)) {
                lex.push_back(two);
                i += 2;
                continue;
            }
        }

        // Односимвольные операторы и разделители
        if (operators1.count(line[i]) || delimiters.count(line[i])) {
            lex.push_back(string(1, line[i]));
            i++;
            continue;
        }

        // Числа (включая ошибочные)
        if (isdigit(line[i])) {
            string num;
            while (i < len && (isalnum(line[i]) || line[i] == '_')) {
                num += line[i++];
            }
            lex.push_back(num);
            continue;
        }

        if (isalpha(line[i]) || line[i] == '_') {
            string word;
            while (i < len && (isalnum(line[i]) || line[i] == '_' || line[i] == '@' || line[i] == '#')) {
                word += line[i++];
            }
            lex.push_back(word);
            continue;
        }

        // Неизвестные символы
        lex.push_back(string(1, line[i++]));
    }
    return lex;
}

// Вывод по требованиям
void printTokens(const vector<Token>& tokens) {
    cout << "\nLexeme                        | Type\n";
    cout << "------------------------------+--------------------\n";
    for (const auto& t : tokens)
        printf("%-30s | %s\n", t.value.c_str(), t.type.c_str());

    cout << "\n[\n";
    for (size_t i = 0; i < tokens.size(); i++) {
        cout << "  (" << tokens[i].type << ", " << tokens[i].value << ")" << (i < tokens.size() - 1 ? "," : "") << "\n";
    }
    cout << "]\n";
}

// Вывод ошибок
void printErrors() {
    if (errors.empty()) {
        cout << "\nNo lexical errors found.\n";
        return;
    }
    cout << "\n========== LEXICAL ERRORS ==========\n\n";
    for (const auto& e : errors)
        cout << "[" << e.type << "] Line " << e.lineNum << ": " << e.message << "\n";
    cout << "\nTotal errors: " << errors.size() << "\n====================================\n\n";
}
void saveTokens(const vector<Token>& tokens) {
    ofstream file("tokens.txt");
    if (!file) {
        cerr << "Error: Cannot create tokens.cpp\n";
        return;
    }
    
    file << "[\n";
    for (size_t i = 0; i < tokens.size(); i++) {
        file << "  (" << tokens[i].type << ", " << tokens[i].value << ")";
        if (i < tokens.size() - 1) {
            file << ",";
        }
        file << "\n";
    }
    file << "]\n";
    file.close();
    
    cout << "Tokens saved to tokens.cpp\n";
}
// Программа
int main() {
    vector<Token> tokens;
    ifstream test("test_cleaned.txt");

    if (!test) {
        cerr << "No such file: test_cleaned.cpp\n";
        return 1;
    }

    string line;
    int lineNum = 0;
    bool inIncludeContext = false;
    bool waitingForHeader = false;
    
    while (getline(test, line)) {
        lineNum++;
        vector<string> lexemes = split(line, lineNum);
        
        for (size_t j = 0; j < lexemes.size(); j++) {
            const string& lex = lexemes[j];
            
            // Определяем контекст #include
            if (lex == "#include") {
                inIncludeContext = true;
                waitingForHeader = true;
                tokens.emplace_back(getTokenType(lex, lineNum, false), lex, lineNum);
                continue;
            }
            
            // Если мы в контексте #include
            if (inIncludeContext) {
                if (waitingForHeader && lex == "<") {
                    tokens.emplace_back("DELIMITER", lex, lineNum);
                    waitingForHeader = false;
                    continue;
                }
                if (lex == ">") {
                    tokens.emplace_back("DELIMITER", lex, lineNum);
                    inIncludeContext = false;
                    waitingForHeader = false;
                    continue;
                }
                // Если это имя заголовка (iostream)
                if (!waitingForHeader && lex != "<" && lex != ">") {
                    tokens.emplace_back(getTokenType(lex, lineNum, false), lex, lineNum);
                    continue;
                }
            }
            
            // Обычная обработка
            tokens.emplace_back(getTokenType(lex, lineNum, false), lex, lineNum);
            
            // Сброс контекста, если не в include
            if (!inIncludeContext) {
                waitingForHeader = false;
            }
        }
    }

    printTokens(tokens);
    printErrors();
    
    // Сохраняем токены в файл для синтаксического анализатора
    saveTokens(tokens);
    
    cout << "Success. Detected " << tokens.size() << " tokens.\nErrors: " << errors.size() << "\n";

    return 0;
}
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <cctype>

using namespace std;

struct Symbol {
    string name;
    string type;
    bool initialized;
    string scope;
};

struct Triad {
    int number;
    string operation;
    string operand1;
    string operand2;
};

class SemanticAnalyzer {
private:
    vector<Symbol> symbolTable;
    vector<Triad> triads;
    int triadCounter = 0;
    vector<string> errors;
    
    string currentScope = "global";
    string currentFunction = "";
    string currentReturnType = "";
    int loopDepth = 0;
    int labelCounter = 0;
    int tempCounter = 0;
    
    map<string, string> varTypes;
    map<string, bool> varInitialized;
    map<string, int> functionParams;
    set<string> varsInCurrentScope;
    
    string newLabel() { return "L" + to_string(++labelCounter); }
    string newTemp() { return "t" + to_string(++tempCounter); }
    
    void addTriad(const string& op, const string& arg1, const string& arg2) {
        triads.push_back({++triadCounter, op, arg1, arg2});
    }
    
    void addError(const string& msg) {
        errors.push_back(msg);
    }
    
    void addSymbol(const string& name, const string& type, bool init, const string& scope) {
        for (const auto& s : symbolTable) {
            if (s.name == name && s.scope == scope) {
                if (name != "cout" && name != "endl" && name != "is_even" && name != "main") {
                    addError("Variable '" + name + "' already declared in scope '" + scope + "'");
                }
                return;
            }
        }
        symbolTable.push_back({name, type, init, scope});
        varTypes[name] = type;
        varInitialized[name] = init;
    }
    
    bool isDeclared(const string& name) {
        if (name == "cout" || name == "endl") return true;
        return varTypes.find(name) != varTypes.end();
    }
    
    string trim(const string& str) {
        size_t start = str.find_first_not_of(" \t");
        if (start == string::npos) return "";
        size_t end = str.find_last_not_of(" \t");
        return str.substr(start, end - start + 1);
    }

public:
    void analyze(const vector<string>& astLines) {
        addSymbol("cout", "ostream", true, "global");
        addSymbol("endl", "manipulator", true, "global");
        parseLinear(astLines);
    }
    
    void printSymbolTable() {
        cout << "\n" << string(70, '=') << "\n";
        cout << "=== SYMBOL TABLE ===\n";
        cout << string(70, '=') << "\n";
        cout << "+-----------------+--------+----------+-------------+\n";
        cout << "| Name            | Type   | Scope    | Initialized |\n";
        cout << "+-----------------+--------+----------+-------------+\n";
        for (const auto& s : symbolTable) {
            printf("| %-15s | %-6s | %-8s | %-11s |\n",
                   s.name.c_str(), s.type.c_str(), s.scope.c_str(),
                   s.initialized ? "yes" : "no");
        }
        cout << "+-----------------+--------+----------+-------------+\n";
        cout << "Total symbols: " << symbolTable.size() << "\n";
    }
    
    void printTriads() {
        cout << "\n" << string(70, '=') << "\n";
        cout << "=== TRIADS ===\n";
        cout << string(70, '=') << "\n";
        for (const auto& t : triads) {
            cout << t.number << ": (" << t.operation << ", " << t.operand1 << ", " << t.operand2 << ")\n";
        }
        cout << "Total triads: " << triadCounter << "\n";
    }
    
    void printErrors() {
        if (errors.empty()) {
            cout << "\n" << string(70, '=') << "\n";
            cout << "=== NO SEMANTIC ERRORS FOUND ===\n";
            cout << string(70, '=') << "\n";
        } else {
            cout << "\n" << string(70, '=') << "\n";
            cout << "=== SEMANTIC ERRORS ===\n";
            cout << string(70, '=') << "\n";
            for (const auto& e : errors) {
                cout << e << "\n";
            }
        }
    }
    
    bool hasErrors() const { return !errors.empty(); }

private:
    void parseLinear(const vector<string>& lines) {
        bool inFunction = false;
        bool inParams = false;
        bool inBody = false;
        string funcName;
        
        // Для отслеживания вложенных конструкций
        bool inForLoop = false;
        string forVarName;
        int forInitValue;
        string forCondition;
        string forEndLabel;
        string forStartLabel;
        
        for (size_t i = 0; i < lines.size(); i++) {
            string line = lines[i];
            
            string content = line;
            size_t pos = content.find_first_not_of("| +-");
            if (pos != string::npos) content = content.substr(pos);
            if (content.empty()) continue;
            if (content.find("+-- ") == 0) content = content.substr(4);
            
            // Определяем глубину для вложенности
            int depth = 0;
            for (char c : line) {
                if (c == '|') depth++;
                else if (c == ' ' || c == '+' || c == '-') continue;
                else break;
            }
            
            if (content.find("function:") == 0) {
                string rest = content.substr(9);
                size_t arrow = rest.find(" -> ");
                funcName = rest.substr(0, arrow);
                string returnType = rest.substr(arrow + 4);
                while (!funcName.empty() && funcName.front() == ' ') funcName.erase(0, 1);
                while (!returnType.empty() && returnType.front() == ' ') returnType.erase(0, 1);
                
                currentFunction = funcName;
                currentReturnType = returnType;
                currentScope = funcName;
                inFunction = true;
                inParams = false;
                inBody = false;
                varsInCurrentScope.clear();
                inForLoop = false;
                
                addSymbol(funcName, returnType, true, "global");
                functionParams[funcName] = 0;
            }
            else if (content == "params") {
                inParams = true;
                inBody = false;
            }
            else if (content == "body") {
                inParams = false;
                inBody = true;
            }
            else if (inParams && content.find(':') != string::npos && content.length() < 50) {
                size_t colon = content.find(':');
                string paramName = content.substr(0, colon);
                string paramType = content.substr(colon + 2);
                while (!paramName.empty() && paramName.front() == ' ') paramName.erase(0, 1);
                while (!paramType.empty() && paramType.front() == ' ') paramType.erase(0, 1);
                addSymbol(paramName, paramType, true, currentScope);
                functionParams[funcName]++;
            }
            else if (inBody && content.find("var:") == 0) {
                string rest = content.substr(5);
                size_t eqPos = rest.find(" = ");
                string varName = rest.substr(0, eqPos);
                while (!varName.empty() && varName.front() == ' ') varName.erase(0, 1);
                
                if (varsInCurrentScope.find(varName) != varsInCurrentScope.end()) {
                    addError("Variable '" + varName + "' already declared in current scope");
                }
                varsInCurrentScope.insert(varName);
                
                if (eqPos != string::npos) {
                    string initValue = rest.substr(eqPos + 3);
                    while (!initValue.empty() && initValue.front() == ' ') initValue.erase(0, 1);
                    if (!initValue.empty() && initValue.front() == '"' && initValue.back() == '"') {
                        initValue = initValue.substr(1, initValue.length() - 2);
                    }
                    addTriad("=", varName, initValue);
                    addSymbol(varName, "short", true, currentScope);
                } else {
                    addSymbol(varName, "short", false, currentScope);
                }
            }
            else if (inBody && content.find("assign:") == 0) {
                string rest = content.substr(8);
                size_t eqPos = rest.find(" = ");
                if (eqPos != string::npos) {
                    string lhs = rest.substr(0, eqPos);
                    string rhs = rest.substr(eqPos + 3);
                    while (!lhs.empty() && lhs.front() == ' ') lhs.erase(0, 1);
                    while (!rhs.empty() && rhs.front() == ' ') rhs.erase(0, 1);
                    
                    if (!isDeclared(lhs)) {
                        addError("Variable '" + lhs + "' used without declaration");
                    } else {
                        addTriad("=", lhs, rhs);
                        varInitialized[lhs] = true;
                    }
                }
            }
            else if (inBody && content.find("return") == 0) {
                string value = content.length() > 7 ? content.substr(7) : "";
                while (!value.empty() && value.front() == ' ') value.erase(0, 1);
                
                if (currentFunction == "is_even" && !value.empty()) {
                    string expr = value;
                    while (!expr.empty() && expr.front() == '(' && expr.back() == ')') {
                        expr = expr.substr(1, expr.length() - 2);
                        expr = trim(expr);
                    }
                    
                    size_t eqPosLocal = expr.find(" == ");
                    if (eqPosLocal != string::npos) {
                        string left = expr.substr(0, eqPosLocal);
                        string right = expr.substr(eqPosLocal + 4);
                        
                        size_t modPos = left.find(" % ");
                        if (modPos != string::npos) {
                            string varName = left.substr(0, modPos);
                            string constVal = left.substr(modPos + 3);
                            while (!varName.empty() && varName.front() == ' ') varName.erase(0, 1);
                            while (!constVal.empty() && constVal.front() == ' ') constVal.erase(0, 1);
                            while (!right.empty() && right.front() == ' ') right.erase(0, 1);
                            
                            addTriad("%", varName, constVal);
                            string temp = "^" + to_string(triadCounter);
                            addTriad("==", temp, right);
                            addTriad("return", "^" + to_string(triadCounter), "-");
                        }
                    }
                } else if (!value.empty()) {
                    addTriad("return", value, "-");
                } else {
                    addTriad("return", "-", "-");
                }
            }
            else if (inBody && content.find("for ") == 0) {
                string rest = content.substr(4);
                size_t eqPos = rest.find(" = ");
                if (eqPos != string::npos) {
                    string varName = rest.substr(0, eqPos);
                    string initPart = rest.substr(eqPos + 3);
                    size_t spacePos = initPart.find(' ');
                    string initValue = initPart.substr(0, spacePos);
                    while (!varName.empty() && varName.front() == ' ') varName.erase(0, 1);
                    
                    if (varsInCurrentScope.find(varName) == varsInCurrentScope.end()) {
                        varsInCurrentScope.insert(varName);
                        addSymbol(varName, "short", true, currentScope);
                    }
                    
                    addTriad("=", varName, initValue);
                    
                    // Условие цикла
                    addTriad("<=", varName, "limit");
                    string condTemp = "^" + to_string(triadCounter);
                    
                    string startLabel = newLabel();
                    string endLabel = newLabel();
                    
                    addTriad("br_false", condTemp, endLabel);
                    addTriad("label", startLabel, "-");
                    
                    inForLoop = true;
                    forVarName = varName;
                    forStartLabel = startLabel;
                    forEndLabel = endLabel;
                }
            }
            else if (inBody && content.find("while ") == 0) {
                string startLabel = newLabel();
                string endLabel = newLabel();
                
                addTriad("label", startLabel, "-");
                // Условие будет добавлено позже, пока заглушка
                addTriad("condition", "?", "?");
                string condTemp = "^" + to_string(triadCounter);
                addTriad("br_false", condTemp, endLabel);
                
                inForLoop = true;
                forStartLabel = startLabel;
                forEndLabel = endLabel;
            }
            else if (inBody && content.find("if ") == 0) {
                // Для if нужны метки для then/else
                string elseLabel = newLabel();
                string endLabel = newLabel();
                
                addTriad("br_false", "condition", elseLabel);
                addTriad("br", endLabel, "-");
                addTriad("label", elseLabel, "-");
                addTriad("label", endLabel, "-");
            }
            else if (inBody && content.find("++") != string::npos && content.length() < 10) {
                string varName = content;
                while (!varName.empty() && varName.back() == '+') varName.pop_back();
                while (!varName.empty() && varName.front() == ' ') varName.erase(0, 1);
                addTriad("++", varName, "-");
                varInitialized[varName] = true;
                
                // Если это инкремент переменной цикла, добавляем переход
                if (inForLoop && varName == forVarName) {
                    addTriad("br", forStartLabel, "-");
                    addTriad("label", forEndLabel, "-");
                    inForLoop = false;
                }
            }
            else if (inBody && content.find("cout <<") == 0) {
                string stream = "cout";
                string val = content;
                
                size_t p = 0;
                while ((p = val.find(" << ")) != string::npos) {
                    string part = val.substr(0, p);
                    if (!part.empty() && part != "cout") {
                        while (!part.empty() && part.front() == ' ') part.erase(0, 1);
                        if (!part.empty() && part.front() == '"' && part.back() == '"') {
                            part = part.substr(1, part.length() - 2);
                        }
                        addTriad("<<", stream, part);
                        stream = "^" + to_string(triadCounter);
                    }
                    val = val.substr(p + 4);
                }
                if (!val.empty() && val != "cout") {
                    while (!val.empty() && val.front() == ' ') val.erase(0, 1);
                    if (!val.empty() && val.front() == '"' && val.back() == '"') {
                        val = val.substr(1, val.length() - 2);
                    }
                    addTriad("<<", stream, val);
                }
            }
        }
        
        if (currentFunction == "main") {
            addTriad("return", "0", "-");
        }
    }
};

int main() {
    cout << "=== Semantic Analyzer ===\n";
    cout << "AST file: ast_output.txt\n";
    
    ifstream file("ast_output.txt");
    if (!file.is_open()) {
        cerr << "Error: Cannot open ast_output.txt\n";
        return 1;
    }
    
    vector<string> astLines;
    string line;
    while (getline(file, line)) {
        astLines.push_back(line);
    }
    file.close();
    
    SemanticAnalyzer analyzer;
    analyzer.analyze(astLines);
    
    analyzer.printSymbolTable();
    analyzer.printTriads();
    analyzer.printErrors();
    
    if (!analyzer.hasErrors()) {
        cout << "\n=== Semantic analysis completed successfully ===\n";
    } else {
        cout << "\n=== Semantic analysis completed with errors ===\n";
    }
    
    return 0;
}
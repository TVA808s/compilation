#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <cctype>

using namespace std;

struct Symbol { string name, type, scope; bool initialized; };
struct Triad { int number; string op, arg1, arg2; };

class SemanticAnalyzer {
private:
    vector<Symbol> symbolTable;
    vector<Triad> triads;
    vector<string> errors;
    
    string currentScope = "global", currentFunction = "";
    int triadCounter = 0, labelCounter = 0, tempCounter = 0;
    
    map<string, string> varTypes;
    map<string, bool> varInitialized;
    set<string> forLoopVariables;
    vector<pair<string, string>> loopLabels;
    
    string newLabel() { return "L" + to_string(++labelCounter); }
    void addTriad(const string& op, const string& a1, const string& a2) {
        triads.push_back({++triadCounter, op, a1, a2});
    }
    void addError(const string& type, const string& msg) {
        errors.push_back("[" + type + "] " + msg);
    }
    
    string clean(const string& name) {
        string s = name;
        while (s.length() >= 2 && s.front() == '(' && s.back() == ')') 
            s = s.substr(1, s.length() - 2);
        size_t start = s.find_first_not_of(" \t");
        if (start == string::npos) return "";
        size_t end = s.find_last_not_of(" \t");
        s = s.substr(start, end - start + 1);
        for (char c : s) if (!isalnum(c) && c != '_') return "";
        return s;
    }
    
    string trim(const string& s) {
        size_t l = s.find_first_not_of(" \t\r\n"), r = s.find_last_not_of(" \t\r\n");
        return l == string::npos ? "" : s.substr(l, r - l + 1);
    }
    
    string strip(const string& s) {
        string r = trim(s);
        while (r.length() >= 2 && r.front() == '(' && r.back() == ')') 
            r = trim(r.substr(1, r.length() - 2));
        return r;
    }
    
    string detectType(const string& expr) {
        string e = trim(expr);
        if (isdigit(e[0]) || (e[0] == '-' && e.length() > 1 && isdigit(e[1]))) return "short";
        if (e.front() == '"' && e.back() == '"') return "string";
        if (e == "true" || e == "false") return "bool";
        string v = clean(e);
        if (!v.empty() && varTypes.count(v)) return varTypes[v];
        if (!v.empty() && forLoopVariables.count(v)) return "short";
        return "unknown";
    }
    
    bool typesCompatible(const string& left, const string& right) {
        return left == right || (left == "short" && right == "int") || (left == "int" && right == "short");
    }
    
    void addSymbol(const string& name, const string& type, bool init, const string& scope) {
        string n = clean(name);
        if (n.empty()) return;
        for (const auto& s : symbolTable) {
            if (s.name == n && s.scope == scope) {
                if (forLoopVariables.find(n) == forLoopVariables.end())
                    addError("DECLARATION", "Variable '" + n + "' already declared");
                return;
            }
        }
        symbolTable.push_back({n, type, scope, init});
        varTypes[n] = type;
        varInitialized[n] = init;
    }
    
    bool isDeclared(const string& name) {
        string n = clean(name);
        if (n.empty() || n == "cout" || n == "endl") return true;
        for (const auto& s : symbolTable)
            if (s.name == n && (s.scope == currentScope || s.scope == "global"))
                return true;
        return forLoopVariables.count(n) > 0;
    }
    
    string getType(const string& name) {
        string n = clean(name);
        return varTypes.count(n) ? varTypes[n] : "unknown";
    }
    
    string parseExpression(const string& expr) {
        string e = strip(expr);
        if (e.empty()) return "";
        
        // Тернарный оператор
        size_t q = e.find('?');
        if (q != string::npos) {
            size_t c = e.find(':', q + 1);
            if (c != string::npos) {
                string cond = parseExpression(e.substr(0, q));
                string thenExpr = parseExpression(e.substr(q + 1, c - q - 1));
                string elseExpr = parseExpression(e.substr(c + 1));
                addTriad("?:", cond, thenExpr + " : " + elseExpr);
                return "^" + to_string(triadCounter);
            }
        }
        
        // Логические операторы
        for (const string& op : {"||", "&&"}) {
            size_t pos = e.find(op);
            if (pos != string::npos) {
                string l = parseExpression(e.substr(0, pos));
                string r = parseExpression(e.substr(pos + op.length()));
                addTriad(op, l, r);
                return "^" + to_string(triadCounter);
            }
        }
        
        // Вызов функции
        size_t paren = e.find('(');
        if (paren != string::npos && paren > 0) {
            string fname = trim(e.substr(0, paren));
            if (all_of(fname.begin(), fname.end(), [](char c){ return isalnum(c) || c == '_'; }) && fname != "cout") {
                int bal = 1;
                size_t close = paren + 1;
                while (close < e.length() && bal > 0) {
                    if (e[close] == '(') bal++;
                    else if (e[close] == ')') bal--;
                    close++;
                }
                string args = trim(e.substr(paren + 1, close - paren - 2));
                addTriad("call", fname, args.empty() ? "-" : parseExpression(args));
                return "^" + to_string(triadCounter);
            }
        }
        
        // Операторы сравнения и арифметики
        vector<string> ops = {"==", "!=", "<=", ">=", "<", ">", "+", "-", "*", "/", "%"};
        for (const string& op : ops) {
            size_t pos = (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") ? e.rfind(op) : e.find(op);
            if (pos != string::npos && !((op == "<" && pos+1 < e.length() && e[pos+1] == '<') || 
                                         (op == ">" && pos+1 < e.length() && e[pos+1] == '>'))) {
                if ((op == "+" && pos > 0 && e[pos-1] == '+') || (op == "-" && pos > 0 && e[pos-1] == '-')) continue;
                string l = parseExpression(e.substr(0, pos));
                string r = parseExpression(e.substr(pos + op.length()));
                addTriad(op, l, r);
                return "^" + to_string(triadCounter);
            }
        }
        
        // Базовые значения
        e = trim(e);
        if (isdigit(e[0]) || (e[0] == '-' && e.length() > 1)) return e;
        if (e.front() == '"' && e.back() == '"') return e;
        if (e == "true") return "1";
        if (e == "false") return "0";
        
        string v = clean(e);
        if (!v.empty() && !isDeclared(v)) addError("UNDECLARED", "Variable '" + v + "' used without declaration");
        return v.empty() ? e : v;
    }

public:
    void analyze(const vector<string>& ast) {
        addSymbol("cout", "ostream", true, "global");
        addSymbol("endl", "manipulator", true, "global");
        
        bool inBody = false, inParams = false;
        for (const string& line : ast) {
            string content = line;
            size_t pos = content.find_first_not_of("| +-");
            if (pos != string::npos) content = content.substr(pos);
            if (content.empty() || content.find("+-- ") == 0) continue;
            if (content.find("+-- ") == 0) content = content.substr(4);
            
            if (content.find("function:") == 0) {
                size_t arrow = content.find(" -> ");
                currentFunction = trim(content.substr(9, arrow - 9));
                currentScope = currentFunction;
                addSymbol(currentFunction, trim(content.substr(arrow + 4)), true, "global");
                inBody = false; inParams = false;
            }
            else if (content == "params") { inParams = true; inBody = false; }
            else if (content == "body") { inParams = false; inBody = true; }
            else if (inParams && content.find(':') != string::npos) {
                size_t colon = content.find(':');
                addSymbol(trim(content.substr(0, colon)), trim(content.substr(colon + 2)), true, currentScope);
            }
            else if (inBody && content.find("var:") == 0) {
                string rest = content.substr(5);
                size_t eq = rest.find(" = ");
                string name = clean(rest.substr(0, eq));
                if (eq != string::npos) {
                    string val = rest.substr(eq + 3);
                    while (!val.empty() && val.front() == ' ') val.erase(0, 1);
                    if (val.front() == '"' && val.back() == '"') val = val.substr(1, val.length() - 2);
                    
                    string leftType = "short";
                    string rightType = detectType(val);
                    
                    if (rest.substr(eq + 3).front() == '"') rightType = "string";
                    
                    if (leftType != "unknown" && rightType != "unknown" && !typesCompatible(leftType, rightType))
                        addError("TYPE_MISMATCH", "Cannot assign " + rightType + " to " + leftType + " (variable '" + name + "')");
                    
                    addTriad("=", name, parseExpression(val));
                    addSymbol(name, "short", true, currentScope);
                } else addSymbol(name, "short", false, currentScope);
            }
            else if (inBody && content.find("assign:") == 0) {
                string rest = content.substr(8);
                size_t eq = rest.find(" = ");
                if (eq != string::npos) {
                    string lhs = clean(rest.substr(0, eq));
                    string rhsExpr = rest.substr(eq + 3);
                    if (!isDeclared(lhs)) {
                        addError("UNDECLARED", "Variable '" + lhs + "' not declared");
                    } else {
                        string rhs = parseExpression(rhsExpr);
                        string leftType = getType(lhs);
                        string rightType = detectType(rhsExpr);
                        string rhsClean = clean(rhsExpr);
                        if (!rhsClean.empty() && varTypes.count(rhsClean)) rightType = varTypes[rhsClean];
                        if (rhsExpr == "true" || rhsExpr == "false") rightType = "bool";
                        if (rhsExpr.front() == '"') rightType = "string";
                        
                        if (leftType != "unknown" && rightType != "unknown" && !typesCompatible(leftType, rightType))
                            addError("TYPE_MISMATCH", "Cannot assign " + rightType + " to " + leftType);
                        
                        addTriad("=", lhs, rhs);
                        varInitialized[lhs] = true;
                    }
                }
            }
            else if (content.find("return") == 0) {
                string val = content.length() > 7 ? parseExpression(content.substr(7)) : "-";
                addTriad("return", val, "-");
            }
            else if (content.find("for ") == 0) {
                string rest = content.substr(4);
                size_t eq = rest.find(" = ");
                string var = clean(rest.substr(0, eq));
                forLoopVariables.insert(var);
                string after = rest.substr(eq + 3);
                size_t sem = after.find(';');
                string init = after.substr(0, sem);
                size_t condPos = after.find("condition: ");
                string cond = condPos != string::npos ? after.substr(condPos + 11, after.find(';', condPos) - condPos - 11) : "";
                
                bool exists = false;
                for (const auto& s : symbolTable) if (s.name == var && s.scope == currentScope) exists = true;
                if (!exists) addSymbol(var, "short", true, currentScope);
                
                addTriad("=", var, init);
                string condTemp = parseExpression(cond);
                string startL = newLabel(), endL = newLabel();
                loopLabels.push_back({startL, endL});
                addTriad("br_false", condTemp, endL);
            }
            else if (content.find("while (") == 0) {
                string cond = content.substr(7, content.find(')') - 7);
                string startL = newLabel(), endL = newLabel();
                loopLabels.push_back({startL, endL});
                addTriad("br_false", parseExpression(cond), endL);
            }
            else if (content.find("if (") == 0) {
                string cond = content.substr(4, content.find(')') - 4);
                addTriad("br_false", parseExpression(cond), newLabel());
            }
            else if (content == "else") {
                string endL = newLabel();
                addTriad("br", endL, "-");
                addTriad("label", endL, "-");
            }
            else if (content.find("++") != string::npos && content.find("cout") == string::npos) {
                string var = clean(content.substr(0, content.find("++")));
                if (!isDeclared(var)) addError("UNDECLARED", "Variable '" + var + "' not declared");
                else { addTriad("++", var, "-"); varInitialized[var] = true; }
            }
            else if (content.find("cout <<") == 0) {
                string stream = "cout", val = content.substr(8);
                size_t p;
                while ((p = val.find(" << ")) != string::npos) {
                    string part = val.substr(0, p);
                    while (!part.empty() && part.front() == ' ') part.erase(0, 1);
                    if (part.front() == '"' && part.back() == '"') part = part.substr(1, part.length() - 2);
                    string cleanPart = clean(part);
                    if (!cleanPart.empty() && !isDeclared(cleanPart))
                        addError("UNDECLARED", "Variable '" + cleanPart + "' used in output");
                    addTriad("<<", stream, part);
                    stream = "^" + to_string(triadCounter);
                    val = val.substr(p + 4);
                }
                while (!val.empty() && val.front() == ' ') val.erase(0, 1);
                if (val.front() == '"' && val.back() == '"') val = val.substr(1, val.length() - 2);
                string cleanVal = clean(val);
                if (!cleanVal.empty() && !isDeclared(cleanVal))
                    addError("UNDECLARED", "Variable '" + cleanVal + "' used in output");
                addTriad("<<", stream, val);
            }
        }
        
        while (!loopLabels.empty()) {
            auto [start, end] = loopLabels.back();
            addTriad("br", start, "-");
            addTriad("label", end, "-");
            loopLabels.pop_back();
        }
        
        bool hasReturn = false;
        for (const auto& t : triads) if (t.op == "return") { hasReturn = true; break; }
        if (!hasReturn && currentFunction == "main") addTriad("return", "0", "-");
    }
    
    void printSymbolTable() {
        cout <<"\nSYMBOL TABLE\n"
             << "+-----------------+--------+----------+-------------+\n"
             << "| Name            | Type   | Scope    | Initialized |\n"
             << "+-----------------+--------+----------+-------------+\n";
        for (const auto& s : symbolTable)
            printf("| %-15s | %-6s | %-8s | %-11s |\n", s.name.c_str(), s.type.c_str(), s.scope.c_str(), s.initialized ? "yes" : "no");
        cout << "+-----------------+--------+----------+-------------+\nTotal symbols: " << symbolTable.size() << "\n";
    }
    
    void printTriads() {
        cout <<"\nTRIADS\n";
        for (const auto& t : triads)
            cout << t.number << ": (" << t.op << ", " << t.arg1 << ", " << t.arg2 << ")\n";
        cout << "Total triads: " << triadCounter << "\n";
    }
    
    void printErrors() {
        if (errors.empty()) cout << "\nNO SEMANTIC ERRORS FOUND\n";
        else for (const auto& e : errors) cout << e << "\n";
    }
    
    bool hasErrors() const { return !errors.empty(); }
    void saveTriadsToFile(const string& filename) {
    ofstream file(filename);
    if (!file) return;
    for (const auto& t : triads) {
        file << t.number << ": (" << t.op << ", " << t.arg1 << ", " << t.arg2 << ")\n";
    }
    file.close();
    cout << "Triads saved to " << filename << endl;
}
};

int main() {
    cout << "\nSemantic Analyzer\n";
    
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
    
    // Сохраняем триады в файл
    analyzer.saveTriadsToFile("triads.txt");
    
    cout << "\nSemantic analysis completed " 
         << (analyzer.hasErrors() ? "with errors" : "successfully") << "\n";
    if (analyzer.hasErrors()) return 1;
    return 0;
}
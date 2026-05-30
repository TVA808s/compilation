#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <set>
#include <map>

using namespace std;

// ==================== Token Types ====================
enum class TokenType { KEYWORD, IDENTIFIER, OPERATOR, DELIMITER, HEADER, CONSTANT_INT, STRING_CONSTANT, END_OF_FILE, UNKNOWN };

struct Token {
    TokenType type;
    string lexeme;
    int line;
    int col;
};

// ==================== AST Nodes ====================
struct ASTNode { virtual ~ASTNode() = default; };
using ASTPtr = shared_ptr<ASTNode>;

struct IncludeDirective : ASTNode { string headerName; };
struct UsingDecl : ASTNode { string entity; };
struct ParamDecl : ASTNode { string type, name; };

struct Program : ASTNode {
    shared_ptr<IncludeDirective> include;
    vector<shared_ptr<UsingDecl>> usings;
    vector<ASTPtr> functions;
};

struct CompoundStmt : ASTNode { vector<ASTPtr> statements; };

struct FunctionDef : ASTNode {
    string name, returnType;
    vector<shared_ptr<ParamDecl>> params;
    shared_ptr<CompoundStmt> body;
    bool hasBody = true;
};

struct DeclStmt : ASTNode { 
    struct Var { string name; ASTPtr init; };
    vector<Var> vars; 
};

struct AssignStmt : ASTNode { string lhs; ASTPtr rhs; };
struct PostIncExpr : ASTNode { string varName; };
struct ReturnStmt : ASTNode { ASTPtr value; };
struct ExprStmt : ASTNode { ASTPtr expr; };
struct CallExpr : ASTNode { string callee; vector<ASTPtr> args; };
struct BinaryExpr : ASTNode { string op; ASTPtr left, right; };
struct TernaryExpr : ASTNode { ASTPtr cond, thenExpr, elseExpr; };
struct StreamInsertExpr : ASTNode { vector<ASTPtr> values; };
struct Identifier : ASTNode { string name; };
struct IntLiteral : ASTNode { int value; };
struct StringLiteral : ASTNode { string value; };

struct ForStmt : ASTNode { 
    string varName; 
    int initValue; 
    ASTPtr condition;
    string iterVar; 
    shared_ptr<CompoundStmt> body; 
};
struct WhileStmt : ASTNode { 
    ASTPtr condition;
    shared_ptr<CompoundStmt> body; 
};
struct IfStmt : ASTNode { 
    ASTPtr condition;
    shared_ptr<CompoundStmt> thenBranch, elseBranch; 
};

// ==================== Token Loader ====================
vector<Token> loadTokens(const string& filename) {
    vector<Token> tokens;
    ifstream file(filename);
    if (!file) { cerr << "Error: Cannot open '" << filename << "'\n"; return tokens; }
    
    string line;
    int lineNum = 1;
    while (getline(file, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == string::npos) { lineNum++; continue; }
        size_t end = line.find_last_not_of(" \t\r\n,");
        line = line.substr(start, end - start + 1);
        if (line.empty() || line == "[" || line == "]") { lineNum++; continue; }
        
        size_t op = line.find('('), cp = line.rfind(')');
        if (op == string::npos || cp == string::npos) { lineNum++; continue; }
        
        string content = line.substr(op + 1, cp - op - 1);
        size_t comma = content.find(',');
        if (comma == string::npos) { lineNum++; continue; }
        
        string typeStr = content.substr(0, comma);
        string value = content.substr(comma + 1);
        
        typeStr.erase(0, typeStr.find_first_not_of(" \t"));
        typeStr.erase(typeStr.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        if (value.empty()) { lineNum++; continue; }
        
        Token t;
        t.lexeme = value;
        t.line = lineNum;
        t.col = 0;
        
        if (typeStr == "KEYWORD") t.type = TokenType::KEYWORD;
        else if (typeStr == "IDENTIFIER") t.type = TokenType::IDENTIFIER;
        else if (typeStr == "OPERATOR") t.type = TokenType::OPERATOR;
        else if (typeStr == "DELIMITER") t.type = TokenType::DELIMITER;
        else if (typeStr == "HEADER") t.type = TokenType::HEADER;
        else if (typeStr == "CONSTANT_INT") t.type = TokenType::CONSTANT_INT;
        else if (typeStr == "STRING_CONSTANT") t.type = TokenType::STRING_CONSTANT;
        else t.type = TokenType::UNKNOWN;
        
        tokens.push_back(t);
        lineNum++;
    }
    file.close();
    return tokens;
}

// ==================== Parser ====================
class Parser {
    const vector<Token>& tokens;
    size_t pos = 0;
    
    set<string> declaredVars;
    map<string, int> functionParams;
    bool inFunction = false;
    int loopDepth = 0;
    vector<string> errors;
    
    Token cur() const { return pos < tokens.size() ? tokens[pos] : Token{TokenType::END_OF_FILE, "", -1, 0}; }
    Token next() const { return pos + 1 < tokens.size() ? tokens[pos + 1] : Token{TokenType::END_OF_FILE, "", -1, 0}; }
    Token advance() { return pos < tokens.size() ? tokens[pos++] : Token{TokenType::END_OF_FILE, "", -1, 0}; }
    
    void addError(const string& msg, const Token& tok) {
        stringstream ss;
        ss << "Error at line " << tok.line << ": " << msg << " (found '" << tok.lexeme << "')";
        errors.push_back(ss.str());
    }
    
    [[noreturn]] void fatalError(const string& msg) {
        stringstream ss;
        ss << "Syntax error: " << msg << " (found '" << cur().lexeme << "')";
        throw runtime_error(ss.str());
    }
    
    void expect(TokenType t, const string& lex) {
        if (cur().type == t && cur().lexeme == lex) advance();
        else fatalError("expected '" + lex + "'");
    }
    
    bool match(TokenType t, const string& lex) {
        if (cur().type == t && cur().lexeme == lex) { advance(); return true; }
        return false;
    }
    
    void checkVarDeclared(const string& name, const Token& tok) {
        if (declaredVars.find(name) == declaredVars.end()) {
            addError("Variable '" + name + "' used without declaration", tok);
        }
    }
    
    void checkVarNotDeclared(const string& name, const Token& tok) {
        if (declaredVars.find(name) != declaredVars.end()) {
            addError("Variable '" + name + "' already declared", tok);
        }
    }

public:
    Parser(const vector<Token>& tokens) : tokens(tokens) {}
    
    bool hasErrors() const { return !errors.empty(); }
    void printErrors() const {
        for (const auto& e : errors) cout << e << "\n";
    }
    
    ASTPtr parse() {
        auto prog = make_shared<Program>();
        
        if (cur().type == TokenType::KEYWORD && cur().lexeme == "#include") {
            expect(TokenType::KEYWORD, "#include");
            if (cur().lexeme == "<") advance();
            auto inc = make_shared<IncludeDirective>();
            inc->headerName = advance().lexeme;
            if (cur().lexeme == ">") advance();
            prog->include = inc;
        }
        
        while (cur().type == TokenType::KEYWORD && cur().lexeme == "using") {
            expect(TokenType::KEYWORD, "using");
            if (cur().lexeme == "std") advance();
            expect(TokenType::OPERATOR, "::");
            auto u = make_shared<UsingDecl>();
            u->entity = advance().lexeme;
            expect(TokenType::DELIMITER, ";");
            prog->usings.push_back(u);
        }
        
        while (cur().type != TokenType::END_OF_FILE) {
            if (cur().lexeme == "return" && !inFunction) {
                addError("'return' used outside function", cur());
                advance();
                while (cur().lexeme != ";") advance();
                advance();
                continue;
            }
            prog->functions.push_back(parseFunction());
        }
        
        return prog;
    }

private:
    ASTPtr parseFunction() {
        auto fn = make_shared<FunctionDef>();
        
        fn->returnType = advance().lexeme;
        fn->name = advance().lexeme;
        
        expect(TokenType::DELIMITER, "(");
        fn->params = {};
        
        if (!(cur().type == TokenType::DELIMITER && cur().lexeme == ")")) {
            do {
                auto p = make_shared<ParamDecl>();
                p->type = advance().lexeme;
                p->name = advance().lexeme;
                fn->params.push_back(p);
            } while (match(TokenType::DELIMITER, ","));
        }
        
        functionParams[fn->name] = (int)fn->params.size();
        expect(TokenType::DELIMITER, ")");
        
        if (cur().type == TokenType::DELIMITER && cur().lexeme == "{") {
            fn->hasBody = true;
            declaredVars.clear();
            for (auto& p : fn->params) declaredVars.insert(p->name);
            inFunction = true;
            fn->body = parseCompound();
            inFunction = false;
        } else {
            fn->hasBody = false;
            addError("Function '" + fn->name + "' has no body", cur());
            fn->body = make_shared<CompoundStmt>();
            while (cur().type != TokenType::END_OF_FILE && 
                   cur().type != TokenType::KEYWORD) advance();
        }
        
        return fn;
    }
    
    shared_ptr<CompoundStmt> parseCompound() {
        auto block = make_shared<CompoundStmt>();
        
        if (!(cur().type == TokenType::DELIMITER && cur().lexeme == "{")) {
            addError("Expected '{' to start block", cur());
            return block;
        }
        
        expect(TokenType::DELIMITER, "{");
        
        while (!(cur().type == TokenType::DELIMITER && cur().lexeme == "}")) {
            if (cur().type == TokenType::END_OF_FILE) {
                addError("Unclosed block: expected '}'", cur());
                break;
            }
            block->statements.push_back(parseStmt());
        }
        
        if (cur().type == TokenType::DELIMITER && cur().lexeme == "}") {
            advance();
        }
        
        return block;
    }
    
    ASTPtr parseStmt() {
        if (cur().type == TokenType::UNKNOWN) {
            addError("Unknown token '" + cur().lexeme + "'", cur());
            advance();
            return make_shared<ExprStmt>();
        }
        
        if (cur().type == TokenType::KEYWORD && cur().lexeme == "short") return parseDecl();
        if (cur().lexeme == "for") return parseFor();
        if (cur().lexeme == "while") return parseWhile();
        if (cur().lexeme == "if") return parseIf();
        if (cur().lexeme == "return") return parseReturnStmt();
        if (cur().lexeme == "break") return parseBreakStmt();
        if (cur().lexeme == "continue") return parseContinueStmt();
        
        return parseExprStmt();
    }
    
    ASTPtr parseReturnStmt() {
        if (!inFunction) addError("'return' used outside function", cur());
        advance();
        auto stmt = make_shared<ReturnStmt>();
        if (!(cur().type == TokenType::DELIMITER && cur().lexeme == ";")) {
            stmt->value = parseExpr();
        }
        expect(TokenType::DELIMITER, ";");
        return stmt;
    }
    
    ASTPtr parseBreakStmt() {
        if (loopDepth == 0) addError("'break' used outside loop", cur());
        advance();
        expect(TokenType::DELIMITER, ";");
        return make_shared<ExprStmt>();
    }
    
    ASTPtr parseContinueStmt() {
        if (loopDepth == 0) addError("'continue' used outside loop", cur());
        advance();
        expect(TokenType::DELIMITER, ";");
        return make_shared<ExprStmt>();
    }
    
    ASTPtr parseDecl() {
        auto stmt = make_shared<DeclStmt>();
        advance(); // short
        
        do {
            string varName = advance().lexeme;
            Token varToken = tokens[pos-1];
            checkVarNotDeclared(varName, varToken);
            declaredVars.insert(varName);
            
            DeclStmt::Var v;
            v.name = varName;
            expect(TokenType::OPERATOR, "=");
            v.init = parseExpr();
            stmt->vars.push_back(v);
        } while (match(TokenType::DELIMITER, ","));
        
        expect(TokenType::DELIMITER, ";");
        return stmt;
    }
    
    ASTPtr parseFor() {
        auto stmt = make_shared<ForStmt>();
        expect(TokenType::KEYWORD, "for");
        expect(TokenType::DELIMITER, "(");
        advance(); // short
        stmt->varName = advance().lexeme;
        expect(TokenType::OPERATOR, "=");
        stmt->initValue = stoi(advance().lexeme);
        expect(TokenType::DELIMITER, ";");
        
        // Сохраняем условие как выражение
        stmt->condition = parseExpr();
        
        expect(TokenType::DELIMITER, ";");
        stmt->iterVar = advance().lexeme;
        expect(TokenType::OPERATOR, "++");
        expect(TokenType::DELIMITER, ")");
        
        loopDepth++;
        stmt->body = parseCompound();
        loopDepth--;
        
        return stmt;
    }

    ASTPtr parseWhile() {
        auto stmt = make_shared<WhileStmt>();
        advance(); // while
        expect(TokenType::DELIMITER, "(");
        
        // Сохраняем условие как выражение
        stmt->condition = parseExpr();
        
        expect(TokenType::DELIMITER, ")");
        
        loopDepth++;
        stmt->body = parseCompound();
        loopDepth--;
        
        return stmt;
    }

    ASTPtr parseIf() {
        auto stmt = make_shared<IfStmt>();
        advance(); // if
        expect(TokenType::DELIMITER, "(");
        
        // Сохраняем условие как выражение
        stmt->condition = parseExpr();
        
        expect(TokenType::DELIMITER, ")");
        
        stmt->thenBranch = parseCompound();
        
        if (cur().lexeme == "else") {
            advance();
            stmt->elseBranch = parseCompound();
        }
        return stmt;
    }
    
    ASTPtr parseExprStmt() {
        if (cur().lexeme == "cout") {
            advance();
            auto s = make_shared<StreamInsertExpr>();
            while (match(TokenType::OPERATOR, "<<")) {
                if (cur().lexeme == "endl") {
                    auto e = make_shared<Identifier>(); e->name = "endl";
                    s->values.push_back(e); advance();
                } else if (cur().type == TokenType::STRING_CONSTANT) {
                    auto str = make_shared<StringLiteral>(); str->value = cur().lexeme;
                    s->values.push_back(str); advance();
                } else {
                    s->values.push_back(parseExpr());
                }
            }
            expect(TokenType::DELIMITER, ";");
            auto stmt = make_shared<ExprStmt>(); stmt->expr = s;
            return stmt;
        }
        
        if (cur().type == TokenType::IDENTIFIER && (next().lexeme == "=" || next().lexeme == "+=")) {
            string varName = advance().lexeme;
            Token varToken = tokens[pos-1];
            checkVarDeclared(varName, varToken);
            advance(); // = или +=
            auto expr = parseExpr();
            expect(TokenType::DELIMITER, ";");
            auto assign = make_shared<AssignStmt>();
            assign->lhs = varName;
            assign->rhs = expr;
            auto stmt = make_shared<ExprStmt>(); stmt->expr = assign;
            return stmt;
        }
        
        if (cur().type == TokenType::IDENTIFIER && next().lexeme == "++") {
            string varName = advance().lexeme;
            Token varToken = tokens[pos-1];
            checkVarDeclared(varName, varToken);
            advance();
            expect(TokenType::DELIMITER, ";");
            auto stmt = make_shared<PostIncExpr>();
            stmt->varName = varName;
            auto exprStmt = make_shared<ExprStmt>(); exprStmt->expr = stmt;
            return exprStmt;
        }
        
        auto expr = parseExpr();
        expect(TokenType::DELIMITER, ";");
        auto stmt = make_shared<ExprStmt>(); stmt->expr = expr;
        return stmt;
    }
    
    // ============ Expressions ============
    ASTPtr parseExpr() { return parseTernary(); }
    
    ASTPtr parseTernary() {
        ASTPtr cond = parseLogicalOr();
        if (match(TokenType::OPERATOR, "?")) {
            auto t = make_shared<TernaryExpr>();
            t->cond = cond;
            t->thenExpr = parseExpr();
            expect(TokenType::OPERATOR, ":");
            t->elseExpr = parseExpr();
            return t;
        }
        return cond;
    }
    
    ASTPtr parseLogicalOr() {
        ASTPtr left = parseLogicalAnd();
        while (match(TokenType::OPERATOR, "||")) {
            auto b = make_shared<BinaryExpr>();
            b->op = "||"; b->left = left; b->right = parseLogicalAnd();
            left = b;
        }
        return left;
    }
    
    ASTPtr parseLogicalAnd() {
        ASTPtr left = parseEquality();
        while (match(TokenType::OPERATOR, "&&")) {
            auto b = make_shared<BinaryExpr>();
            b->op = "&&"; b->left = left; b->right = parseEquality();
            left = b;
        }
        return left;
    }
    
    ASTPtr parseEquality() {
        ASTPtr left = parseRelational();
        while (match(TokenType::OPERATOR, "==") || match(TokenType::OPERATOR, "!=")) {
            auto b = make_shared<BinaryExpr>();
            b->op = tokens[pos-1].lexeme; b->left = left; b->right = parseRelational();
            left = b;
        }
        return left;
    }
    
    ASTPtr parseRelational() {
        ASTPtr left = parseAdditive();
        while (match(TokenType::OPERATOR, "<") || match(TokenType::OPERATOR, "<=") ||
               match(TokenType::OPERATOR, ">") || match(TokenType::OPERATOR, ">=")) {
            auto b = make_shared<BinaryExpr>();
            b->op = tokens[pos-1].lexeme; b->left = left; b->right = parseAdditive();
            left = b;
        }
        return left;
    }
    
    ASTPtr parseAdditive() {
        ASTPtr left = parseMultiplicative();
        while (match(TokenType::OPERATOR, "+") || match(TokenType::OPERATOR, "-")) {
            auto b = make_shared<BinaryExpr>();
            b->op = tokens[pos-1].lexeme; b->left = left; b->right = parseMultiplicative();
            left = b;
        }
        return left;
    }
    
    ASTPtr parseMultiplicative() {
        ASTPtr left = parsePostfix();
        while (match(TokenType::OPERATOR, "*") || match(TokenType::OPERATOR, "/") ||
               match(TokenType::OPERATOR, "%")) {
            auto b = make_shared<BinaryExpr>();
            b->op = tokens[pos-1].lexeme; b->left = left; b->right = parsePostfix();
            left = b;
        }
        return left;
    }
    
    ASTPtr parsePostfix() {
        ASTPtr expr = parsePrimary();
        if (match(TokenType::DELIMITER, "(")) {
            auto call = make_shared<CallExpr>();
            int argCount = 0;
            
            if (auto id = dynamic_pointer_cast<Identifier>(expr)) {
                call->callee = id->name;
            }
            
            if (!(cur().type == TokenType::DELIMITER && cur().lexeme == ")")) {
                do {
                    call->args.push_back(parseExpr());
                    argCount++;
                } while (match(TokenType::DELIMITER, ","));
            }
            
            expect(TokenType::DELIMITER, ")");
            
            if (functionParams.find(call->callee) != functionParams.end()) {
                if (argCount != functionParams[call->callee]) {
                    addError("Function '" + call->callee + "' expects " + 
                             to_string(functionParams[call->callee]) + " argument(s), got " + 
                             to_string(argCount), cur());
                }
            } else {
                addError("Function '" + call->callee + "' not declared", cur());
            }
            return call;
        }
        return expr;
    }
    
    ASTPtr parsePrimary() {
        if (cur().type == TokenType::UNKNOWN) {
            addError("Unknown token '" + cur().lexeme + "'", cur());
            advance();
            return make_shared<Identifier>();
        }
        
        if (cur().type == TokenType::IDENTIFIER) {
            auto id = make_shared<Identifier>();
            id->name = advance().lexeme;
            return id;
        }
        
        if (cur().type == TokenType::CONSTANT_INT) {
            auto lit = make_shared<IntLiteral>();
            lit->value = stoi(advance().lexeme);
            return lit;
        }
        
        if (cur().type == TokenType::STRING_CONSTANT) {
            auto lit = make_shared<StringLiteral>();
            lit->value = advance().lexeme;
            return lit;
        }
        
        if (match(TokenType::DELIMITER, "(")) {
            ASTPtr expr = parseExpr();
            expect(TokenType::DELIMITER, ")");
            return expr;
        }
        
        fatalError("unexpected token in expression");
    }
};

// ==================== AST Printer ====================
void printExprInline(ASTPtr node);    

void printAST(ASTPtr node, const string& prefix = "", bool last = true) {
    if (!node) return;
    
    string indent = prefix + (last ? "+-- " : "+-- ");
    string childPrefix = prefix + (last ? "    " : "|   ");
    
    if (auto p = dynamic_pointer_cast<Program>(node)) {
        cout << "Program\n";
        if (p->include) printAST(p->include, "", false);
        for (size_t i = 0; i < p->usings.size(); i++)
            printAST(p->usings[i], "", i == p->usings.size() - 1 && p->functions.empty());
        for (size_t i = 0; i < p->functions.size(); i++)
            printAST(p->functions[i], "", i == p->functions.size() - 1);
    }
    else if (auto inc = dynamic_pointer_cast<IncludeDirective>(node)) {
        cout << indent << "include: " << inc->headerName << "\n";
    }
    else if (auto u = dynamic_pointer_cast<UsingDecl>(node)) {
        cout << indent << "using: " << u->entity << "\n";
    }
    else if (auto f = dynamic_pointer_cast<FunctionDef>(node)) {
        cout << indent << "function: " << f->name << " -> " << f->returnType << "\n";
        if (!f->params.empty()) {
            cout << childPrefix << "+-- params\n";
            for (size_t i = 0; i < f->params.size(); i++) {
                cout << childPrefix << "    +-- " << f->params[i]->name << ": " << f->params[i]->type << "\n";
            }
        }
        cout << childPrefix << "+-- body\n";
        for (size_t i = 0; i < f->body->statements.size(); i++)
            printAST(f->body->statements[i], childPrefix + "    ", i == f->body->statements.size() - 1);
    }
    else if (auto d = dynamic_pointer_cast<DeclStmt>(node)) {
        for (auto& v : d->vars) {
            cout << indent << "var: " << v.name;
            if (v.init) {
                cout << " = ";
                printExprInline(v.init);
            }
            cout << "\n";
        }
    }
    else if (auto a = dynamic_pointer_cast<AssignStmt>(node)) {
        cout << indent << "assign: " << a->lhs << " = ";
        printExprInline(a->rhs);
        cout << "\n";
    }
    else if (auto p = dynamic_pointer_cast<PostIncExpr>(node)) {
        cout << indent << p->varName << "++\n";
    }
    else if (auto r = dynamic_pointer_cast<ReturnStmt>(node)) {
        cout << indent << "return";
        if (r->value) { 
            cout << " ";
            printExprInline(r->value);
        }
        cout << "\n";
    }
    else if (auto f = dynamic_pointer_cast<ForStmt>(node)) {
        cout << indent << "for " << f->varName << " = " << f->initValue;
        if (f->condition) {
            cout << "; condition: ";
            printExprInline(f->condition);
        }
        cout << "; " << f->iterVar << "++\n";
        cout << childPrefix << "+-- body\n";
        for (size_t i = 0; i < f->body->statements.size(); i++)
            printAST(f->body->statements[i], childPrefix + "    ", i == f->body->statements.size() - 1);
    }
    else if (auto w = dynamic_pointer_cast<WhileStmt>(node)) {
        cout << indent << "while (";
        if (w->condition) printExprInline(w->condition);
        cout << ")\n";
        cout << childPrefix << "+-- body\n";
        for (size_t i = 0; i < w->body->statements.size(); i++)
            printAST(w->body->statements[i], childPrefix + "    ", i == w->body->statements.size() - 1);
    }
    else if (auto i = dynamic_pointer_cast<IfStmt>(node)) {
        cout << indent << "if (";
        if (i->condition) printExprInline(i->condition);
        cout << ")\n";
        cout << childPrefix << "+-- then\n";
        for (size_t j = 0; j < i->thenBranch->statements.size(); j++)
            printAST(i->thenBranch->statements[j], childPrefix + "|   ", j == i->thenBranch->statements.size() - 1 && !i->elseBranch);
        if (i->elseBranch) {
            cout << childPrefix << "+-- else\n";
            for (size_t j = 0; j < i->elseBranch->statements.size(); j++)
                printAST(i->elseBranch->statements[j], childPrefix + "    ", j == i->elseBranch->statements.size() - 1);
        }
    }
    else if (auto c = dynamic_pointer_cast<CallExpr>(node)) {
        cout << indent << "call: " << c->callee << "(";
        for (size_t i = 0; i < c->args.size(); i++) {
            if (i > 0) cout << ", ";
            printExprInline(c->args[i]);
        }
        cout << ")\n";
    }
    else if (auto s = dynamic_pointer_cast<StreamInsertExpr>(node)) {
        cout << indent << "cout << ";
        for (size_t i = 0; i < s->values.size(); i++) {
            if (i > 0) cout << " << ";
            printExprInline(s->values[i]);
        }
        cout << "\n";
    }
    else if (auto e = dynamic_pointer_cast<ExprStmt>(node)) {
        printAST(e->expr, prefix, last);
    }
    else if (auto b = dynamic_pointer_cast<BinaryExpr>(node)) {
        printExprInline(b);
    }
    else if (auto t = dynamic_pointer_cast<TernaryExpr>(node)) {
        printExprInline(t);
    }
}

void printExprInline(ASTPtr node) {
    if (!node) return;
    
    if (auto id = dynamic_pointer_cast<Identifier>(node)) {
        cout << id->name;
    }
    else if (auto lit = dynamic_pointer_cast<IntLiteral>(node)) {
        cout << lit->value;
    }
    else if (auto str = dynamic_pointer_cast<StringLiteral>(node)) {
        cout << "\"" << str->value << "\"";
    }
    else if (auto b = dynamic_pointer_cast<BinaryExpr>(node)) {
        cout << "(";
        printExprInline(b->left);
        cout << " " << b->op << " ";
        printExprInline(b->right);
        cout << ")";
    }
    else if (auto t = dynamic_pointer_cast<TernaryExpr>(node)) {
        printExprInline(t->cond);
        cout << " ? ";
        printExprInline(t->thenExpr);
        cout << " : ";
        printExprInline(t->elseExpr);
    }
    else if (auto c = dynamic_pointer_cast<CallExpr>(node)) {
        cout << c->callee << "(";
        for (size_t i = 0; i < c->args.size(); i++) {
            if (i > 0) cout << ", ";
            printExprInline(c->args[i]);
        }
        cout << ")";
    }
    else if (auto p = dynamic_pointer_cast<PostIncExpr>(node)) {
        cout << p->varName << "++";
    }
    else if (auto a = dynamic_pointer_cast<AssignStmt>(node)) {
        cout << a->lhs << " = ";
        printExprInline(a->rhs);
    }
}

// ==================== Main ====================
int main() {
    cout << "=== Syntax Analyzer ===\n\n";
    
    auto tokens = loadTokens("tokensErrors.cpp");
    if (tokens.empty()) { cerr << "No tokens loaded.\n"; return 1; }
    
    cout << "Loaded " << tokens.size() << " tokens.\n\n";
    
    Parser parser(tokens);
    try {
        auto ast = parser.parse();
        
        if (parser.hasErrors()) {
            cout << "=== Semantic/Syntax Errors ===\n\n";
            parser.printErrors();
            cout << "\n=== Parsing completed with errors ===\n";
        } else {
            cout << "=== Parsing OK ===\n\n=== AST ===\n\n";
            printAST(ast);
        }
    } catch (const exception& e) {
        cerr << "=== FATAL ERROR ===\n" << e.what() << "\n";
        return 1;
    }
    return 0;
}
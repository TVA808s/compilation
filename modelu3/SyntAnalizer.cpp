#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <sstream>

// ==================== Token Types ====================

enum class TokenType {
    KEYWORD, IDENTIFIER, OPERATOR, DELIMITER,
    HEADER, CONSTANT_INT, STRING_CONSTANT, END_OF_FILE
};

struct Token {
    TokenType type;
    std::string lexeme;
};

// ==================== AST Nodes ====================

struct ASTNode { virtual ~ASTNode() = default; };
using ASTPtr = std::shared_ptr<ASTNode>;

struct IncludeDirective : ASTNode {
    std::string headerName;
};

struct UsingDecl : ASTNode {
    std::string entity;
};

struct ParamDecl : ASTNode {
    std::string type, name;
};

struct Program : ASTNode {
    std::shared_ptr<IncludeDirective> include;
    std::vector<std::shared_ptr<UsingDecl>> usings;
    std::vector<ASTPtr> functions;
};

struct CompoundStmt : ASTNode {
    std::vector<ASTPtr> statements;
};

struct FunctionDef : ASTNode {
    std::string name, returnType;
    std::vector<std::shared_ptr<ParamDecl>> params;
    std::shared_ptr<CompoundStmt> body;
};

struct DeclStmt : ASTNode {
    struct Var { std::string name; ASTPtr init; };
    std::vector<Var> vars;
};

struct ForStmt : ASTNode {
    std::string varName;
    int initValue;
    ASTPtr condition;
    std::string iterVar;
    std::shared_ptr<CompoundStmt> body;
};

struct WhileStmt : ASTNode {
    ASTPtr condition;
    std::shared_ptr<CompoundStmt> body;
};

struct IfStmt : ASTNode {
    ASTPtr condition;
    std::shared_ptr<CompoundStmt> thenBranch, elseBranch;
};

struct ReturnStmt : ASTNode {
    ASTPtr value;
};

struct ExprStmt : ASTNode {
    ASTPtr expr;
};

struct AssignStmt : ASTNode {
    std::string op, lhs;
    ASTPtr rhs;
};

struct PostIncExpr : ASTNode {
    std::string varName;
};

struct StreamInsertExpr : ASTNode {
    std::vector<ASTPtr> values;
};

struct CallExpr : ASTNode {
    std::string callee;
    ASTPtr arg;
};

struct BinaryExpr : ASTNode {
    std::string op;
    ASTPtr left, right;
};

struct TernaryExpr : ASTNode {
    ASTPtr cond, thenExpr, elseExpr;
};

struct Identifier : ASTNode {
    std::string name;
};

struct IntLiteral : ASTNode {
    int value;
};

struct StringLiteral : ASTNode {
    std::string value;
};

// ==================== Token Loader ====================

std::vector<Token> loadTokens(const std::string& filename) {
    std::vector<Token> tokens;
    std::ifstream file(filename);
    if (!file) { std::cerr << "Error: Cannot open '" << filename << "'\n"; return tokens; }
    
    std::string line;
    while (std::getline(file, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n,");
        line = line.substr(start, end - start + 1);
        if (line.empty() || line == "[" || line == "]") continue;
        
        size_t op = line.find('('), cp = line.rfind(')');
        if (op == std::string::npos || cp == std::string::npos) continue;
        
        std::string content = line.substr(op + 1, cp - op - 1);
        size_t comma = content.find(',');
        if (comma == std::string::npos) continue;
        
        std::string typeStr = content.substr(0, comma);
        std::string value = content.substr(comma + 1);
        
        typeStr.erase(0, typeStr.find_first_not_of(" \t"));
        typeStr.erase(typeStr.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        if (value.empty()) continue;
        
        Token t;
        t.lexeme = value;
        if (typeStr == "KEYWORD") t.type = TokenType::KEYWORD;
        else if (typeStr == "IDENTIFIER") t.type = TokenType::IDENTIFIER;
        else if (typeStr == "OPERATOR") t.type = TokenType::OPERATOR;
        else if (typeStr == "DELIMITER") t.type = TokenType::DELIMITER;
        else if (typeStr == "HEADER") t.type = TokenType::HEADER;
        else if (typeStr == "CONSTANT_INT") t.type = TokenType::CONSTANT_INT;
        else if (typeStr == "STRING_CONSTANT") t.type = TokenType::STRING_CONSTANT;
        else continue;
        tokens.push_back(t);
    }
    file.close();
    return tokens;
}

// ==================== Parser ====================

class Parser {
    const std::vector<Token>& tokens;
    size_t pos = 0;

    Token cur() const { return pos < tokens.size() ? tokens[pos] : Token{TokenType::END_OF_FILE, ""}; }
    Token next() const { return pos + 1 < tokens.size() ? tokens[pos + 1] : Token{TokenType::END_OF_FILE, ""}; }
    Token advance() { return pos < tokens.size() ? tokens[pos++] : Token{TokenType::END_OF_FILE, ""}; }
    
    [[noreturn]] void error(const std::string& msg) {
        std::stringstream ss;
        ss << "Syntax error: " << msg << " (found '" << cur().lexeme << "')";
        throw std::runtime_error(ss.str());
    }
    
    Token expect(TokenType t, const std::string& lex) {
        if (cur().type == t && cur().lexeme == lex) return advance();
        error("expected '" + lex + "'");
    }
    
    bool match(TokenType t, const std::string& lex) {
        if (cur().type == t && cur().lexeme == lex) { advance(); return true; }
        return false;
    }
    
    bool isType() const {
        return cur().type == TokenType::KEYWORD && 
               (cur().lexeme == "short" || cur().lexeme == "int" || cur().lexeme == "bool");
    }

public:
    Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

    std::shared_ptr<Program> parse() {
        auto prog = std::make_shared<Program>();
        
        // #include <iostream>
        expect(TokenType::KEYWORD, "#include");
        expect(TokenType::DELIMITER, "<");
        auto inc = std::make_shared<IncludeDirective>();
        inc->headerName = advance().lexeme;  // любое имя заголовка
        expect(TokenType::DELIMITER, ">");
        prog->include = inc;
        
        // using std::...
        while (cur().type == TokenType::KEYWORD && cur().lexeme == "using") {
            auto u = parseUsing();
            prog->usings.push_back(u);
        }
        
        // functions
        while (!(cur().type == TokenType::END_OF_FILE)) {
            prog->functions.push_back(parseFunction());
        }
        
        return prog;
    }

private:
    std::shared_ptr<UsingDecl> parseUsing() {
        expect(TokenType::KEYWORD, "using");
        expect(TokenType::IDENTIFIER, "std");
        expect(TokenType::OPERATOR, "::");
        auto u = std::make_shared<UsingDecl>();
        u->entity = advance().lexeme;  // любое: cout, endl, ...
        expect(TokenType::DELIMITER, ";");
        return u;
    }
    
    ASTPtr parseFunction() {
        auto fn = std::make_shared<FunctionDef>();
        fn->returnType = advance().lexeme;  // любой тип
        fn->name = advance().lexeme;        // любое имя
        expect(TokenType::DELIMITER, "(");
        fn->params = parseParams();
        expect(TokenType::DELIMITER, ")");
        fn->body = parseCompound();
        return fn;
    }
    
    std::vector<std::shared_ptr<ParamDecl>> parseParams() {
        std::vector<std::shared_ptr<ParamDecl>> params;
        if (cur().type == TokenType::DELIMITER && cur().lexeme == ")") return params;
        
        do {
            auto p = std::make_shared<ParamDecl>();
            p->type = advance().lexeme;   // любой тип
            p->name = advance().lexeme;   // любое имя
            params.push_back(p);
        } while (match(TokenType::DELIMITER, ","));
        return params;
    }
    
    std::shared_ptr<CompoundStmt> parseCompound() {
        auto block = std::make_shared<CompoundStmt>();
        expect(TokenType::DELIMITER, "{");
        while (!(cur().type == TokenType::DELIMITER && cur().lexeme == "}")) {
            block->statements.push_back(parseStmt());
        }
        expect(TokenType::DELIMITER, "}");
        return block;
    }
    
    ASTPtr parseStmt() {
        // short ...
        if (cur().type == TokenType::KEYWORD && cur().lexeme == "short") return parseDecl();
        // for
        if (cur().lexeme == "for") return parseFor();
        // while
        if (cur().lexeme == "while") return parseWhile();
        // if
        if (cur().lexeme == "if") return parseIf();
        // return
        if (cur().lexeme == "return") {
            advance();
            auto stmt = std::make_shared<ReturnStmt>();
            stmt->value = parseExpr();
            expect(TokenType::DELIMITER, ";");
            return stmt;
        }
        // выражение
        return parseExprStmt();
    }
    
    ASTPtr parseDecl() {
        auto stmt = std::make_shared<DeclStmt>();
        advance();  // short
        do {
            DeclStmt::Var v;
            v.name = advance().lexeme;
            expect(TokenType::OPERATOR, "=");
            v.init = parseExpr();
            stmt->vars.push_back(v);
        } while (match(TokenType::DELIMITER, ","));
        expect(TokenType::DELIMITER, ";");
        return stmt;
    }
    
    ASTPtr parseFor() {
        auto stmt = std::make_shared<ForStmt>();
        expect(TokenType::KEYWORD, "for");
        expect(TokenType::DELIMITER, "(");
        advance();  // short
        stmt->varName = advance().lexeme;
        expect(TokenType::OPERATOR, "=");
        stmt->initValue = std::stoi(advance().lexeme);
        expect(TokenType::DELIMITER, ";");
        stmt->condition = parseExpr();
        expect(TokenType::DELIMITER, ";");
        stmt->iterVar = advance().lexeme;
        expect(TokenType::OPERATOR, "++");
        expect(TokenType::DELIMITER, ")");
        stmt->body = parseCompound();
        return stmt;
    }
    
    ASTPtr parseWhile() {
        auto stmt = std::make_shared<WhileStmt>();
        advance();  // while
        expect(TokenType::DELIMITER, "(");
        stmt->condition = parseExpr();
        expect(TokenType::DELIMITER, ")");
        stmt->body = parseCompound();
        return stmt;
    }
    
    ASTPtr parseIf() {
        auto stmt = std::make_shared<IfStmt>();
        advance();  // if
        expect(TokenType::DELIMITER, "(");
        stmt->condition = parseExpr();
        expect(TokenType::DELIMITER, ")");
        stmt->thenBranch = parseCompound();
        expect(TokenType::KEYWORD, "else");
        stmt->elseBranch = parseCompound();
        return stmt;
    }
    
    ASTPtr parseExprStmt() {
        // cout << ...
        if (cur().lexeme == "cout") {
            advance();
            auto s = std::make_shared<StreamInsertExpr>();
            while (match(TokenType::OPERATOR, "<<")) {
                if (cur().lexeme == "endl") {
                    auto e = std::make_shared<Identifier>(); e->name = "endl";
                    s->values.push_back(e); advance();
                } else if (cur().type == TokenType::STRING_CONSTANT) {
                    auto str = std::make_shared<StringLiteral>(); str->value = cur().lexeme;
                    s->values.push_back(str); advance();
                } else {
                    s->values.push_back(parseExpr());
                }
            }
            expect(TokenType::DELIMITER, ";");
            auto stmt = std::make_shared<ExprStmt>(); stmt->expr = s;
            return stmt;
        }
        
        // name += expr
        if (cur().type == TokenType::IDENTIFIER && next().lexeme == "+=") {
            std::string lhs = advance().lexeme; advance();
            auto stmt = std::make_shared<AssignStmt>();
            stmt->op = "+="; stmt->lhs = lhs;
            stmt->rhs = parseExpr();
            expect(TokenType::DELIMITER, ";");
            return stmt;
        }
        
        // name++
        if (cur().type == TokenType::IDENTIFIER && next().lexeme == "++") {
            auto stmt = std::make_shared<PostIncExpr>();
            stmt->varName = advance().lexeme;
            expect(TokenType::OPERATOR, "++");
            expect(TokenType::DELIMITER, ";");
            return stmt;
        }
        
        auto expr = parseExpr();
        expect(TokenType::DELIMITER, ";");
        auto stmt = std::make_shared<ExprStmt>(); stmt->expr = expr;
        return stmt;
    }
    
    // ============ Expressions ============
    
    ASTPtr parseExpr() { return parseTernary(); }
    
    ASTPtr parseTernary() {
        ASTPtr cond = parseLogicalOr();
        if (match(TokenType::OPERATOR, "?")) {
            auto t = std::make_shared<TernaryExpr>();
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
            auto b = std::make_shared<BinaryExpr>();
            b->op = "||"; b->left = left; b->right = parseLogicalAnd();
            left = b;
        }
        return left;
    }
    
    ASTPtr parseLogicalAnd() {
        ASTPtr left = parseEquality();
        while (match(TokenType::OPERATOR, "&&")) {
            auto b = std::make_shared<BinaryExpr>();
            b->op = "&&"; b->left = left; b->right = parseEquality();
            left = b;
        }
        return left;
    }
    
    ASTPtr parseEquality() {
        ASTPtr left = parseRelational();
        while (match(TokenType::OPERATOR, "==") || match(TokenType::OPERATOR, "!=")) {
            auto b = std::make_shared<BinaryExpr>();
            b->op = tokens[pos-1].lexeme; b->left = left; b->right = parseRelational();
            left = b;
        }
        return left;
    }
    
    ASTPtr parseRelational() {
        ASTPtr left = parseAdditive();
        while (match(TokenType::OPERATOR, "<") || match(TokenType::OPERATOR, "<=") ||
               match(TokenType::OPERATOR, ">") || match(TokenType::OPERATOR, ">=")) {
            auto b = std::make_shared<BinaryExpr>();
            b->op = tokens[pos-1].lexeme; b->left = left; b->right = parseAdditive();
            left = b;
        }
        return left;
    }
    
    ASTPtr parseAdditive() {
        ASTPtr left = parseMultiplicative();
        while (match(TokenType::OPERATOR, "+") || match(TokenType::OPERATOR, "-")) {
            auto b = std::make_shared<BinaryExpr>();
            b->op = tokens[pos-1].lexeme; b->left = left; b->right = parseMultiplicative();
            left = b;
        }
        return left;
    }
    
    ASTPtr parseMultiplicative() {
        ASTPtr left = parsePostfix();
        while (match(TokenType::OPERATOR, "*") || match(TokenType::OPERATOR, "/") ||
               match(TokenType::OPERATOR, "%")) {
            auto b = std::make_shared<BinaryExpr>();
            b->op = tokens[pos-1].lexeme; b->left = left; b->right = parsePostfix();
            left = b;
        }
        return left;
    }
    
    ASTPtr parsePostfix() {
        ASTPtr expr = parsePrimary();
        if (match(TokenType::DELIMITER, "(")) {
            auto call = std::make_shared<CallExpr>();
            if (auto id = std::dynamic_pointer_cast<Identifier>(expr))
                call->callee = id->name;
            if (!(cur().type == TokenType::DELIMITER && cur().lexeme == ")"))
                call->arg = parseExpr();
            expect(TokenType::DELIMITER, ")");
            return call;
        }
        return expr;
    }
    
    ASTPtr parsePrimary() {
        if (cur().type == TokenType::IDENTIFIER) {
            auto id = std::make_shared<Identifier>();
            id->name = advance().lexeme;
            return id;
        }
        if (cur().type == TokenType::CONSTANT_INT) {
            auto lit = std::make_shared<IntLiteral>();
            lit->value = std::stoi(advance().lexeme);
            return lit;
        }
        if (cur().type == TokenType::STRING_CONSTANT) {
            auto lit = std::make_shared<StringLiteral>();
            lit->value = advance().lexeme;
            return lit;
        }
        if (match(TokenType::DELIMITER, "(")) {
            ASTPtr expr = parseExpr();
            expect(TokenType::DELIMITER, ")");
            return expr;
        }
        error("unexpected token in expression");
    }
};

// ==================== AST Printer ====================

void printAST(ASTPtr node, std::string prefix = "", bool last = true) {
    if (!node) return;
    std::string indent = prefix + (last ? "`-- " : "|-- ");
    std::string child = prefix + (last ? "    " : "|   ");
    
    if (auto p = std::dynamic_pointer_cast<Program>(node)) {
        std::cout << "Program\n";
        std::cout << child << "|-- Include: " << p->include->headerName << "\n";
        std::cout << child << "|-- Usings\n";
        for (auto& u : p->usings)
            std::cout << child << "|   |-- " << u->entity << "\n";
        std::cout << child << "`-- Functions\n";
        for (size_t i = 0; i < p->functions.size(); i++)
            printAST(p->functions[i], child + "    ", i == p->functions.size() - 1);
    }
    else if (auto f = std::dynamic_pointer_cast<FunctionDef>(node)) {
        std::cout << indent << "function: " << f->name << " -> " << f->returnType << "\n";
        if (!f->params.empty()) {
            for (auto& p : f->params)
                std::cout << child << "|-- param: " << p->name << " : " << p->type << "\n";
        }
        std::cout << child << "`-- body\n";
        for (size_t i = 0; i < f->body->statements.size(); i++)
            printAST(f->body->statements[i], child + "    ", i == f->body->statements.size() - 1);
    }
    else if (auto d = std::dynamic_pointer_cast<DeclStmt>(node)) {
        std::cout << indent << "decl\n";
        for (auto& v : d->vars) {
            std::cout << child << "|-- " << v.name;
            if (auto lit = std::dynamic_pointer_cast<IntLiteral>(v.init))
                std::cout << " = " << lit->value;
            std::cout << "\n";
        }
    }
    else if (auto f = std::dynamic_pointer_cast<ForStmt>(node)) {
        std::cout << indent << "for " << f->varName << " = " << f->initValue << " to ...\n";
        std::cout << child << "`-- body\n";
        for (size_t i = 0; i < f->body->statements.size(); i++)
            printAST(f->body->statements[i], child + "    ", i == f->body->statements.size() - 1);
    }
    else if (auto w = std::dynamic_pointer_cast<WhileStmt>(node)) {
        std::cout << indent << "while\n";
        std::cout << child << "`-- body\n";
        for (size_t i = 0; i < w->body->statements.size(); i++)
            printAST(w->body->statements[i], child + "    ", i == w->body->statements.size() - 1);
    }
    else if (auto i = std::dynamic_pointer_cast<IfStmt>(node)) {
        std::cout << indent << "if\n";
        std::cout << child << "|-- then\n";
        for (size_t j = 0; j < i->thenBranch->statements.size(); j++)
            printAST(i->thenBranch->statements[j], child + "|   ", j == i->thenBranch->statements.size() - 1);
        std::cout << child << "`-- else\n";
        for (size_t j = 0; j < i->elseBranch->statements.size(); j++)
            printAST(i->elseBranch->statements[j], child + "    ", j == i->elseBranch->statements.size() - 1);
    }
    else if (auto r = std::dynamic_pointer_cast<ReturnStmt>(node)) {
        std::cout << indent << "return\n";
    }
    else if (auto s = std::dynamic_pointer_cast<StreamInsertExpr>(node)) {
        std::cout << indent << "cout\n";
    }
    else if (auto a = std::dynamic_pointer_cast<AssignStmt>(node)) {
        std::cout << indent << a->lhs << " " << a->op << " ...\n";
    }
    else if (auto p = std::dynamic_pointer_cast<PostIncExpr>(node)) {
        std::cout << indent << p->varName << "++\n";
    }
    else if (auto c = std::dynamic_pointer_cast<CallExpr>(node)) {
        std::cout << indent << "call: " << c->callee << "\n";
    }
    else if (auto b = std::dynamic_pointer_cast<BinaryExpr>(node)) {
        std::cout << indent << "binary: " << b->op << "\n";
        printAST(b->left, child, false);
        printAST(b->right, child, true);
    }
    else if (auto t = std::dynamic_pointer_cast<TernaryExpr>(node)) {
        std::cout << indent << "ternary ?:\n";
    }
    else if (auto id = std::dynamic_pointer_cast<Identifier>(node)) {
        std::cout << indent << id->name << "\n";
    }
    else if (auto l = std::dynamic_pointer_cast<IntLiteral>(node)) {
        std::cout << indent << l->value << "\n";
    }
}

// ==================== Main ====================

int main() {
    std::cout << "=== Syntax Analyzer ===\n\n";
    
    auto tokens = loadTokens("tokens.cpp");
    if (tokens.empty()) { std::cerr << "No tokens loaded.\n"; return 1; }
    
    std::cout << "Loaded " << tokens.size() << " tokens.\n\n";
    
    Parser parser(tokens);
    try {
        auto ast = parser.parse();
        std::cout << "=== Parsing OK ===\n\n=== AST ===\n\n";
        printAST(ast);
    } catch (const std::exception& e) {
        std::cerr << "=== ERROR ===\n" << e.what() << "\n";
        return 1;
    }
    return 0;
}
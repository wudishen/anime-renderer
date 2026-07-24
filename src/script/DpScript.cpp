#include "DpScript.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace DpScript {
namespace {

enum class TokenKind {
    End,
    Number,
    Ident,
    Plus,
    Minus,
    Star,
    Slash,
    Assign,
    Dot,
    Comma,
    LParen,
    RParen,
    Return,
    Error,
};

struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    float number = 0.0f;
};

struct Value {
    enum class Kind { Number, Vec3 } kind = Kind::Number;
    float number = 0.0f;
    glm::vec3 vec{0.0f};

    static Value FromNumber(float n) {
        Value v;
        v.kind = Kind::Number;
        v.number = n;
        return v;
    }

    static Value FromVec3(const glm::vec3& p) {
        Value v;
        v.kind = Kind::Vec3;
        v.vec = p;
        return v;
    }

    bool IsNumber() const { return kind == Kind::Number; }
    bool IsVec3() const { return kind == Kind::Vec3; }
};

class Lexer {
public:
    explicit Lexer(std::string source)
        : m_Source(std::move(source)) {}

    Token Next() {
        SkipTrivia();
        if (m_Pos >= m_Source.size()) {
            return {TokenKind::End, {}, 0.0f};
        }

        const char c = m_Source[m_Pos];
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && m_Pos + 1 < m_Source.size() &&
             std::isdigit(static_cast<unsigned char>(m_Source[m_Pos + 1])))) {
            return LexNumber();
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            return LexIdent();
        }

        ++m_Pos;
        switch (c) {
        case '+': return {TokenKind::Plus, "+", 0.0f};
        case '-': return {TokenKind::Minus, "-", 0.0f};
        case '*': return {TokenKind::Star, "*", 0.0f};
        case '/': return {TokenKind::Slash, "/", 0.0f};
        case '=': return {TokenKind::Assign, "=", 0.0f};
        case '.': return {TokenKind::Dot, ".", 0.0f};
        case ',': return {TokenKind::Comma, ",", 0.0f};
        case '(': return {TokenKind::LParen, "(", 0.0f};
        case ')': return {TokenKind::RParen, ")", 0.0f};
        default:
            return {TokenKind::Error, std::string(1, c), 0.0f};
        }
    }

private:
    void SkipTrivia() {
        while (m_Pos < m_Source.size()) {
            const char c = m_Source[m_Pos];
            if (c == '#') {
                while (m_Pos < m_Source.size() && m_Source[m_Pos] != '\n') {
                    ++m_Pos;
                }
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(c))) {
                ++m_Pos;
                continue;
            }
            break;
        }
    }

    Token LexNumber() {
        const size_t start = m_Pos;
        while (m_Pos < m_Source.size() &&
               (std::isdigit(static_cast<unsigned char>(m_Source[m_Pos])) || m_Source[m_Pos] == '.')) {
            ++m_Pos;
        }
        const std::string text = m_Source.substr(start, m_Pos - start);
        return {TokenKind::Number, text, std::strtof(text.c_str(), nullptr)};
    }

    Token LexIdent() {
        const size_t start = m_Pos;
        while (m_Pos < m_Source.size()) {
            const char c = m_Source[m_Pos];
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                ++m_Pos;
                continue;
            }
            break;
        }
        const std::string text = m_Source.substr(start, m_Pos - start);
        if (text == "return") {
            return {TokenKind::Return, text, 0.0f};
        }
        return {TokenKind::Ident, text, 0.0f};
    }

    std::string m_Source;
    size_t m_Pos = 0;
};

class Parser {
public:
    Parser(std::string source, const Scene& scene)
        : m_Lex(std::move(source))
        , m_Scene(scene) {
        for (const SceneObject& object : m_Scene.GetObjects()) {
            if (object.IsMesh() && IsValidIdent(object.name)) {
                m_Meshes[object.name] = &object;
            }
        }
        Advance();
    }

    Result Run() {
        try {
            while (m_Token.kind != TokenKind::End) {
                if (m_Token.kind == TokenKind::Return) {
                    Advance();
                    const Value value = ParseExpression();
                    if (!value.IsVec3()) {
                        return Fail("return value must be a vec3 point");
                    }
                    ExpectEndOrStop();
                    return {true, value.vec, {}};
                }

                if (m_Token.kind != TokenKind::Ident) {
                    return Fail("expected assignment or return");
                }
                const std::string name = m_Token.text;
                Advance();
                if (m_Token.kind != TokenKind::Assign) {
                    return Fail("expected '=' after identifier");
                }
                Advance();
                m_Vars[name] = ParseExpression();
            }
            return Fail("script must end with 'return <point>'");
        } catch (const std::runtime_error& ex) {
            return Fail(ex.what());
        }
    }

private:
    static bool IsValidIdent(const std::string& name) {
        if (name.empty() || !(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_')) {
            return false;
        }
        for (char c : name) {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
                return false;
            }
        }
        return true;
    }

    [[noreturn]] void Error(const std::string& message) {
        throw std::runtime_error(message);
    }

    Result Fail(const std::string& message) const {
        return {false, {}, message};
    }

    void Advance() {
        m_Token = m_Lex.Next();
        if (m_Token.kind == TokenKind::Error) {
            Error("unexpected character '" + m_Token.text + "'");
        }
    }

    void ExpectEndOrStop() {
        // Allow trailing comments/whitespace only.
        if (m_Token.kind != TokenKind::End) {
            // Still ok to have more statements? Require end for clarity.
            // Allow nothing after return.
            if (m_Token.kind != TokenKind::End) {
                Error("unexpected tokens after return");
            }
        }
    }

    Value ParseExpression() {
        Value left = ParseTerm();
        while (m_Token.kind == TokenKind::Plus || m_Token.kind == TokenKind::Minus) {
            const TokenKind op = m_Token.kind;
            Advance();
            const Value right = ParseTerm();
            left = (op == TokenKind::Plus) ? Add(left, right) : Sub(left, right);
        }
        return left;
    }

    Value ParseTerm() {
        Value left = ParseUnary();
        while (m_Token.kind == TokenKind::Star || m_Token.kind == TokenKind::Slash) {
            const TokenKind op = m_Token.kind;
            Advance();
            const Value right = ParseUnary();
            left = (op == TokenKind::Star) ? Mul(left, right) : Div(left, right);
        }
        return left;
    }

    Value ParseUnary() {
        if (m_Token.kind == TokenKind::Minus) {
            Advance();
            const Value v = ParseUnary();
            if (v.IsNumber()) {
                return Value::FromNumber(-v.number);
            }
            if (v.IsVec3()) {
                return Value::FromVec3(-v.vec);
            }
            Error("unary '-' type error");
        }
        return ParsePrimary();
    }

    Value ParsePrimary() {
        if (m_Token.kind == TokenKind::Number) {
            const float n = m_Token.number;
            Advance();
            return Value::FromNumber(n);
        }

        if (m_Token.kind == TokenKind::LParen) {
            Advance();
            Value v = ParseExpression();
            if (m_Token.kind != TokenKind::RParen) {
                Error("expected ')'");
            }
            Advance();
            return v;
        }

        if (m_Token.kind == TokenKind::Ident) {
            const std::string name = m_Token.text;
            Advance();

            // Function call: name(...)
            if (m_Token.kind == TokenKind::LParen) {
                return ParseCall(name);
            }

            // name.prop or bare variable
            if (m_Token.kind == TokenKind::Dot) {
                return ParseMeshOrVarAccess(name);
            }

            const auto it = m_Vars.find(name);
            if (it == m_Vars.end()) {
                Error("unknown variable '" + name + "'");
            }
            return it->second;
        }

        Error("expected expression");
    }

    Value ParseCall(const std::string& name) {
        Advance(); // (
        std::vector<Value> args;
        if (m_Token.kind != TokenKind::RParen) {
            args.push_back(ParseExpression());
            while (m_Token.kind == TokenKind::Comma) {
                Advance();
                args.push_back(ParseExpression());
            }
        }
        if (m_Token.kind != TokenKind::RParen) {
            Error("expected ')'");
        }
        Advance();
        return EvalBuiltin(name, args);
    }

    Value ParseMeshOrVarAccess(const std::string& name) {
        // Could be var.x / var.y / var.z OR mesh.p1 / mesh.centroid
        Advance(); // .
        if (m_Token.kind != TokenKind::Ident) {
            Error("expected property name after '.'");
        }
        const std::string prop = m_Token.text;
        Advance();

        const auto varIt = m_Vars.find(name);
        if (varIt != m_Vars.end()) {
            if (!varIt->second.IsVec3()) {
                Error("'" + name + "' has no components");
            }
            if (prop == "x") {
                return Value::FromNumber(varIt->second.vec.x);
            }
            if (prop == "y") {
                return Value::FromNumber(varIt->second.vec.y);
            }
            if (prop == "z") {
                return Value::FromNumber(varIt->second.vec.z);
            }
            Error("unknown component '" + prop + "'");
        }

        const auto meshIt = m_Meshes.find(name);
        if (meshIt == m_Meshes.end()) {
            Error("unknown mesh or variable '" + name + "'");
        }
        return EvalMeshProperty(*meshIt->second, prop);
    }

    static glm::vec3 WorldVertex(const SceneObject& mesh, int index) {
        return glm::vec3(mesh.transform * glm::vec4(mesh.vertices[static_cast<size_t>(index)], 1.0f));
    }

    static glm::vec3 WorldCentroid(const SceneObject& mesh) {
        glm::vec3 sum(0.0f);
        for (const glm::vec3& v : mesh.vertices) {
            sum += v;
        }
        const glm::vec3 local = sum / static_cast<float>(mesh.vertices.size());
        return glm::vec3(mesh.transform * glm::vec4(local, 1.0f));
    }

    Value EvalMeshProperty(const SceneObject& mesh, const std::string& prop) {
        if (mesh.vertices.empty()) {
            Error("mesh '" + mesh.name + "' has no vertices");
        }
        if (prop == "centroid") {
            return Value::FromVec3(WorldCentroid(mesh));
        }
        if (prop == "count") {
            return Value::FromNumber(static_cast<float>(mesh.vertices.size()));
        }

        // p1, p2, ... (1-based) or v0, v1, ... (0-based)
        if ((prop[0] == 'p' || prop[0] == 'v') && prop.size() > 1) {
            bool ok = true;
            int index = 0;
            for (size_t i = 1; i < prop.size(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(prop[i]))) {
                    ok = false;
                    break;
                }
                index = index * 10 + (prop[i] - '0');
            }
            if (ok) {
                if (prop[0] == 'p') {
                    index -= 1; // 1-based
                }
                if (index < 0 || index >= static_cast<int>(mesh.vertices.size())) {
                    Error("vertex index out of range on mesh '" + mesh.name + "'");
                }
                return Value::FromVec3(WorldVertex(mesh, index));
            }
        }

        Error("unknown mesh property '" + mesh.name + "." + prop + "'");
    }

    Value EvalBuiltin(const std::string& name, const std::vector<Value>& args) {
        if (name == "vec") {
            if (args.size() != 3 || !args[0].IsNumber() || !args[1].IsNumber() || !args[2].IsNumber()) {
                Error("vec(x,y,z) expects three numbers");
            }
            return Value::FromVec3({args[0].number, args[1].number, args[2].number});
        }
        if (name == "avg") {
            if (args.empty()) {
                Error("avg() needs at least one argument");
            }
            glm::vec3 sum(0.0f);
            for (const Value& a : args) {
                if (!a.IsVec3()) {
                    Error("avg() expects vec3 arguments");
                }
                sum += a.vec;
            }
            return Value::FromVec3(sum / static_cast<float>(args.size()));
        }
        if (name == "len") {
            if (args.size() != 1 || !args[0].IsVec3()) {
                Error("len(v) expects one vec3");
            }
            return Value::FromNumber(glm::length(args[0].vec));
        }
        if (name == "normalize") {
            if (args.size() != 1 || !args[0].IsVec3()) {
                Error("normalize(v) expects one vec3");
            }
            const float l = glm::length(args[0].vec);
            if (l < 1e-8f) {
                Error("normalize() of zero-length vector");
            }
            return Value::FromVec3(args[0].vec / l);
        }
        if (name == "dot") {
            if (args.size() != 2 || !args[0].IsVec3() || !args[1].IsVec3()) {
                Error("dot(a,b) expects two vec3");
            }
            return Value::FromNumber(glm::dot(args[0].vec, args[1].vec));
        }
        if (name == "cross") {
            if (args.size() != 2 || !args[0].IsVec3() || !args[1].IsVec3()) {
                Error("cross(a,b) expects two vec3");
            }
            return Value::FromVec3(glm::cross(args[0].vec, args[1].vec));
        }
        Error("unknown function '" + name + "'");
    }

    Value Add(const Value& a, const Value& b) {
        if (a.IsVec3() && b.IsVec3()) {
            return Value::FromVec3(a.vec + b.vec);
        }
        if (a.IsNumber() && b.IsNumber()) {
            return Value::FromNumber(a.number + b.number);
        }
        Error("type error in '+'");
    }

    Value Sub(const Value& a, const Value& b) {
        if (a.IsVec3() && b.IsVec3()) {
            return Value::FromVec3(a.vec - b.vec);
        }
        if (a.IsNumber() && b.IsNumber()) {
            return Value::FromNumber(a.number - b.number);
        }
        Error("type error in '-'");
    }

    Value Mul(const Value& a, const Value& b) {
        if (a.IsVec3() && b.IsNumber()) {
            return Value::FromVec3(a.vec * b.number);
        }
        if (a.IsNumber() && b.IsVec3()) {
            return Value::FromVec3(a.number * b.vec);
        }
        if (a.IsNumber() && b.IsNumber()) {
            return Value::FromNumber(a.number * b.number);
        }
        Error("type error in '*'");
    }

    Value Div(const Value& a, const Value& b) {
        if (a.IsVec3() && b.IsNumber()) {
            if (std::fabs(b.number) < 1e-12f) {
                Error("division by zero");
            }
            return Value::FromVec3(a.vec / b.number);
        }
        if (a.IsNumber() && b.IsNumber()) {
            if (std::fabs(b.number) < 1e-12f) {
                Error("division by zero");
            }
            return Value::FromNumber(a.number / b.number);
        }
        Error("type error in '/'");
    }

    Lexer m_Lex;
    const Scene& m_Scene;
    Token m_Token;
    std::unordered_map<std::string, Value> m_Vars;
    std::unordered_map<std::string, const SceneObject*> m_Meshes;
};

} // namespace

Result Evaluate(const std::string& source, const Scene& scene) {
    Parser parser(source, scene);
    return parser.Run();
}

} // namespace DpScript

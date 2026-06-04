#include "sdf_eval.h"
#include <cctype>
#include <cstdlib>

namespace mg {

struct ExprParser {
    const char* s;
    size_t len;
    size_t pos;
    f32 x, y, z, w;

    ExprParser(const char* s, size_t len, f32 x, f32 y, f32 z, f32 w)
        : s(s), len(len), pos(0), x(x), y(y), z(z), w(w) {}

    char peek() { return pos < len ? s[pos] : '\0'; }
    char advance() { return pos < len ? s[pos++] : '\0'; }
    void skipWs() { while (pos < len && std::isspace(s[pos])) pos++; }

    f32 parse() {
        skipWs();
        f32 r = expr();
        return r;
    }

    f32 expr() {
        f32 r = term();
        skipWs();
        while (peek() == '+' || peek() == '-') {
            char op = advance(); skipWs(); f32 rhs = term();
            r = (op == '+') ? r + rhs : r - rhs;
        }
        return r;
    }

    f32 term() {
        f32 r = unary();
        skipWs();
        while (peek() == '*' || peek() == '/') {
            char op = advance(); skipWs(); f32 rhs = unary();
            r = (op == '*') ? r * rhs : r / rhs;
        }
        return r;
    }

    f32 unary() {
        skipWs();
        if (peek() == '-') { advance(); return -unary(); }
        if (peek() == '+') { advance(); return unary(); }
        return power();
    }

    f32 power() {
        f32 r = atom();
        skipWs();
        if (peek() == '^') { advance(); skipWs(); r = std::pow(r, unary()); }
        return r;
    }

    f32 atom() {
        skipWs();
        char c = peek();
        if (c == '(') {
            advance(); f32 r = expr(); skipWs();
            if (peek() == ')') advance();
            return r;
        }
        if (std::isdigit(c) || c == '.') {
            std::string val;
            if (peek() == '-') val += advance();
            while (pos < len && (std::isdigit(s[pos]) || s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E' ||
                   (s[pos] == '+' || s[pos] == '-') && pos > 0 && (s[pos-1]=='e'||s[pos-1]=='E')))
                val += advance();
            return val.empty() ? 0.0f : (f32)std::atof(val.c_str());
        }
        if (std::isalpha(c) || c == '_') {
            std::string name;
            while (pos < len && (std::isalnum(s[pos]) || s[pos] == '_')) name += advance();

            if (name == "pi") return 3.14159265f;
            if (name == "e")  return 2.71828183f;
            if (name == "x") return x;
            if (name == "y") return y;
            if (name == "z") return z;
            if (name == "w" || name == "t") return w;

            skipWs();
            bool paren = (peek() == '(');
            if (paren) advance();

            auto arg = [&]() -> f32 { skipWs(); return expr(); };
            auto arg2 = [&]() -> f32 { skipWs(); if (peek() == ',') advance(); skipWs(); return expr(); };
            auto arg3 = [&]() -> f32 { skipWs(); if (peek() == ',') advance(); skipWs(); return expr(); };

            if (name == "abs")   { f32 a = arg(); if (paren && peek()==')') advance(); return std::abs(a); }
            if (name == "sin")   { f32 a = arg(); if (paren && peek()==')') advance(); return std::sin(a); }
            if (name == "cos")   { f32 a = arg(); if (paren && peek()==')') advance(); return std::cos(a); }
            if (name == "tan")   { f32 a = arg(); if (paren && peek()==')') advance(); return std::tan(a); }
            if (name == "sqrt")  { f32 a = arg(); if (paren && peek()==')') advance(); return std::sqrt(a); }
            if (name == "floor") { f32 a = arg(); if (paren && peek()==')') advance(); return std::floor(a); }
            if (name == "ceil")  { f32 a = arg(); if (paren && peek()==')') advance(); return std::ceil(a); }
            if (name == "length") {
                f32 a = arg(); skipWs();
                if (peek() == ',') { advance(); f32 b = arg2(); f32 c = arg3(); if (paren) advance(); return std::sqrt(a*a+b*b+c*c); }
                if (paren && peek()==')') advance(); return std::abs(a);
            }
            if (name == "max")   { f32 a = arg(); f32 b = arg2(); if (paren && peek()==')') advance(); return std::fmax(a,b); }
            if (name == "min")   { f32 a = arg(); f32 b = arg2(); if (paren && peek()==')') advance(); return std::fmin(a,b); }
            if (name == "pow")   { f32 a = arg(); f32 b = arg2(); if (paren && peek()==')') advance(); return std::pow(a,b); }
            if (name == "clamp") { f32 a = arg(); f32 b = arg2(); f32 c = arg3(); if (paren) advance(); return std::fmax(a, std::fmin(c, b)); }
            if (name == "mix" || name == "lerp") { f32 a = arg(); f32 b = arg2(); f32 c = arg3(); if (paren) advance(); return a + (b-a)*c; }
            if (name == "dot")   { f32 a = arg(); f32 b = arg2(); f32 c = arg3(); f32 d = arg3(); if (paren) advance(); return a*b + c*d; }

            if (paren) { while (peek() != ')' && pos < len) advance(); if (peek() == ')') advance(); }
            return 0.0f;
        }
        return 0.0f;
    }
};

f32 evalExpr(const char* expr, size_t len, f32 x, f32 y, f32 z, f32 w) {
    if (!expr || len == 0) return 0.0f;
    ExprParser p(expr, len, x, y, z, w);
    return p.parse();
}

} // namespace mg

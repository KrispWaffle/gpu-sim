#include "parser.hpp"
#include "config.hpp"
#include <sstream>
#include <cctype>
#include <algorithm>

namespace{

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b-1])) b--;
    return s.substr(a, b - a);
}

std::string stripComment(const std::string& s) {
    auto pos = s.find(';');
    return pos == std::string::npos ? s : s.substr(0, pos);
}


std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> toks;
    std::string cur;
    auto flush = [&]() { if (!cur.empty()) { toks.push_back(cur); cur.clear(); } };
    for (char c : line) {
        if (std::isspace((unsigned char)c) || c == ',') {
            flush();
        } else if (c == '@') {
            flush();
            toks.push_back("@");
        } else {
            cur += c;
        }
    }

    flush();
    return toks;
}

bool tryParseNumber(const std::string& s, float& out) {
    if (s.empty()) return false;
    size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (i >= s.size()) return false;
    bool hasDigit = false, hasDot = false;
    for (; i < s.size(); i++) {
        if (std::isdigit((unsigned char)s[i])) hasDigit = true;
        else if (s[i] == '.' && !hasDot) hasDot = true;
        else return false;
    }
    if (!hasDigit) return false;
    try { out = std::stof(s); return true; }
    catch (...) { return false; }
}

Operand parseOperand(const std::string& tok) {
    float n;
    if (tryParseNumber(tok, n)) return n;
    return tok;
}

StoreLoc parseStoreLoc(const std::string& tok, int ln) {
    auto t = toLower(tok);
    if (t == "shared") return StoreLoc::SHARED;
    if (t == "global") return StoreLoc::GLOBAL;
    if (t == "local")  return StoreLoc::LOCAL;
    throw ParseError(ln, "expected shared|global|local, got '" + tok + "'");
}

int parseRegOrOffset(const std::string& tok, int ln) {
    if (tok.empty()) throw ParseError(ln, "empty offset");
    std::string s = tok;
    if (s[0] == 'r' || s[0] == 'R') s = s.substr(1);
    try { return std::stoi(s); }
    catch (...) { throw ParseError(ln, "bad offset '" + tok + "'"); }
}

bool isOpcode(const std::string& tok) {
    static const std::vector<std::string> ops = {
        "add","sub","mul","div","neg","ld","st","mov",
        "halt","jmp","cmp_lt","and","or","xor"
    };
    auto t = toLower(tok);
    return std::find(ops.begin(), ops.end(), t) != ops.end();
}

Opcode parseOpcode(const std::string& tok, int ln) {
    auto t = toLower(tok);
    if (t == "add")    return Opcode::ADD;
    if (t == "sub")    return Opcode::SUB;
    if (t == "mul")    return Opcode::MUL;
    if (t == "div")    return Opcode::DIV;
    if (t == "neg")    return Opcode::NEG;
    if (t == "ld")     return Opcode::LD;
    if (t == "st")     return Opcode::ST;
    if (t == "mov")    return Opcode::MOV;
    if (t == "halt")   return Opcode::HALT;
    if (t == "jmp")    return Opcode::JMP;
    if (t == "cmp_lt") return Opcode::CMP_LT;
    if (t == "and")    return Opcode::AND;
    if (t == "or")     return Opcode::OR;
    if (t == "xor")    return Opcode::XOR;
    throw ParseError(ln, "unknown opcode '" + tok + "'");
}

// def <loc> <name> = <value> [tidx] [@<regOrOffset>]
Instr parseDef(const std::vector<std::string>& toks, int ln) {
    if (toks.size() < 5)
        throw ParseError(ln, "def <loc> <name> = <value> [tidx] [@rN]");
    StoreLoc loc = parseStoreLoc(toks[1], ln);
    const std::string& name = toks[2];
    if (toks[3] != "=") throw ParseError(ln, "def: expected '=' after name");
    float val;
    if (!tryParseNumber(toks[4], val))
        throw ParseError(ln, "def: expected number, got '" + toks[4] + "'");

    bool threadIdx = false;
    int offset = 0;
    for (size_t i = 5; i < toks.size(); i++) {
        auto t = toLower(toks[i]);
        if (t == "tidx") {
            threadIdx = true;
        } else if (t == "@") {
            if (i + 1 >= toks.size())
                throw ParseError(ln, "def: '@' needs a register or offset");
            offset = parseRegOrOffset(toks[i + 1], ln);
            i++;
        } else {
            throw ParseError(ln, "def: unexpected token '" + toks[i] + "'");
        }
    }
    Variable v{name, val, offset, /*isConstant*/false, threadIdx, loc};
    return Instr{Opcode::DEF, { v }};
}

}  // namespace

Program parseProgram(const std::string& source) {
    Program prog;

    struct Line {
        int ln;
        std::vector<std::string> toks;
        bool isLabel;
        std::string label;
    };

    std::vector<Line> lines;
    {
        std::istringstream in(source);
        std::string raw;
        int ln = 0;
        while (std::getline(in, raw)) {
            ln++;
            std::string s = trim(stripComment(raw));
            if (s.empty()) continue;
            auto toks = tokenize(s);

            if (toks.empty()) continue; 
            
            if (toks.size() == 1 && toks[0].size() > 1 && toks[0].back() == ':') {
                std::string name = toks[0].substr(0, toks[0].size() - 1);
                if (name.empty()) throw ParseError(ln, "empty label name");
                lines.push_back({ln, {}, true, name});
            } else {
                lines.push_back({ln, std::move(toks), false, ""});
            }
        }
    }

    int instrIndex = 0;
    for (auto& pl : lines) {
        if (pl.isLabel) {
            if (prog.labels.count(pl.label))
                throw ParseError(pl.ln, "duplicate label '" + pl.label + "'");
            prog.labels[pl.label] = instrIndex;
        } else {
            instrIndex++;
        }
    }

    for (auto& pl : lines) {
        if (pl.isLabel) continue;
        const auto& toks = pl.toks;
        auto head = toLower(toks[0]);

        if (head == "def") {
            prog.instructions.push_back(parseDef(toks, pl.ln));
            continue;
        }
        if (!isOpcode(toks[0]))
            throw ParseError(pl.ln, "unknown opcode/keyword '" + toks[0] + "'");

        Instr instr{parseOpcode(toks[0], pl.ln), {}};
        for (size_t i = 1; i < toks.size(); i++)
            instr.src.push_back(parseOperand(toks[i]));
        prog.instructions.push_back(instr);
    }

    return prog;
}
std::vector<std::string> tokenizerView(const std::string& source)

{
    return tokenize(source);
}
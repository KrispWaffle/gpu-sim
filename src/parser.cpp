#include "parser.hpp"
#include "config.hpp"
#include <sstream>
#include <cctype>
#include <algorithm>
#include <cerrno>
#include <cstdlib>

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

bool allDigits(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(),
        [](unsigned char c) { return std::isdigit(c); });
}

bool tryParseNumber(const std::string& s, float& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    out = std::strtof(s.c_str(), &end);
    return end != s.c_str() && end == s.c_str() + s.size() && errno != ERANGE;
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
    if (!allDigits(s)) throw ParseError(ln, "bad offset '" + tok + "'");
    try { return std::stoi(s); }
    catch (...) { throw ParseError(ln, "bad offset '" + tok + "'"); }
}

struct OpSpec { Opcode op; size_t arity; };

const std::unordered_map<std::string, OpSpec>& opcodeTable() {
    static const std::unordered_map<std::string, OpSpec> table = {
        {"add",    {Opcode::ADD,    3}},
        {"sub",    {Opcode::SUB,    3}},
        {"mul",    {Opcode::MUL,    3}},
        {"div",    {Opcode::DIV,    3}},
        {"and",    {Opcode::AND,    3}},
        {"or",     {Opcode::OR,     3}},
        {"xor",    {Opcode::XOR,    3}},
        {"neg",    {Opcode::NEG,    2}},
        {"ld",     {Opcode::LD,     2}},
        {"st",     {Opcode::ST,     2}},
        {"mov",    {Opcode::MOV,    2}},
        {"cmp_lt", {Opcode::CMP_LT, 2}},
        {"cmp_eq", {Opcode::CMP_EQ, 2}},
        {"cmp_gt", {Opcode::CMP_GT, 2}},
        {"jmp",    {Opcode::JMP,    1}},
        {"jmpu",   {Opcode::JMPU,   1}},
        {"bar",    {Opcode::BAR,    0}},
        {"halt",   {Opcode::HALT,   0}},
    };
    return table;
}


void checkIndex(const std::string& tok, size_t prefixLen, int limit,
                const char* what, int ln) {
    std::string num = tok.substr(prefixLen);
    if (!allDigits(num)) return;
    int idx = -1;
    try { idx = std::stoi(num); } catch (...) {}
    if (idx < 0 || idx >= limit)
        throw ParseError(ln, std::string(what) + " index out of range in '" + tok
                         + "' (max " + std::to_string(limit - 1) + ")");
}

// gm[x] / sm[x] — x is a register, variable, tidx, or literal index
Operand parseMemRef(const std::string& tok, int ln, const SimConfig& cfg);

void validateOperand(const std::string& tok, int ln, const SimConfig& cfg) {
    if (tok.size() > 1 && tok[0] == 'r')
        checkIndex(tok, 1, cfg.numRegisters, "register", ln);
    else if (tok.size() > 2 && tok.compare(0, 2, "gm") == 0)
        checkIndex(tok, 2, cfg.globalMemSize, "global memory", ln);
    else if (tok.size() > 2 && tok.compare(0, 2, "sm") == 0)
        checkIndex(tok, 2, cfg.warpSize, "shared memory", ln);
}

Operand parseMemRef(const std::string& tok, int ln, const SimConfig& cfg) {
    auto open = tok.find('[');
    if (open == std::string::npos || tok.size() < open + 3 || tok.back() != ']')
        throw ParseError(ln, "malformed memory reference '" + tok + "'");
    std::string prefix = toLower(tok.substr(0, open));
    std::string inner = tok.substr(open + 1, tok.size() - open - 2);

    StoreLoc space;
    int limit;
    const char* what;
    if (prefix == "gm")      { space = StoreLoc::GLOBAL; limit = cfg.globalMemSize; what = "global memory"; }
    else if (prefix == "sm") { space = StoreLoc::SHARED; limit = cfg.warpSize;      what = "shared memory"; }
    else throw ParseError(ln, "memory reference must be gm[...] or sm[...], got '" + tok + "'");

    if (allDigits(inner))
        checkIndex(inner, 0, limit, what, ln);
    else
        validateOperand(inner, ln, cfg);
    return MemRef{space, inner};
}

// def <loc> <name> = <value> [tidx] [@<regOrOffset>]
Instr parseDef(const std::vector<std::string>& toks, int ln, const SimConfig& cfg) {
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
    int limit = loc == StoreLoc::LOCAL  ? cfg.numRegisters
              : loc == StoreLoc::SHARED ? cfg.warpSize
              : cfg.globalMemSize;
    if (offset >= limit)
        throw ParseError(ln, "def: offset " + std::to_string(offset)
                         + " out of range (max " + std::to_string(limit - 1) + ")");
    Variable v{name, val, offset, /*isConstant*/false, threadIdx, loc};
    return Instr{Opcode::DEF, { v }, ln};
}

}  // namespace

Program parseProgram(const std::string& source, const SimConfig& cfg) {
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
            
            if (toks[0].size() > 1 && toks[0].back() == ':') {
                if (toks.size() > 1)
                    throw ParseError(ln, "label '" + toks[0] + "' must be on its own line");
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
            prog.instructions.push_back(parseDef(toks, pl.ln, cfg));
            continue;
        }

        auto it = opcodeTable().find(head);
        if (it == opcodeTable().end())
            throw ParseError(pl.ln, "unknown opcode/keyword '" + toks[0] + "'");
        const OpSpec& spec = it->second;

        if (toks.size() - 1 != spec.arity)
            throw ParseError(pl.ln, "'" + head + "' expects "
                             + std::to_string(spec.arity) + " operand(s), got "
                             + std::to_string(toks.size() - 1));

        Instr instr{spec.op, {}, pl.ln};
        if (spec.op == Opcode::JMP || spec.op == Opcode::JMPU) {
            float n;
            if (tryParseNumber(toks[1], n))
                throw ParseError(pl.ln, head + ": expected label name, got '" + toks[1] + "'");
            if (!prog.labels.count(toks[1]))
                throw ParseError(pl.ln, head + ": unknown label '" + toks[1] + "'");
            instr.src.push_back(toks[1]);
        } else {
            for (size_t i = 1; i < toks.size(); i++) {
                if (toks[i].find('[') != std::string::npos) {
                    instr.src.push_back(parseMemRef(toks[i], pl.ln, cfg));
                    continue;
                }
                Operand op = parseOperand(toks[i]);
                if (std::holds_alternative<std::string>(op))
                    validateOperand(toks[i], pl.ln, cfg);
                instr.src.push_back(std::move(op));
            }
        }
        prog.instructions.push_back(instr);
    }

    return prog;
}

std::vector<std::string> tokenizerView(const std::string& source)
{
    return tokenize(source);
}
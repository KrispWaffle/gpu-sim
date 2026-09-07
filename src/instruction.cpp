#include "instruction.hpp"
#include "execution.hpp"
#include "gpu.hpp"
#include <iostream>
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <cmath>
#include <limits>

int getRegisterName(const std::string& reg)
{
    if (reg.length() > 1 && std::isalpha(static_cast<unsigned char>(reg[0])))
    {
        std::string num = reg.substr(1);
        if (num == "TIDX") return TIDX_RETURN_VAL;
        try { return std::stoi(num); }
        catch (...) { return -2; }
    }
    return -2;
}

int getMemoryLocation(const std::string& mem){
    if (mem.size() < 2) return -2;
    std::string num = mem.substr(2);
    if (num == "TIDX") return TIDX_RETURN_VAL;
    try { return std::stoi(num); }
    catch (...) { return -2; }
}

OpInfo decodeOperand(const Operand& op, ExecutionContext& ctx) {
    int tid = ctx.thread.id();
   
    int lane = tid % (int)ctx.warp.memory.size();

    if (auto pf = std::get_if<float>(&op)) {
        return { OpKind::Constant, *pf, 0, {} };
    }
    if (auto pi = std::get_if<int>(&op)) {
        return { OpKind::Constant, static_cast<float>(*pi), 0, {} };
    }

    if (auto pm = std::get_if<MemRef>(&op)) {
        const std::string& t = pm->idxTok;
        char* end = nullptr;
        float lit = std::strtof(t.c_str(), &end);
        Operand inner;
        if (end && end != t.c_str() && *end == '\0') inner = lit;
        else                                         inner = t;
        OpInfo idxInfo = decodeOperand(inner, ctx);
        if (idxInfo.kind == OpKind::Invalid)
            return { OpKind::Invalid, 0.0f, 0, {} };
        float value = fetch(idxInfo, ctx);
        if (!std::isfinite(value) || value < 0 ||
            static_cast<double>(value) > std::numeric_limits<int>::max())
            return { OpKind::Invalid, 0.0f, 0, {} };
        int idx = static_cast<int>(value);
        size_t limit = pm->space == StoreLoc::GLOBAL ? ctx.globalMem.size()
                                                     : ctx.warp.memory.size();
        if (idx < 0 || (size_t)idx >= limit)
            return { OpKind::Invalid, 0.0f, 0, {} };
        OpKind kind = pm->space == StoreLoc::GLOBAL ? OpKind::Global : OpKind::Shared;
        return { kind, 0.0f, static_cast<size_t>(idx), {} };
    }

    if (auto ps = std::get_if<std::string>(&op)) {
        const std::string& s = *ps;

        if (s == "tidx" || s == "TIDX") {
            return { OpKind::Constant, static_cast<float>(tid), 0, {} };
        }

        if (s.size() > 1 && s[0] == 'r') {
            int r = getRegisterName(s);
            if (r == TIDX_RETURN_VAL) r = lane;
            if (r >= 0 && r < (int)ctx.thread._registers.size())
                return { OpKind::Register, 0.0f, static_cast<size_t>(r), {} };
            if (r >= 0) return { OpKind::Invalid, 0.0f, 0, {} };
        }
        if (s.size() > 2 && s.substr(0,2) == "gm") {
            int g = getMemoryLocation(s);
            if (g == TIDX_RETURN_VAL) g = tid;
            if (g >= 0 && g < (int)ctx.globalMem.size())
                return { OpKind::Global, 0.0f, static_cast<size_t>(g), {} };
            if (g >= 0) return { OpKind::Invalid, 0.0f, 0, {} };
        }
        if (s.size() > 2 && s.substr(0,2) == "sm") {
            int f = getMemoryLocation(s);
            if (f == TIDX_RETURN_VAL) f = lane;
            if (f >= 0 && f < (int)ctx.warp.memory.size())
                return { OpKind::Shared, 0.0f, static_cast<size_t>(f), {} };
            if (f >= 0) return { OpKind::Invalid, 0.0f, 0, {} };
        }

        if (auto ov = ctx.vars.getVar(s)) {
            Variable v = *ov;
            int addr = v.threadIDX ? (v.loc == StoreLoc::GLOBAL ? tid : lane) : v.offset;
            return { OpKind::Variable, v.value, static_cast<size_t>(addr), std::move(v) };
        }
    }

    return { OpKind::Invalid, 0.0f, 0, {} };
}

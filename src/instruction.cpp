#include "instruction.hpp"
#include "execution.hpp"
#include "gpu.hpp"
#include <iostream>
#include <cctype>
#include <stdexcept>

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

    if (auto pf = std::get_if<float>(&op)) {
        return { OpKind::Constant, *pf, 0, {} };
    }
    if (auto pi = std::get_if<int>(&op)) {
        return { OpKind::Constant, static_cast<float>(*pi), 0, {} };
    }

    if (auto ps = std::get_if<std::string>(&op)) {
        const std::string& s = *ps;

        if (s == "tidx" || s == "TIDX") {
            return { OpKind::Constant, static_cast<float>(tid), 0, {} };
        }

        if (s.size() > 1 && s[0] == 'r') {
            int r = getRegisterName(s);
            if (r == TIDX_RETURN_VAL)            return { OpKind::Register, 0.0f, static_cast<size_t>(tid), {} };
            if (r >= 0 && r < NUM_REGISTERS)     return { OpKind::Register, 0.0f, static_cast<size_t>(r),   {} };
        }
        if (s.size() > 2 && s.substr(0,2) == "gm") {
            int g = getMemoryLocation(s);
            if (g == TIDX_RETURN_VAL)            return { OpKind::Global, 0.0f, static_cast<size_t>(tid), {} };
            if (g >= 0 && g < GLOBAL_MEM_SIZE)   return { OpKind::Global, 0.0f, static_cast<size_t>(g),   {} };
        }
        if (s.size() > 2 && s.substr(0,2) == "sm") {
            int f = getMemoryLocation(s);
            if (f == TIDX_RETURN_VAL)            return { OpKind::Shared, 0.0f, static_cast<size_t>(tid), {} };
            if (f >= 0 && f < WARP_SIZE)         return { OpKind::Shared, 0.0f, static_cast<size_t>(f),   {} };
        }

        if (auto ov = ctx.vars.getVar(s)) {
            Variable v = *ov;
            int addr = v.threadIDX ? tid : v.offset;
            return { OpKind::Variable, v.value, static_cast<size_t>(addr), std::move(v) };
        }
    }

    return { OpKind::Invalid, 0.0f, 0, {} };
}

#include "execution.hpp"
#include <iostream>
#include <stdexcept>

float fetch(const OpInfo& o, const ExecutionContext& ctx) {
    switch (o.kind) {
        case OpKind::Constant: return o.constVal;
        case OpKind::Register: return ctx.thread._registers[o.index];
        case OpKind::Global: return ctx.globalMem[o.index];
        case OpKind::Shared: return ctx.warp.memory[o.index];
        case OpKind::Variable:
            switch (o.var.loc) {
                case StoreLoc::GLOBAL: return ctx.globalMem[o.index];
                case StoreLoc::SHARED: return ctx.warp.memory[o.index];
                case StoreLoc::LOCAL: return ctx.thread._registers[o.index];
            }
        default:
            std::cerr << "ERROR in fetch: unsupported operand kind\n";
            throw std::runtime_error("fetch error");
    }
}

float eval(const OpInfo& lhs, const OpInfo& rhs, Opcode op, const ExecutionContext& ctx) {
    float a = fetch(lhs, ctx);
    float b = fetch(rhs, ctx);

    switch (op) {
        case Opcode::ADD: return a + b;
        case Opcode::SUB: return a - b;
        case Opcode::MUL: return a * b;
        case Opcode::DIV:
            if (b == 0.0f) throw std::runtime_error("DIV by zero");
            return a / b;
        case Opcode::MOV:
            return b;
        case Opcode::NEG:
            return b*-1;
        default:
            throw std::runtime_error("eval unsupported opcode");
    }
}

ErrorCode storeInLocation(OpInfo& dst, float result, ExecutionContext& ctx) {
    switch (dst.kind) {
        case OpKind::Register:
            ctx.thread._registers[dst.index] = result;
            break;
        case OpKind::Global:
            ctx.globalMem[dst.index] = result;
            break;
        case OpKind::Shared:
            ctx.warp.memory[dst.index] = result;
            break;
        case OpKind::Variable:
            switch (dst.var.loc) {
                case StoreLoc::GLOBAL: ctx.globalMem[dst.index] = result; break;
                case StoreLoc::SHARED: ctx.warp.memory[dst.index] = result; break;
                case StoreLoc::LOCAL: ctx.thread._registers[dst.index] = result; break;
            }
            break;
        default:
            std::cerr << "ERROR in storing result\n";
            return ErrorCode::InvalidMemorySpace;
    }
    return ErrorCode::None;
}

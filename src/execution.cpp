#include "execution.hpp"
#include <iostream>
#include <stdexcept>

float fetch(const OpInfo& o, const ExecutionContext& ctx) {
    switch (o.kind) {
        case OpKind::Constant: return o.constVal;
        case OpKind::Register: return ctx.thread._registers.at(o.index);
        case OpKind::Global:   return ctx.globalMem.at(o.index);
        case OpKind::Shared:   return ctx.warp.memory.at(o.index);
        case OpKind::Variable:
            switch (o.var.loc) {
                case StoreLoc::GLOBAL: return ctx.globalMem.at(o.index);
                case StoreLoc::SHARED: return ctx.warp.memory.at(o.index);
                case StoreLoc::LOCAL:  return ctx.thread._registers.at(o.index);
            }
            throw std::runtime_error("fetch: variable has invalid StoreLoc");
        case OpKind::Invalid:
        default:
            throw std::runtime_error("fetch: invalid operand");
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
        case Opcode::MOV: return b;
        case Opcode::NEG: return -b;
        case Opcode::XOR: return static_cast<float>(static_cast<int>(a) ^ static_cast<int>(b));
        case Opcode::OR:  return static_cast<float>(static_cast<int>(a) | static_cast<int>(b));
        case Opcode::AND: return static_cast<float>(static_cast<int>(a) & static_cast<int>(b));
        default:
            throw std::runtime_error("eval unsupported opcode");
    }
}

ErrorCode storeInLocation(const OpInfo& dst, float result, ExecutionContext& ctx) {
    switch (dst.kind) {
        case OpKind::Register:
            if (dst.index >= ctx.thread._registers.size())
                return ErrorCode::InvalidMemorySpace;
            ctx.thread._registers[dst.index] = result;
            return ErrorCode::None;
        case OpKind::Global:
            if (dst.index >= ctx.globalMem.size())
                return ErrorCode::GlobalOutOfBounds;
            ctx.globalMem[dst.index] = result;
            return ErrorCode::None;
        case OpKind::Shared:
            if (dst.index >= ctx.warp.memory.size())
                return ErrorCode::SharedOutOfBounds;
            ctx.warp.memory[dst.index] = result;
            return ErrorCode::None;
        case OpKind::Variable:
            switch (dst.var.loc) {
                case StoreLoc::GLOBAL:
                    if (dst.index >= ctx.globalMem.size())
                        return ErrorCode::GlobalOutOfBounds;
                    ctx.globalMem[dst.index] = result;
                    return ErrorCode::None;
                case StoreLoc::SHARED:
                    if (dst.index >= ctx.warp.memory.size())
                        return ErrorCode::SharedOutOfBounds;
                    ctx.warp.memory[dst.index] = result;
                    return ErrorCode::None;
                case StoreLoc::LOCAL:
                    if (dst.index >= ctx.thread._registers.size())
                        return ErrorCode::InvalidMemorySpace;
                    ctx.thread._registers[dst.index] = result;
                    return ErrorCode::None;
            }
            return ErrorCode::InvalidMemorySpace;
        case OpKind::Constant:
        case OpKind::Invalid:
        default:
            return ErrorCode::InvalidMemorySpace;
    }
}

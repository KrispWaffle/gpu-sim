#include "operations.hpp"
#include "execution.hpp"
#include "vartable.hpp"
#include "labeltable.hpp"
#include <iostream>
#include <string>

std::array<HandlerFn, 24> opcode_handlers;

void setup_opcode_handlers()
{
    opcode_handlers[static_cast<int>(Opcode::ADD)]    = _binary_;
    opcode_handlers[static_cast<int>(Opcode::SUB)]    = _binary_;
    opcode_handlers[static_cast<int>(Opcode::MUL)]    = _binary_;
    opcode_handlers[static_cast<int>(Opcode::DIV)]    = _binary_;
    opcode_handlers[static_cast<int>(Opcode::AND)]    = _binary_;
    opcode_handlers[static_cast<int>(Opcode::OR)]     = _binary_;
    opcode_handlers[static_cast<int>(Opcode::XOR)]    = _binary_;
    opcode_handlers[static_cast<int>(Opcode::NEG)]    = _neg_;
    opcode_handlers[static_cast<int>(Opcode::MOV)]    = _mov_;
    opcode_handlers[static_cast<int>(Opcode::LD)]     = _ld_; 
    opcode_handlers[static_cast<int>(Opcode::ST)]     = _st_;
    opcode_handlers[static_cast<int>(Opcode::HALT)]   = _halt_;
    opcode_handlers[static_cast<int>(Opcode::DEF)]    = _def_;
    opcode_handlers[static_cast<int>(Opcode::LABEL)]  = _label_;
    opcode_handlers[static_cast<int>(Opcode::CMP_LT)] = _cond_;
    opcode_handlers[static_cast<int>(Opcode::CMP_EQ)] = _cond_;
    opcode_handlers[static_cast<int>(Opcode::CMP_GT)] = _cond_;
    opcode_handlers[static_cast<int>(Opcode::JMP)]    = _jump_;
    opcode_handlers[static_cast<int>(Opcode::JMPU)]   = _jump_;
    opcode_handlers[static_cast<int>(Opcode::BAR)]    = _bar_;
}

static const char* opSymbol(Opcode op) {
    switch (op) {
        case Opcode::ADD: return "+";
        case Opcode::SUB: return "-";
        case Opcode::MUL: return "*";
        case Opcode::DIV: return "/";
        case Opcode::AND: return "&";
        case Opcode::OR:  return "|";
        case Opcode::XOR: return "^";
        default:          return "?";
    }
}

static std::string showOperand(const OpInfo& op) {
    switch (op.kind) {
        case OpKind::Register: return "r" + std::to_string(op.index);
        case OpKind::Global:   return "gm[" + std::to_string(op.index) + "]";
        case OpKind::Shared:   return "sm[" + std::to_string(op.index) + "]";
        case OpKind::Constant: return std::to_string(op.constVal);
        case OpKind::Variable: return op.var.name;
        default:               return "?";
    }
}

ErrorCode _binary_(ExecutionContext& ctx, const Instr& instr)
{
    if (instr.src.size() < 3) return ErrorCode::BadOperand;
    OpInfo dst = decodeOperand(instr.src[0], ctx);
    OpInfo lhs = decodeOperand(instr.src[1], ctx);
    OpInfo rhs = decodeOperand(instr.src[2], ctx);
    if (dst.kind == OpKind::Invalid || lhs.kind == OpKind::Invalid || rhs.kind == OpKind::Invalid)
        return ErrorCode::BadOperand;

    float result = eval(lhs, rhs, instr.op, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None) return err;

    std::cout << "\n[T" << ctx.thread.id() << "] " << showOperand(lhs)
              << " " << opSymbol(instr.op) << " " << showOperand(rhs)
              << " -> " << showOperand(dst) << "\n";
    return ErrorCode::None;
}

ErrorCode _neg_(ExecutionContext& ctx, const Instr& instr)
{
    if (instr.src.size() < 2) return ErrorCode::BadOperand;
    OpInfo dst = decodeOperand(instr.src[0], ctx);
    OpInfo src = decodeOperand(instr.src[1], ctx);
    if (dst.kind == OpKind::Invalid || src.kind == OpKind::Invalid)
        return ErrorCode::BadOperand;

    float result = -fetch(src, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None) return err;

    std::cout << "\n[T" << ctx.thread.id() << "] NEG " << showOperand(src)
              << " -> " << showOperand(dst) << "\n";
    return ErrorCode::None;
}

ErrorCode _mov_(ExecutionContext& ctx, const Instr& instr)
{
    if (instr.src.size() < 2) return ErrorCode::BadOperand;
    OpInfo dst = decodeOperand(instr.src[0], ctx);
    OpInfo src = decodeOperand(instr.src[1], ctx);
    if (dst.kind == OpKind::Invalid || src.kind == OpKind::Invalid)
        return ErrorCode::BadOperand;

    float result = fetch(src, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None) return err;

    std::cout << "\n[T" << ctx.thread.id() << "] MOV " << showOperand(src)
              << " -> " << showOperand(dst) << "\n";
    return ErrorCode::None;
}

ErrorCode _ld_(ExecutionContext& ctx, const Instr& instr)
{
    if (instr.src.size() < 2) return ErrorCode::BadOperand;
    OpInfo dst = decodeOperand(instr.src[0], ctx);
    OpInfo src = decodeOperand(instr.src[1], ctx);
    if (dst.kind == OpKind::Invalid || src.kind == OpKind::Invalid)
        return ErrorCode::BadOperand;

    float result = fetch(src, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None) return err;

    std::cout << "\n[T" << ctx.thread.id() << "] LD " << showOperand(src)
              << " -> " << showOperand(dst) << "\n";
    return ErrorCode::None;
}

ErrorCode _st_(ExecutionContext& ctx, const Instr& instr)
{
    if (instr.src.size() < 2) return ErrorCode::BadOperand;
    OpInfo dst = decodeOperand(instr.src[0], ctx);
    OpInfo src = decodeOperand(instr.src[1], ctx);
    if (dst.kind == OpKind::Invalid || src.kind == OpKind::Invalid)
        return ErrorCode::BadOperand;

    float result = fetch(src, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None) return err;

    std::cout << "\n[T" << ctx.thread.id() << "] ST " << showOperand(src)
              << " -> " << showOperand(dst) << "\n";
    return ErrorCode::None;
}

ErrorCode _halt_(ExecutionContext& ctx, const Instr&)
{
    ctx.thread.active = false;
    std::cout << "\n[T" << ctx.thread.id() << "] HALT\n";
    return ErrorCode::None;
}

ErrorCode _def_(ExecutionContext& ctx, const Instr& instr)
{
    if (instr.src.empty()) return ErrorCode::BadOperand;

    Variable var;
    try {
        var = std::get<Variable>(instr.src[0]);
    } catch (const std::bad_variant_access&) {
        return ErrorCode::BadOperand;
    }

    int lane = ctx.thread.id() % (int)ctx.warp.memory.size();
    int writeOffset = var.threadIDX
        ? (var.loc == StoreLoc::GLOBAL ? ctx.thread.id() : lane)
        : var.offset;

    if (!ctx.vars.has(var.name)) {
        ctx.vars.addVar(var);
    }

    switch (var.loc) {
        case StoreLoc::GLOBAL:
            if (writeOffset < 0 || writeOffset >= (int)ctx.globalMem.size())
                return ErrorCode::GlobalOutOfBounds;
            ctx.globalMem[writeOffset] = var.value;
            break;
        case StoreLoc::LOCAL:
            if (writeOffset < 0 || writeOffset >= (int)ctx.thread._registers.size())
                return ErrorCode::InvalidMemorySpace;
            ctx.thread._registers[writeOffset] = var.value;
            break;
        case StoreLoc::SHARED:
            if (writeOffset < 0 || writeOffset >= (int)ctx.warp.memory.size())
                return ErrorCode::SharedOutOfBounds;
            ctx.warp.memory[writeOffset] = var.value;
            break;
    }
    return ErrorCode::None;
}

ErrorCode _label_(ExecutionContext& ctx, const Instr& instr)
{
    if (instr.src.size() < 2) return ErrorCode::BadOperand;
    const std::string* name = std::get_if<std::string>(&instr.src[0]);
    const int* pos = std::get_if<int>(&instr.src[1]);
    if (!name || !pos) return ErrorCode::BadOperand;
    ctx.labels.addLabel(*name, *pos);
    return ErrorCode::None;
}

ErrorCode _cond_(ExecutionContext& ctx, const Instr& instr)
{
    if (instr.src.size() < 2) return ErrorCode::BadOperand;
    OpInfo a = decodeOperand(instr.src[0], ctx);
    OpInfo b = decodeOperand(instr.src[1], ctx);
    if (a.kind == OpKind::Invalid || b.kind == OpKind::Invalid)
        return ErrorCode::BadOperand;

    float va = fetch(a, ctx);
    float vb = fetch(b, ctx);
    bool taken = instr.op == Opcode::CMP_EQ ? va == vb
               : instr.op == Opcode::CMP_GT ? va >  vb
               :                              va <  vb;
    if (taken) {
        ctx.thread.predicateReg = 1;
        std::cout << "\n[T" << ctx.thread.id() << "] COND TRUE\n";
    } else {
        ctx.thread.predicateReg = 0;
        std::cout << "\n[T" << ctx.thread.id() << "] COND FALSE\n";
    }
    return ErrorCode::None;
}

ErrorCode _bar_(ExecutionContext&, const Instr&)
{
    return ErrorCode::None;
}

ErrorCode _jump_(ExecutionContext& ctx, const Instr& instr)
{
    if (instr.src.empty()) return ErrorCode::BadOperand;
    const std::string* name = std::get_if<std::string>(&instr.src[0]);
    if (!name) return ErrorCode::BadOperand;

    std::optional<int> pos = ctx.labels.getLabel(*name);
    if (!pos.has_value()) return ErrorCode::VarNotFound;

    (void)pos;
    return ErrorCode::None;
}

#include "operations.hpp"
#include "execution.hpp"
#include "vartable.hpp"
#include "labeltable.hpp"
#include <iostream>
#include <algorithm>
std::array<HandlerFn, 13> opcode_handlers;

void setup_opcode_handlers()
{
    opcode_handlers[static_cast<int>(Opcode::ADD)] = _add_;
    opcode_handlers[static_cast<int>(Opcode::SUB)] = _sub_;
    opcode_handlers[static_cast<int>(Opcode::MUL)] = _mul_;
    opcode_handlers[static_cast<int>(Opcode::DIV)] = _div_;
    opcode_handlers[static_cast<int>(Opcode::NEG)] = _neg_;
    opcode_handlers[static_cast<int>(Opcode::MOV)] = _mov_;
    opcode_handlers[static_cast<int>(Opcode::LD)] = _ld_;
    opcode_handlers[static_cast<int>(Opcode::ST)] = _st_;
    opcode_handlers[static_cast<int>(Opcode::HALT)] = _halt_;
    opcode_handlers[static_cast<int>(Opcode::DEF)] = _def_;
    opcode_handlers[static_cast<int>(Opcode::LABEL)] = _label_;
    opcode_handlers[static_cast<int>(Opcode::CMP_LT)] = _cond_;
    opcode_handlers[static_cast<int>(Opcode::JNZ)] = _jump_;

}

ErrorCode _add_(Thread &t, Warp &warp, std::vector<float> &global, const Instr &instr)
{
    ExecutionContext ctx{t, warp, global};
    OpInfo dst = decodeOperand(instr.src[0], t);
    OpInfo lhs = decodeOperand(instr.src[1], t);
    OpInfo rhs = decodeOperand(instr.src[2], t);

    float result = eval(lhs, rhs, Opcode::ADD, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None)
        return err;
    auto printOperand = [&](const OpInfo &op) -> std::string
    {
        if (op.kind == OpKind::Register)
            return "r" + std::to_string(op.index);
        return std::to_string(fetch(op, ctx));
    };

    std::cout << "\n[T" << t.id() << "] ADD "
              << printOperand(lhs) << " + "
              << printOperand(rhs) << " -> "
              << (dst.kind == OpKind::Register ? "r" + std::to_string(dst.index) : dst.var.name)
              << "\n";

    return ErrorCode::None;
}
ErrorCode _sub_(Thread &t, Warp &warp, std::vector<float> &global, const Instr &instr)
{
    ExecutionContext ctx{t, warp, global};
    OpInfo dst = decodeOperand(instr.src[0], t);
    OpInfo lhs = decodeOperand(instr.src[1], t);
    OpInfo rhs = decodeOperand(instr.src[2], t);

    float result = eval(lhs, rhs, Opcode::SUB, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None)
        return err;
    auto printOperand = [&](const OpInfo &op) -> std::string
    {
        if (op.kind == OpKind::Register)
            return "r" + std::to_string(op.index);
        return std::to_string(fetch(op, ctx));
    };

    std::cout << "\n[T" << t.id() << "] SUB "
              << printOperand(lhs) << " - "
              << printOperand(rhs) << " -> "
              << (dst.kind == OpKind::Register ? "r" + std::to_string(dst.index) : dst.var.name)
              << "\n";

    return ErrorCode::None;
}
ErrorCode _mul_(Thread &t, Warp &warp, std::vector<float> &global, const Instr &instr)
{
    ExecutionContext ctx{t, warp, global};
    OpInfo dst = decodeOperand(instr.src[0], t);
    OpInfo lhs = decodeOperand(instr.src[1], t);
    OpInfo rhs = decodeOperand(instr.src[2], t);

    float result = eval(lhs, rhs, Opcode::MUL, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None)
        return err;
    auto printOperand = [&](const OpInfo &op) -> std::string
    {
        if (op.kind == OpKind::Register)
            return "r" + std::to_string(op.index);
        return std::to_string(fetch(op, ctx));
    };

    std::cout << "\n[T" << t.id() << "] MUL "
              << printOperand(lhs) << " * "
              << printOperand(rhs) << " -> "
              << (dst.kind == OpKind::Register ? "r" + std::to_string(dst.index) : dst.var.name)
              << "\n";

    return ErrorCode::None;
}
ErrorCode _div_(Thread &t, Warp &warp, std::vector<float> &global, const Instr &instr)
{
    ExecutionContext ctx{t, warp, global};
    OpInfo dst = decodeOperand(instr.src[0], t);
    OpInfo lhs = decodeOperand(instr.src[1], t);
    OpInfo rhs = decodeOperand(instr.src[2], t);

    float result = eval(lhs, rhs, Opcode::DIV, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None)
        return err;
    auto printOperand = [&](const OpInfo &op) -> std::string
    {
        if (op.kind == OpKind::Register)
            return "r" + std::to_string(op.index);
        return std::to_string(fetch(op, ctx));
    };

    std::cout << "\n[T" << t.id() << "] DIV "
              << printOperand(lhs) << " / "
              << printOperand(rhs) << " -> "
              << (dst.kind == OpKind::Register ? "r" + std::to_string(dst.index) : dst.var.name)
              << "\n";

    return ErrorCode::None;
}

ErrorCode _neg_(Thread &t, Warp &warp, std::vector<float> &global, const Instr &instr)
{
    ExecutionContext ctx{t, warp, global};

    if (instr.src.size() < 2) {
        std::cerr << "NEG error: insufficient operands\n";
        return ErrorCode::InvalidMemorySpace;
    }
    
    std::string dst_str, src_str;
    try {
        dst_str = std::get<std::string>(instr.src[0]);
        src_str = std::get<std::string>(instr.src[1]);
    } catch (const std::bad_variant_access& e) {
        std::cerr << "NEG error: operands must be strings\n";
        return ErrorCode::StringReq;
    }
    
    OpInfo dst = decodeOperand(dst_str, t);
    OpInfo src = decodeOperand(src_str, t);
    float result = eval(dst, src, Opcode::NEG, ctx);
    auto printOperand = [&](const OpInfo &op) -> std::string
    {
        if (op.kind == OpKind::Register)
            return "r" + std::to_string(op.index);
        return std::to_string(fetch(op, ctx));
    };
    std::cout << "\n[T" << t.id() << "] NEG "
              << printOperand(dst) << " *-1 "
              << printOperand(src) << " -> "
              << (dst.kind == OpKind::Register ? "r" + std::to_string(dst.index) : dst.var.name)
              << "\n";
    return ErrorCode::None;
}
ErrorCode _mov_(Thread &t, Warp &warp, std::vector<float> &global, const Instr &instr)
{
    ExecutionContext ctx{t, warp, global};
    OpInfo dest = decodeOperand(instr.src[0], t);
    OpInfo src = decodeOperand(instr.src[1], t);
    float result = eval(dest, src, Opcode::MOV, ctx);
    if (dest.index >= 0 && dest.index < t._registers.size()) {
        t._registers[dest.index] = result;
    } else {
        std::cerr << "MOV error: invalid register index " << dest.index << "\n";
        return ErrorCode::InvalidMemorySpace;
    }

    if (src.kind == OpKind::Register)
    {
        std::cout << "\n[T" << t.id() << "] MOV r" << src.index << " -> r" << dest.index << "\n";
    }
    else
    {
        std::cout << "\n[T" << t.id() << "] MOV " << src.constVal << " -> r" << dest.index << "\n";
    }
    return ErrorCode::None;
}

ErrorCode _ld_(Thread &t, Warp &warp, std::vector<float> &global, const Instr &instr)
{
    if (instr.src.size() < 2) {
        std::cerr << "LD error: insufficient operands\n";
        return ErrorCode::InvalidMemorySpace;
    }
    
    std::string src, dest;
    try {
        src = std::get<std::string>(instr.src[1]);
        dest = std::get<std::string>(instr.src[0]);
    } catch (const std::bad_variant_access& e) {
        std::cerr << "LD error: operands must be strings\n";
        return ErrorCode::StringReq;
    }

    int src_idx = getMemoryLocation(src);
    int dest_idx = getRegisterName(dest);

    if (dest_idx < 0 || dest_idx >= t._registers.size()) {
        std::cerr << "LD error: invalid destination register index " << dest_idx << "\n";
        return ErrorCode::InvalidMemorySpace;
    }
    
    if (src.find("gm") != std::string::npos)
    {
        if (src_idx == TIDX_RETURN_VAL)
        {
            int addr = t.id();
            if (addr >= 0 && addr < global.size()) {
                t._registers[dest_idx] = global[addr];
            } else {
                std::cerr << "LD error: global memory address out of bounds: " << addr << "\n";
                return ErrorCode::GlobalOutOfBounds;
            }
        }
        else if (src_idx >= global.size())
        {
            std::cerr << "LD error: global out of bounds\n";
            return ErrorCode::GlobalOutOfBounds;
        }
        else
        {
            t._registers[dest_idx] = global[src_idx];
        }
    }
    else if (src.find("sm") != std::string::npos)
    {
        if (src_idx == TIDX_RETURN_VAL)
        {
            int addr = t.id();
            if (addr >= 0 && addr < warp.memory.size()) {
                t._registers[dest_idx] = warp.memory[addr];
            } else {
                std::cerr << "LD error: shared memory address out of bounds: " << addr << "\n";
                return ErrorCode::SharedOutOfBounds;
            }
        }
        else if (src_idx >= warp.memory.size())
        {
            std::cerr << "LD error: shared/warp out of bounds\n";
            return ErrorCode::SharedOutOfBounds;
        }
        else
        {
            t._registers[dest_idx] = warp.memory[src_idx];
        }
    }
    else
    {
        std::cerr << "LD error: invalid memory space\n";
        return ErrorCode::InvalidMemorySpace;
    }
    std::cout << "\n[T" << t.id() << "] LD " << dest << " <- [" << src_idx << "]\n";

    return ErrorCode::None;
}

ErrorCode _st_(Thread &t, Warp &warp, std::vector<float> &global, const Instr &instr)
{
    if (instr.src.size() < 2) {
        std::cerr << "ST error: insufficient operands\n";
        return ErrorCode::InvalidMemorySpace;
    }
    
    std::string dest, src_reg;
    try {
        dest = std::get<std::string>(instr.src[0]);
        src_reg = std::get<std::string>(instr.src[1]);
    } catch (const std::bad_variant_access& e) {
        std::cerr << "ST error: operands must be strings\n";
        return ErrorCode::StringReq;
    }
    
    int src_idx = getRegisterName(src_reg);
    int addr = t.id();

    if (src_idx < 0 || src_idx >= t._registers.size()) {
        std::cerr << "ST error: invalid register index " << src_idx << "\n";
        return ErrorCode::InvalidMemorySpace;
    }
    
    if (dest.find("gm") != std::string::npos)
    {
        if (addr >= 0 && addr < global.size()) {
            global[addr] = t._registers[src_idx];
        } else {
            std::cerr << "ST error: global memory address out of bounds: " << addr << "\n";
            return ErrorCode::GlobalOutOfBounds;
        }
    }
    else if (dest.find("sm") != std::string::npos)
    {
        if (addr >= 0 && addr < warp.memory.size()) {
            warp.memory[addr] = t._registers[src_idx];
        } else {
            std::cerr << "ST error: shared memory address out of bounds: " << addr << "\n";
            return ErrorCode::SharedOutOfBounds;
        }
    }
    else
    {
        std::cerr << "ST error: invalid memory space\n";
        return ErrorCode::InvalidMemorySpace;
    }
    std::cout << "\n[T" << t.id() << "] ST r" << src_idx << " -> " << dest << "[" << addr << "]\n";

    return ErrorCode::None;
}

ErrorCode _halt_(Thread &t, Warp &, std::vector<float> &, const Instr &)
{
    t.active = false;
    std::cout << "\n[T" << t.id() << "] HALT\n";
    return ErrorCode::None;
}

ErrorCode _def_(Thread &t, Warp &warp, std::vector<float> &global_mem, const Instr &instr)
{
    if (instr.src.empty()) {
        std::cerr << "DEF error: no operands\n";
        return ErrorCode::InvalidMemorySpace;
    }
    
    Variable var;
    try {
        var = std::get<Variable>(instr.src[0]);
    } catch (const std::bad_variant_access& e) {
        std::cerr << "DEF error: operand must be a Variable\n";
        return ErrorCode::InvalidMemorySpace;
    }
    if (var.threadIDX)
    {
        var.offset = t.id();
    }

    VarTable::getInstance().addVar(var, t.id());

    switch (var.loc)
    {
    case StoreLoc::GLOBAL:
        if (var.offset >= 0 && var.offset < global_mem.size()) {
            global_mem[var.offset] = var.value;
        } else {
            std::cerr << "VAR DEF error: global memory offset out of bounds: " << var.offset << "\n";
            return ErrorCode::GlobalOutOfBounds;
        }
        break;
    case StoreLoc::LOCAL:
        if (var.offset >= NUM_REGISTERS)
        {
            std::cerr << "VAR DEF error: variable offset larger than register count\n";
            break;
        }
        if (var.offset >= 0 && var.offset < t._registers.size()) {
            t._registers[var.offset] = var.value;
        } else {
            std::cerr << "VAR DEF error: register offset out of bounds: " << var.offset << "\n";
            return ErrorCode::InvalidMemorySpace;
        }
        break;
    case StoreLoc::SHARED:
        if (var.offset >= GLOBAL_MEM_SIZE)
        {
            std::cerr << "VAR DEF error: variable offset larger than warp mem size\n";
            break;
        }
        if (var.offset >= 0 && var.offset < warp.memory.size()) {
            warp.memory[var.offset] = var.value;
        } else {
            std::cerr << "VAR DEF error: shared memory offset out of bounds: " << var.offset << "\n";
            return ErrorCode::SharedOutOfBounds;
        }
        break;
    default:
        break;
    }

    return ErrorCode::None;
}

ErrorCode _label_(Thread &t, Warp &warp, std::vector<float> &global, const Instr &instr)
{
    if (instr.src.size() < 2) {
        std::cerr << "LABEL error: insufficient operands\n";
        return ErrorCode::InvalidMemorySpace;
    }
    
    if(const std::string* loop= std::get_if<std::string>(&instr.src[0])){
        try {
            int pos = std::get<int>(instr.src[1]);
            labelTable::getInstance().addLabel(*loop, pos);
        } catch (const std::bad_variant_access& e) {
            std::cerr << "LABEL error: second operand must be an integer\n";
            return ErrorCode::InvalidMemorySpace;
        }
    }else{
        std::cerr << "LABEL error: first operand has to be a string\n";
        return ErrorCode::StringReq;
    }
    return ErrorCode::None;
}

ErrorCode _cond_(Thread &t, Warp &warp, std::vector<float> &global, const Instr &instr)
{
    if (instr.src.size() < 2) {
        std::cerr << "CMP_LT error: insufficient operands\n";
        return ErrorCode::InvalidMemorySpace;
    }
    
    std::string var1_name, var2_name;
    try {
        var1_name = std::get<std::string>(instr.src[0]);
        var2_name = std::get<std::string>(instr.src[1]);
    } catch (const std::bad_variant_access& e) {
        std::cerr << "CMP_LT error: operands must be strings\n";
        return ErrorCode::StringReq;
    }
    
    std::optional<Variable> v1 = VarTable::getInstance().getVar(var1_name, t.id());
    std::optional<Variable> v2 = VarTable::getInstance().getVar(var2_name, t.id());
    if (!v1.has_value() || !v2.has_value()) {
        std::cerr << "CMP_LT error: variable not found\n";
        return ErrorCode::VarNotFound;
    }
    if(v1.value().value <v2.value().value){
        t.predicateReg = true;
        std::cout  << "\nCOND FAILED CONTINUING LOOP\n";
    }else{
        t.predicateReg = false;
        std::cout  << "\nEXITING LOOP\n";

    }

    return ErrorCode::None;
}

ErrorCode _jump_(Thread &t, Warp &warp, std::vector<float> &global, const Instr &instr)
{

    std::cout << "JUMP\n";
    if (instr.src.empty()) {
        std::cerr << "JNZ error: no operands\n";
        return ErrorCode::InvalidMemorySpace;
    }
    
    std::string label_name;
    try {
        label_name = std::get<std::string>(instr.src[0]);
    } catch (const std::bad_variant_access& e) {
        std::cerr << "JNZ error: operand must be a string\n";
        return ErrorCode::StringReq;
    }
    
    std::optional<int> labelPos = labelTable::getInstance().getLabel(label_name);

    if(t.predicateReg){
        t.pc = static_cast<size_t>(labelPos.value()-1);
        std::cout << "\nJUMPED TO " << labelPos.value()-1 << "\n";
    }else{
        return ErrorCode::None;
    }
    return ErrorCode::None;
}

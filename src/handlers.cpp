#include "handlers.hpp"
#include "execution.hpp"
#include "vartable.hpp"
#include <iostream>

std::array<HandlerFn, 10> opcode_handlers;

void setup_opcode_handlers() {
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
}

ErrorCode _add_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr) {
    ExecutionContext ctx{t, warp, global};
    OpInfo dst = decodeOperand(instr.src[0], t);
    OpInfo lhs = decodeOperand(instr.src[1], t);
    OpInfo rhs = decodeOperand(instr.src[2],t);

    float result = eval(lhs, rhs, Opcode::ADD, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None) return err;
    auto printOperand = [&](const OpInfo& op) -> std::string {
        if (op.kind == OpKind::Register) return "r" + std::to_string(op.index);
        return std::to_string(fetch(op, ctx));
    };

    std::cout << "\n[T" << t.id() << "] ADD "
              << printOperand(lhs) << " + "
              << printOperand(rhs) << " -> "
              << (dst.kind == OpKind::Register ? "r" + std::to_string(dst.index) : dst.var.name)
              << "\n";
    t.printRegisters();
    return ErrorCode::None;
}
ErrorCode _sub_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr) {
    ExecutionContext ctx{t, warp, global};
    OpInfo dst = decodeOperand(instr.src[0], t);
    OpInfo lhs = decodeOperand(instr.src[1], t);
    OpInfo rhs = decodeOperand(instr.src[2],t);

    float result = eval(lhs, rhs, Opcode::SUB, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None) return err;
    auto printOperand = [&](const OpInfo& op) -> std::string {
        if (op.kind == OpKind::Register) return "r" + std::to_string(op.index);
        return std::to_string(fetch(op, ctx));
    };

    std::cout << "[T" << t.id() << "] SUB "
              << printOperand(lhs) << " - "
              << printOperand(rhs) << " -> "
              << (dst.kind == OpKind::Register ? "r" + std::to_string(dst.index) : dst.var.name)
              << "\n";
    t.printRegisters();
    return ErrorCode::None;
}
ErrorCode _mul_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr) {
    ExecutionContext ctx{t, warp, global};
    OpInfo dst = decodeOperand(instr.src[0], t);
    OpInfo lhs = decodeOperand(instr.src[1], t);
    OpInfo rhs = decodeOperand(instr.src[2],t);

    float result = eval(lhs, rhs, Opcode::MUL, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None) return err;
    auto printOperand = [&](const OpInfo& op) -> std::string {
        if (op.kind == OpKind::Register) return "r" + std::to_string(op.index);
        return std::to_string(fetch(op, ctx));
    };

    std::cout << "[T" << t.id() << "] MUL "
              << printOperand(lhs) << " * "
              << printOperand(rhs) << " -> "
              << (dst.kind == OpKind::Register ? "r" + std::to_string(dst.index) : dst.var.name)
              << "\n";
    t.printRegisters();
    return ErrorCode::None;
}
ErrorCode _div_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr) {
    ExecutionContext ctx{t, warp, global};
    OpInfo dst = decodeOperand(instr.src[0], t);
    OpInfo lhs = decodeOperand(instr.src[1], t);
    OpInfo rhs = decodeOperand(instr.src[2],t);

    float result = eval(lhs, rhs, Opcode::DIV, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None) return err;
    auto printOperand = [&](const OpInfo& op) -> std::string {
        if (op.kind == OpKind::Register) return "r" + std::to_string(op.index);
        return std::to_string(fetch(op, ctx));
    };

    std::cout << "[T" << t.id() << "] DIV "
              << printOperand(lhs) << " / "
              << printOperand(rhs) << " -> "
              << (dst.kind == OpKind::Register ? "r" + std::to_string(dst.index) : dst.var.name)
              << "\n";
    t.printRegisters();
    return ErrorCode::None;
}

ErrorCode _neg_(Thread& t, Warp&, std::vector<float>&, const Instr& instr) {
    int dest = getRegisterName(std::get<std::string>(instr.src[0]));
    int src = getRegisterName(std::get<std::string>(instr.src[1]));
    
    t._registers[dest] = -1*t._registers[src];
    std::cout << "[T" << t.id() << "] NEG r" << src << "\n";
    t.printRegisters();
    return ErrorCode::None;
}
ErrorCode _mov_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr) {
    ExecutionContext ctx{t, warp, global};
    OpInfo dest = decodeOperand(instr.src[0],t);
    OpInfo src = decodeOperand(instr.src[1],t);
    float result = eval(dest, src, Opcode::MOV, ctx);
    t._registers[dest.index] = result;
    t.printRegisters();
    if(src.kind == OpKind::Register){
        std::cout << "[T" << t.id() << "] MOV r" << src.index << " -> r" << dest.index << "\n";

    }else{
        std::cout << "[T" << t.id() << "] MOV " << src.constVal << " -> r" << dest.index << "\n";

    }
    return ErrorCode::None;
}

ErrorCode _ld_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr) {
    std::string src = std::get<std::string>(instr.src[1]);
    std::string dest = std::get<std::string>(instr.src[0]);

    int src_idx = getMemoryLocation(src);
    int dest_idx = getRegisterName(dest);
    
    if (src.find("gm") !=std::string::npos ) {
        if (src_idx == TIDX_RETURN_VAL) {
            int addr = t.id(); 
            t._registers[dest_idx] = global[addr];
            
        }else if(src_idx >= global.size()){
            std::cerr << "LD error: global out of bounds\n";
            return ErrorCode::GlobalOutOfBounds;
        }else{
            t._registers[dest_idx] = global[src_idx];

        }
        
    } else if (src.find("sm") !=std::string::npos) {
         if (src_idx == TIDX_RETURN_VAL) {
            int addr = t.id(); 
            t._registers[dest_idx] = warp.memory[addr];
            
        }else if(src_idx >= warp.memory.size()){
            std::cerr << "LD error: shared/warp out of bounds\n";
            return ErrorCode::SharedOutOfBounds;
        }else{
            t._registers[dest_idx] = warp.memory[src_idx];

        }
    } else {
        std::cerr << "LD error: invalid memory space\n";
        return ErrorCode::InvalidMemorySpace;
    }
    std::cout << "[T" << t.id() << "] LD " << dest << " <- [" << src_idx << "]\n";
    t.printRegisters();
    return ErrorCode::None;
}

ErrorCode _st_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr) {
    std::string dest = std::get<std::string>(instr.src[0]);
    int src_idx = getRegisterName(std::get<std::string>(instr.src[1]));
    int addr = t.id(); 

    if (dest.find("gm") != std::string::npos) {
        global[addr] = t._registers[src_idx];
    } else if (dest.find("sm") != std::string::npos) {
        warp.memory[addr] = t._registers[src_idx];
    } else {
        std::cerr << "ST error: invalid memory space\n";
        return ErrorCode::InvalidMemorySpace;
    }
    std::cout << "[T" << t.id() << "] ST r" << src_idx<< " -> " << dest << "[" << addr << "]\n";
    t.printRegisters();
    return ErrorCode::None;
}

ErrorCode _halt_(Thread& t, Warp&,std::vector<float>&,const Instr&) {
    t.active = false;
    std::cout << "[T" << t.id() << "] HALT\n";
    return ErrorCode::None;
}

ErrorCode _def_(Thread& t, Warp& warp,std::vector<float>& global_mem,const Instr& instr) {
    Variable var = std::get<Variable>(instr.src[0]);
    if(var.threadIDX){
        var.offset = t.id();

    }

    VarTable::getInstance().addVar(var,t.id());

    switch (var.loc)
    {
    case StoreLoc::GLOBAL:
        global_mem[var.offset] = var.value;  
        break;
    case StoreLoc::LOCAL:
        if(var.offset>NUM_REGISTERS){
            std::cerr << "VAR DEF error: variable offset larger than register count\n";
            break;
        }
        t._registers[var.offset] = var.value;
        break;
    case StoreLoc::SHARED:
        if(var.offset>GLOBAL_MEM_SIZE){
            std::cerr << "VAR DEF error: variable offset larger than warp mem size\n";
            break;
        }
        warp.memory[var.offset] = var.value;
        break;
    default:
        break;
    }
    
    return ErrorCode::None;
}
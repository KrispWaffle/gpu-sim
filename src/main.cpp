#include <iostream>
#include <vector>
#include <algorithm>
#include <variant>
#include <string>
#include <stdexcept>
#include <array>
#include <optional>
#include <cstddef>
#include <memory>
#include <cassert>
#include <thread>
#include <tuple>
#include <unordered_map>
constexpr int NUM_THREADS = 4;
constexpr int NUM_REGISTERS = 4;
constexpr int GLOBAL_MEM_SIZE = NUM_THREADS;
constexpr int WARP_SIZE = 32;
constexpr int SLEEP_TIME =0; // In seconds 
constexpr size_t NUM_OPCODES = 9; 
constexpr int NUM_VAR_LOCS=3;
enum class Opcode
{
    ADD,
    SUB,
    MUL,
    DIV,
    NEG,
    LD,
    ST,
    MOV,
    HALT,
    DEF,
};


enum class states
{
    IDLE,
    ACTIVE,
    FINISHED
};
enum class ErrorCode {
    None,
    GlobalOutOfBounds,
    SharedOutOfBounds,
    InvalidMemorySpace,
    DivByZero,
};
enum class StoreLoc{
    GLOBAL,
    SHARED,
    LOCAL
};
struct Variable
{

    std::string name;
    float value; 
    int  offset; 
    bool isConstant;
    bool threadIDX;
    StoreLoc loc;
};

using Operand = std::variant<Opcode, std::string, float, Variable, StoreLoc>;
struct Instr
{
    Opcode op;
    std::vector<Operand> src;
};
class Thread
{

public:
    size_t pc;
    int id_;
    Instr intruction;
    static int _id;
    std::vector<float> _registers;
    bool active;
    Thread() : id_(_id++), pc(0), _registers(NUM_REGISTERS, 0.0f), active(false) {}
    int id() { return id_; }
    void printRegisters()
    {
        std::cout << "\nTHREAD: " << this->id_ << "";
        for(int x =0; x<_registers.size(); x++){
            std::cout<< "\nREG: " << x << " VALUE: " << _registers[x];;
            
        }
        
        std::cout << std::endl;
    }
    void set_instruction(Instr _instruction) { intruction = _instruction; }
};
int Thread::_id = 0;

class VarTable{
 public:
    VarTable(const VarTable&) =delete;
    VarTable& operator=(const VarTable&) = delete;
    static VarTable& getInstance(){
        static VarTable instance;
        return instance;
    }


    void addVar(const Variable& var, int thread_id) {
        table[var.name + "_" + std::to_string(thread_id)] = var;
    }

    std::optional<Variable> getVar(const std::string& name, int thread_id) {
        auto it = table.find(name + "_" + std::to_string(thread_id));
        if (it != table.end()) return it->second;
        return std::nullopt;
    }

 private:
    VarTable() {} 
    std::unordered_map<std::string, Variable> table;
};
VarTable& variable_table = VarTable::getInstance();

int getRegisterName(std::string _register)
{
    if (_register.length() > 1 && std::isalpha(static_cast<unsigned char>(_register[0])))
    {
        std::string num = _register.substr(1);
        try
        {
            return std::stoi(num);
        }
        catch (const std::exception &e)
        {
            return -1;
        }
    }
    return -1;
}
enum class OpKind { Constant, Register, Variable, Invalid };

struct OpInfo {
    OpKind   kind;
    float    constVal;   // valid if kind == Constant
    int      index;      // reg number or memory offset/TIDX
    Variable var;        // valid if kind == Variable
};

OpInfo decodeOperand(const Operand &op, Thread &t) {
    if (auto pf = std::get_if<float>(&op)) {
        return { OpKind::Constant, *pf,      0,    {} };
    }

    if (auto ps = std::get_if<std::string>(&op)) {
        const std::string &s = *ps;
        // register?
        if (s.size()>1 && s[0]=='r' && std::isdigit(s[1])) {
            int r = getRegisterName(s);
            if (r >= 0) return { OpKind::Register, 0.0f, r, {} };
        }
        // otherwise, variable lookup
        if (auto ov = variable_table.getVar(s, t.id())) {
            Variable v = *ov;
            int addr = v.offset;
            float val = v.value;
            return { OpKind::Variable, val, addr, std::move(v) };
        }
    }

    return { OpKind::Invalid, 0.0f, -1, {} };
}


constexpr int TIDX_RETURN_VAL = -1;
int getMemoryLocation(std::string mem){
    std::string num = mem.substr(2);


    if(num == "TIDX"){
        return TIDX_RETURN_VAL;
    }
    try
    {
        return std::stoi(num);
    }
    catch(const std::exception& e)
    {
        std::cerr<< "ERROR with getting mem location: "<< e.what() << '\n';
        return -2;
    }

    
}
class Warp
{
private:
    static int _id;

public:
    int id_;
    std::vector<std::shared_ptr<Thread>> threads;
    std::vector<float> memory;

    Warp() : id_(_id++), memory(GLOBAL_MEM_SIZE, 0.0f){}
    int id() { return _id; }
    void addThread(std::shared_ptr<Thread> thread)
    {
        threads.push_back(thread);
    }
    bool isFinished() const
    {
        for (const auto &t : threads)
        {
            if (t->active)
                return false;
        }
        return true;
    }
};
int Warp::_id = 0;

using HandlerFn = ErrorCode(*)(Thread&, Warp&, std::vector<float>&, const Instr&);
using InstrValue = std::variant<float, std::string>;
std::array<HandlerFn, NUM_OPCODES> opcode_handlers;
std::array<HandlerFn, NUM_VAR_LOCS> var_handlers;
struct ExecutionContext {
    Thread& thread;
    Warp& warp;
    std::vector<float>& globalMem;
};

float fetch(const OpInfo& o, const ExecutionContext& ctx) {
    switch (o.kind) {
        case OpKind::Constant: return o.constVal;
        case OpKind::Register: return ctx.thread._registers[o.index];
        case OpKind::Variable:
            switch (o.var.loc) {
                case StoreLoc::GLOBAL: return ctx.globalMem[o.index];
                case StoreLoc::SHARED: return ctx.warp.memory[o.index];
                case StoreLoc::LOCAL:  return ctx.thread._registers[o.index];
            }
        default:
            throw std::runtime_error("ERROR in fetch: unsupported operand kind");
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
            if (b == 0.0f) throw std::runtime_error("ERROR in DIV: cannot divide by zero");
            return a / b;
        default:
            throw std::runtime_error("EVAL error: unsupported opcode");
    }
}

ErrorCode storeInLocation(OpInfo& dst, float result, ExecutionContext& ctx) {
    switch (dst.kind) {
        case OpKind::Register:
            ctx.thread._registers[dst.index] = result;
            break;
        case OpKind::Variable:
            switch (dst.var.loc) {
                case StoreLoc::GLOBAL: ctx.globalMem[dst.index] = result; break;
                case StoreLoc::SHARED: ctx.warp.memory[dst.index] = result; break;
                case StoreLoc::LOCAL:  ctx.thread._registers[dst.index] = result; break;
            }
            break;
        default:
            std::cerr << "STORING RESULT error: cannot write to this operand\n";
            return ErrorCode::InvalidMemorySpace;
    }
    return ErrorCode::None;
}

ErrorCode _add_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr) {
    ExecutionContext ctx{t, warp, global};
    OpInfo dst = decodeOperand(std::get<std::string>(instr.src[0]), t);
    OpInfo lhs = decodeOperand(std::get<std::string>(instr.src[1]), t);
    OpInfo rhs = decodeOperand(instr.src[2],t);

    float result = eval(lhs, rhs, Opcode::ADD, ctx);
    ErrorCode err = storeInLocation(dst, result, ctx);
    if (err != ErrorCode::None) return err;
    auto printOperand = [&](const OpInfo& op) -> std::string {
        if (op.kind == OpKind::Register) return "r" + std::to_string(op.index);
        return std::to_string(fetch(op, ctx));
    };

    std::cout << "[T" << t.id() << "] ADD "
              << printOperand(lhs) << " + "
              << printOperand(rhs) << " -> "
              << (dst.kind == OpKind::Register ? "r" + std::to_string(dst.index) : dst.var.name)
              << "\n";
    t.printRegisters();
    return ErrorCode::None;
}
ErrorCode _sub_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr) {
    ExecutionContext ctx{t, warp, global};
    OpInfo dst = decodeOperand(std::get<std::string>(instr.src[0]), t);
    OpInfo lhs = decodeOperand(std::get<std::string>(instr.src[1]), t);
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
    OpInfo dst = decodeOperand(std::get<std::string>(instr.src[0]), t);
    OpInfo lhs = decodeOperand(std::get<std::string>(instr.src[1]), t);
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
    OpInfo dst = decodeOperand(std::get<std::string>(instr.src[0]), t);
    OpInfo lhs = decodeOperand(std::get<std::string>(instr.src[1]), t);
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
ErrorCode _mov_(Thread& t, Warp&, std::vector<float>&, const Instr& instr) {
    int dest = getRegisterName(std::get<std::string>(instr.src[0]));
    int src = getRegisterName(std::get<std::string>(instr.src[1]));
    t._registers[dest] = t._registers[src];
    std::cout << "[T" << t.id() << "] MOV r" << src << " -> r" << dest+1 << "\n";
    t.printRegisters();
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
    std::cout << "[T" << t.id() << "] ST r" << src_idx+1 << " -> " << dest << "[" << addr << "]\n";
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

    variable_table.addVar(var,t.id());

    switch (std::get<StoreLoc>(instr.src[1]))
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
void setup_opcode_handlers() {
    opcode_handlers[static_cast<int>(Opcode::ADD)]  = _add_;
    opcode_handlers[static_cast<int>(Opcode::SUB)] = _sub_;
    opcode_handlers[static_cast<int>(Opcode::MUL)] = _mul_;
    opcode_handlers[static_cast<int>(Opcode::DIV)] = _div_;
    opcode_handlers[static_cast<int>(Opcode::NEG)] = _neg_;
    opcode_handlers[static_cast<int>(Opcode::MOV)]  = _mov_;
    opcode_handlers[static_cast<int>(Opcode::LD)]   = _ld_;
    opcode_handlers[static_cast<int>(Opcode::ST)]   = _st_;
    opcode_handlers[static_cast<int>(Opcode::HALT)] = _halt_;
    opcode_handlers[static_cast<int>(Opcode::DEF)] = _def_;
}
class SM
{
public:
    int id;
    std::vector<Warp> warps;
    std::vector<float> &globalMemory;
    SM(int sm_id, std::vector<float> &memory) : id(sm_id), globalMemory(memory) {}
    void addWarp(const Warp &warp)
    {
        warps.push_back(warp);
    }
    void cycle(const std::vector<Instr> &program)
    {

        for (auto &warp : warps)
        {
            if (warp.isFinished())
                continue;

            size_t shared_pc = 0;
            for (const auto &t : warp.threads)
            {
                if (t->active)
                {
                    shared_pc = t->pc;
                    break;
                }
            }

            const Instr &instruction = program[shared_pc];

            this->execute(warp, instruction);
        }
    }

private:
    void execute(Warp& warp, const Instr& instruction) {
    HandlerFn fn = opcode_handlers[static_cast<int>(instruction.op)];        
        for (auto& thread : warp.threads) {
            if (!thread->active) continue;
            fn(*thread, warp, globalMemory, instruction);
            if (thread->active) thread->pc++;
        }
            

    }

    
};

class GPU   
{
public:
    std::vector<float> global_memory;
    std::vector<SM> sms;
    std::vector<std::shared_ptr<Thread>> all_threads;
    std::vector<Instr> program;
    
    GPU(const std::vector<Instr> &program) : program(program), global_memory(GLOBAL_MEM_SIZE, 0.0f)
    {
        sms.emplace_back(0, global_memory);

        for (int i = 0; i < NUM_THREADS; i++)
        {
            Thread t;
            t.active = true;
            all_threads.push_back(std::make_shared<Thread>(t));
        }

        for (int i = 0; i < NUM_THREADS; i += WARP_SIZE)
        {
            Warp new_warp;
            for (int j = 0; j < WARP_SIZE && (i + j) < NUM_THREADS; j++)
            {
                new_warp.addThread(all_threads[i + j]);
            }
            sms[0].addWarp(new_warp);
        }
    }
    void print_global_mem(){
        for(const auto& m: this->global_memory){
        std::cout << m << ", ";
        }
        std::cout << "\n";
    }
    std::optional<size_t> store(int value)
    {

        auto it = std::find(global_memory.begin(), global_memory.end(), 0);

        if (it == global_memory.end())
        {
            return std::nullopt;
        }

        *it = value;
        return std::distance(global_memory.begin(), it);
    }

    void run(){
    
        long long cycle_count = 0;
        std::cout << "--- Simulation Starting ---" << std::endl;
        while (true)
        {
            bool all_sms_finished = true;
            for (auto &sm : sms)
            {
                sm.cycle(program);
                for (const auto &warp : sm.warps)
                {
                    if (!warp.isFinished())
                    {
                        all_sms_finished = false;
                    }
                }
            }

            if (all_sms_finished)
                break;

            cycle_count++;
            if (cycle_count > 1000){
                 std::cerr << "Simulation timed out!" << std::endl;
                break;
            }
        }
        std::cout << "--- Simulation Finished in " << cycle_count << " cycles ---" << std::endl;
    }
};

int main()
{
    setup_opcode_handlers();
   // setup_var_handlers();
   
    std::vector<Instr> program = {
    {Opcode::DEF, {Variable{"x",3.0f,0,false, true, StoreLoc::LOCAL}, StoreLoc::LOCAL}},
    {Opcode::SUB, {"x", "x", 2.0f}},
    {Opcode::HALT, {}},
    };
    GPU gpu(program);
   
    gpu.run();
    std::cout << "\n--- Final Register States ---" << std::endl;
    
    for (const auto& thread : gpu.all_threads) {
        thread->printRegisters();
    }
    std::cout<<std::endl;
    std::cout << "\nGLOBAL MEMORY\n";
    gpu.print_global_mem();

}

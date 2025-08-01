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
constexpr int NUM_THREADS = 4;
constexpr int NUM_REGISTERS = 4;
constexpr int GLOBAL_MEM_SIZE = NUM_THREADS;
constexpr int WARP_SIZE = 32;
constexpr int SLEEP_TIME =0; // In seconds 
constexpr size_t NUM_OPCODES = 9; 
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

using Operand = std::variant<Opcode, std::string, float>;

struct Instr
{
    Opcode op;
    std::vector<Operand> src;
};

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
ErrorCode _add_(Thread& t, Warp&, std::vector<float>&, const Instr& instr) {
    int dest = getRegisterName(std::get<std::string>(instr.src[0]));
    int src = getRegisterName(std::get<std::string>(instr.src[1]));
    auto val =0.0f;
    if(const float* p_float = std::get_if<float>(&instr.src[2])){
         val = *p_float;
    }else{
        val = t._registers[getRegisterName(std::get<std::string>(instr.src[2]))];
    }
    t._registers[dest] = t._registers[src] + val;
    std::cout << "[T" << t.id() << "] ADD r" << src << " + " << val << " -> r" << dest << "\n";
    t.printRegisters();
    return ErrorCode::None;
}
ErrorCode _sub_(Thread& t, Warp&, std::vector<float>&, const Instr& instr) {
    int dest = getRegisterName(std::get<std::string>(instr.src[0]));
    int src = getRegisterName(std::get<std::string>(instr.src[1]));
    auto val =0.0f;
    if(const float* p_float = std::get_if<float>(&instr.src[2])){
         val = *p_float;
    }else{
        val = t._registers[getRegisterName(std::get<std::string>(instr.src[2]))];
    }
    t._registers[dest] = t._registers[src] - val;
    std::cout << "[T" << t.id() << "] SUB r" << src << " - " << val << " -> r" << dest << "\n";
    t.printRegisters();
    return ErrorCode::None;
}
ErrorCode _mul_(Thread& t, Warp&, std::vector<float>&, const Instr& instr) {
    int dest = getRegisterName(std::get<std::string>(instr.src[0]));
    int src = getRegisterName(std::get<std::string>(instr.src[1]));
    auto val =0.0f;
    if(const float* p_float = std::get_if<float>(&instr.src[2])){
         val = *p_float;
    }else{
        val = t._registers[getRegisterName(std::get<std::string>(instr.src[2]))];
    }
    t._registers[dest] = t._registers[src] * val;
    std::cout << "[T" << t.id() << "] MUL r" << src << " * " << val << " -> r" << dest << "\n";
    t.printRegisters();
    return ErrorCode::None;
}
ErrorCode _div_(Thread& t, Warp&, std::vector<float>&, const Instr& instr) {
    int dest = getRegisterName(std::get<std::string>(instr.src[0]));
    int src = getRegisterName(std::get<std::string>(instr.src[1]));
    auto val =0.0f;
    if(const float* p_float = std::get_if<float>(&instr.src[2])){
         val = *p_float;
    }else{
        val = t._registers[getRegisterName(std::get<std::string>(instr.src[2]))];
    }
    if(val==0){
        std::cout << "DIV error: cannot divide by zero\n";
        return ErrorCode::DivByZero;
    }
    t._registers[dest] = t._registers[src] / val;
    std::cout << "[T" << t.id() << "] DIV r" << src << " / " << val << " -> r" << dest << "\n";
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
    std::string dest = std::get<std::string>(instr.src[0]);
    int src_idx = getMemoryLocation(std::get<std::string>(instr.src[1]));
    int dest_idx = getRegisterName(dest);
    
    if (dest.find("gm")) {
        if (src_idx == TIDX_RETURN_VAL) {
            int addr = t.id(); 
            t._registers[dest_idx] = global[addr];
            
        }else if(src_idx >= global.size()){
            std::cerr << "LD error: global out of bounds\n";
            return ErrorCode::GlobalOutOfBounds;
        }else{
            t._registers[dest_idx] = global[src_idx];

        }
        
    } else if (dest.find("sm")) {
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

ErrorCode _halt_(Thread& t, Warp&, std::vector<float>&, const Instr&) {
    t.active = false;
    std::cout << "[T" << t.id() << "] HALT\n";
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
        if (!fn) {
            std::cerr << "ERROR: Unknown opcode\n";
            return;
        }

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

    void run()
    {
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
            if (cycle_count > 1000)
            { // Safety break
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
    std::vector<Instr> program = {
    {Opcode::LD, {"r0", "gmTIDX"}},
    {Opcode::ST, {"smTIDX", "r0"}},
    {Opcode::LD, {"r1", "sm0"}},
    {Opcode::LD, {"r2", "sm1"}},
    {Opcode::LD, {"r3", "sm2"}},
    {Opcode::LD, {"r0", "sm3"}},
    {Opcode::ADD, {"r1", "r1", "r2"}},
    {Opcode::ADD, {"r1", "r1", "r3"}},
    {Opcode::ADD, {"r1", "r1", "r0"}},
    {Opcode::ST, {"gm0", "r1"}},
    {Opcode::HALT, {}},
};
    GPU gpu(program);
    for (int i = 0; i < NUM_THREADS; ++i) {
    gpu.global_memory[i] = static_cast<float>(i + 1);
}
    gpu.print_global_mem();

    gpu.run();
    std::cout << "\n--- Final Register States ---" << std::endl;
    
    
    for (const auto& thread : gpu.all_threads) {
        thread->printRegisters();
    }
    std::cout<<std::endl;
    std::cout << "\nGLOBAL MEMORY\n";
    gpu.print_global_mem();

}

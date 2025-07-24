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
constexpr int NUM_THREADS = 32;
constexpr int NUM_REGISTERS = 4;
constexpr int GLOBAL_MEM_SIZE = NUM_THREADS;
constexpr int WARP_SIZE = 32;
constexpr int SLEEP_TIME =0; // In seconds 
enum class Opcode
{
    ADD,
    MUL,
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


struct registerReturnVal{

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
            return std::stoi(num)-1;
        }
        catch (const std::exception &e)
        {
            return -1;
        }
    }
    return -1;
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
            std::cout<< "\nREG: " << x+1 << " VALUE: " << _registers[x];;
            
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

    Warp() : id_(_id++) {}
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

            execute(warp, instruction);
        }
    }

private:
    void execute(Warp warp, const Instr instruction)
    {
        for (auto &thread : warp.threads)
        {
            if (!thread->active)
                continue;

            int dest_idx,src_idx;
            switch (instruction.op)
            {

            case Opcode::ADD:
            {
              dest_idx = getRegisterName(std::get<std::string>(instruction.src[0]));
              src_idx = getRegisterName(std::get<std::string>(instruction.src[1]));
                float t_value = std::get<float>(instruction.src[2]);
                thread->_registers[dest_idx] = thread->_registers[src_idx] + t_value;
                std::cout << "ADDING " << t_value << " WITH REGISTER " << src_idx + 1 << " TO REGISTER " << dest_idx + 1 << std::endl;
                thread->printRegisters();

                break;
            }
                // r2

            case Opcode::MOV:{

          
                dest_idx = getRegisterName(std::get<std::string>(instruction.src[0]));
                src_idx = getRegisterName(std::get<std::string>(instruction.src[1]));
                thread->_registers[dest_idx] = thread->_registers[src_idx];
                std::cout << "STORING " << thread->_registers[src_idx] << " TO REGISTER " << dest_idx + 1 << std::endl;

                thread->printRegisters();

                break;
              }
            case Opcode::LD:{
                dest_idx = getRegisterName(std::get<std::string>(instruction.src[0]));
                src_idx = getRegisterName(std::get<std::string>(instruction.src[1]));
                std::this_thread::sleep_for(std::chrono::seconds(SLEEP_TIME));
                thread->_registers[dest_idx] = globalMemory[src_idx];
                
                std::cout << "LOADING FROM GLOBAL " << thread->_registers[src_idx] << " TO REGISTER " << dest_idx + 1 << std::endl;
                thread->printRegisters();
            }
            case Opcode::ST:{
                src_idx = getRegisterName(std::get<std::string>(instruction.src[1]));
                int global_addr = thread->id();
                globalMemory[global_addr] = thread->_registers[src_idx];
                std::cout << "LOADING FROM REGISTER " << thread->_registers[src_idx] << " TO GLOBAL " << dest_idx + 1 << std::endl;
                thread->printRegisters();


            }
            case Opcode::HALT:
                thread->active = false;
                break;
            default:
                std::cout << "ERROR IN RUN FUNCTION\n";

                break;
            }
            if (thread->active)
            {
                thread->pc++;
            }
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

class Program
{
public:
    Program(int burst_time, int arrival_time)
        : id_(_id++), burst_time_(burst_time), arrival_time_(arrival_time) {}

    int id() { return id_; }
    int burstTime() const { return burst_time_; }
    int arrivalTime() const { return arrival_time_; }
    void setBurstTime(int burst_time) { burst_time_ = burst_time; }

private:
    int id_;
    int burst_time_;
    int arrival_time_;
    static int _id;
};

template <typename T>
void remove(std::vector<T> &vec, typename std::vector<T>::iterator pos)
{
    vec.erase(pos);
}
class Robin
{
public:
    std::vector<Program> readyQueue;
    int quantumTime;

    Robin(std::vector<Program> readyQueue, int quantumTime)
        : readyQueue(std::move(readyQueue)), quantumTime(quantumTime) {}

    void simulate()
    {
        while (!readyQueue.empty())
        {
            for (auto it = readyQueue.begin(); it != readyQueue.end();)
            {
                if (it->burstTime() <= quantumTime)
                {
                    std::cout << "Program: " << it->id() << " executed to completion\n";
                    it = readyQueue.erase(it);
                }
                else
                {
                    it->setBurstTime(it->burstTime() - quantumTime);
                    std::cout << "Program: " << it->id() << " ran, remaining burst = " << it->burstTime() << "\n";
                    ++it;
                }
            }
        }
        std::cout << "ROUND ROBINED!!!\n";
    }
};
 
int main()
{
    std::vector<Instr> my_program = {
        {Opcode::ADD, {std::string("r1"), std::string("r1"), 10.0f}}, // r1 = r1 + 10
        {Opcode::ADD, {std::string("r2"), std::string("r1"), 5.0f}},  // r2 = r1 + 5
        {Opcode::MOV, {std::string("r3"), std::string("r2")}},        // r3 = r2
        {Opcode::LD, {std::string("r4"),std::string("g1") }},
        {Opcode::ST, {std::string("g2"),std::string("r4") }},
        {Opcode::HALT, {}}};
    GPU gpu(my_program);
     for(auto& thread : gpu.all_threads) {
        
        thread->_registers[getRegisterName("r1")] = static_cast<float>(thread->id() * 1);
    }
    gpu.global_memory[0] = 13;
    gpu.run();
    std::cout << "\n--- Final Register States ---" << std::endl;
    std::cout << "\nGLOBAL MEMORY\n";
    
    for (const auto& thread : gpu.all_threads) {
        thread->printRegisters();
    }
    gpu.print_global_mem();
    std::cout << std::endl;
    std::cout << gpu.sms[0].warps.size() << std::endl;
}

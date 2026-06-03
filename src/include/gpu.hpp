#pragma once
#include "instruction.hpp"
#include "vartable.hpp"
#include "labeltable.hpp"
#include <vector>
#include <memory>
#include <config.hpp>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <string>
#include <bitset>
struct Splinter{
    size_t pc;
    std::bitset<WARP_SIZE> mask;
};
class Thread {
public:
    int id_;
    bool active;
    std::vector<float> _registers;
    Instr instruction;
    int predicateReg;
    explicit Thread(int id);
    int id() const { return id_; }
    void printRegisters() const;
    void set_instruction(Instr instr);
};

class Warp {
public:
    int id_;

    std::vector<Splinter> splinters;
    std::vector<std::shared_ptr<Thread>> threads;
    std::vector<float> memory;
    Warp();
    bool isFinished() const;
    void addThread(std::shared_ptr<Thread> thread);
    void print_sharedMem() const;
};

class SM {
public:
    int id;
    std::vector<Warp> warps;
    std::vector<float>& globalMemory;
    VarTable& vars;
    LabelTable& labels;
    size_t shared_pc;
    SM(int sm_id, std::vector<float>& memory, VarTable& vars, LabelTable& labels);
    void addWarp(const Warp& warp);
    void cycle(const std::vector<Instr>& program);
private:
    void execute(Warp& warp, const std::vector<Instr>& program);
};

class GPU {
public:
    std::vector<float> global_memory;
    VarTable vars;
    LabelTable labels;
    std::vector<SM> sms;
    std::vector<std::shared_ptr<Thread>> all_threads;
    std::vector<Instr> program;
    std::unordered_map<std::string, int> initial_labels;
    long long cycle_count;

    std::thread worker;
    std::mutex mtx;
    std::atomic<bool> running{false};
    std::atomic<bool> finished{false};

    GPU(const std::vector<Instr>& program);
    ~GPU();

    void run();

    void stop();

    void loadProgram(std::vector<Instr> instrs, std::unordered_map<std::string, int> labels_map);

    void print_shared_mem() const;
    void print_global_mem() const;
    int get_cycle() const;
    void reset();
};

#pragma once
#include "instruction.hpp"
#include <vector>
#include <memory>
#include <config.hpp>
class Thread {
public:
    static int _id;
    size_t pc;
    int id_;
    bool active;
    std::vector<float> _registers;
    Instr intruction;
    Thread();
    int id() const { return id_; }
    void printRegisters() const;
    void set_instruction(Instr instr);
};

class Warp {
public:
    int id_;
    std::vector<std::shared_ptr<Thread>> threads;
    std::vector<float> memory;
    Warp();
    bool isFinished() const;
    void addThread(std::shared_ptr<Thread> thread);
    void wSharedMem() const;
};

class SM {
public:
    int id;
    std::vector<Warp> warps;
    std::vector<float>& globalMemory;
    SM(int sm_id, std::vector<float>& memory);
    void addWarp(const Warp& warp);
    void cycle(const std::vector<Instr>& program);
private:
    void execute(Warp& warp, const Instr& instruction);
};

class GPU {
public:
    std::vector<float> global_memory;
    std::vector<SM> sms;
    std::vector<std::shared_ptr<Thread>> all_threads;
    std::vector<Instr> program;
    GPU(const std::vector<Instr>& program);
    void run();
    void print_shared_mem() const;
    void print_global_mem() const;
};

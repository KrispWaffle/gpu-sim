#include "gpu.hpp"
#include "operations.hpp"
#include <iostream>
#include <algorithm>
#include "vartable.hpp"
int Thread::_id = 0;

Thread::Thread() : id_(_id++), pc(0), _registers(NUM_REGISTERS, 0.0f), active(true), predicateReg(0) {}
void Thread::printRegisters() const {
    std::cout << "\nTHREAD: " << id_ << "\n";
    for (size_t x = 0; x < _registers.size(); x++) {
        std::cout << "REG: " << x << " VALUE: " << _registers[x] << "\n";
    }
}
void Thread::set_instruction(Instr instr) {
    instruction = std::move(instr);
}

Warp::Warp() : id_(0), memory(GLOBAL_MEM_SIZE, 0.0f) {
    static int next_id = 0;
    id_ = next_id++;
}

bool Warp::isFinished() const {
    for (const auto& t : threads) {
        if (t->active) return false;
    }
    return true;
}

void Warp::addThread(std::shared_ptr<Thread> thread) {
    threads.push_back(thread);
}

void Warp::print_sharedMem() const {
    for (const auto& i : memory) {
        std::cout << i << ", ";
    }
    std::cout << "\n";
}

SM::SM(int sm_id, std::vector<float>& memory) : id(sm_id), globalMemory(memory), shared_pc(0) {}

void SM::addWarp(const Warp& warp) {
    warps.push_back(warp);
}

void SM::cycle(const std::vector<Instr>& program) {
    for (auto& warp : warps) {
        if (warp.isFinished()) continue;

        shared_pc = 0;
        for (const auto& t : warp.threads) {
            if (t->active) {
                shared_pc = t->pc;
                break;
            }
        }

        const Instr& instruction = program[shared_pc];

        execute(warp, instruction);
    }
}

void SM::execute(Warp& warp, const Instr& instruction) {
    HandlerFn fn = opcode_handlers[static_cast<int>(instruction.op)];
    for (auto& thread : warp.threads) {
        if (!thread->active) continue;
        fn(*thread, warp, globalMemory, instruction);
        if (thread->active) thread->pc++;
    }
}

GPU::GPU(const std::vector<Instr>& program) : program(program), global_memory(GLOBAL_MEM_SIZE, 0.0f), cycle_count(0) {
    sms.emplace_back(0, global_memory);
    for (int i = 0; i < NUM_THREADS; i++) {
        Thread t;
        all_threads.push_back(std::make_shared<Thread>(t));
    }
    for (int i = 0; i < NUM_THREADS; i += WARP_SIZE) {
        Warp new_warp;
        for (int j = 0; j < WARP_SIZE && (i + j) < NUM_THREADS; j++) {
            new_warp.addThread(all_threads[i + j]);
        }
        sms[0].addWarp(new_warp);
    }
}

GPU::~GPU() {
    stop(); 
}

void GPU::run()
{
    running = true;
    finished = false;

    worker = std::thread([this]() {
        std::cout << "--- Simulation Starting ---"<< std::endl;
        bool all_sms_finished = false; 

        while (running && !all_sms_finished) {
            all_sms_finished = true;
            {
                std::lock_guard<std::mutex> lock(mtx);
                for (auto& sm : sms) {
                    sm.cycle(program);
                    for (const auto& warp : sm.warps) {
                        if (!warp.isFinished()) {
                            all_sms_finished = false;
                        }
                    }
                }
                for(auto& var:VarTable::getInstance().table){
                    switch (var.second.loc)
                    {
                    case StoreLoc::GLOBAL:
                        var.second.value = global_memory[var.second.offset];
                        break;
                    case StoreLoc::LOCAL:
                        //Need to fix this 
                        // var.second.value = all_threads[var.second.offset];
                        break;
                    case StoreLoc::SHARED:
                        //temp solution
                        var.second.value = this->sms[0].warps[0].memory[var.second.offset];
                        break;
                    default:
                        break;
                    }
                }
            cycle_count++;
                std::cout.flush(); 
                
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_TIME)); 
        }

        finished = true;
        std::cout << "\n--- Simulation Finished in " << cycle_count << " cycles ---"<< std::endl;
    });

}

void GPU::stop()
{
    running = false;
    if (worker.joinable()) worker.join();
}

void GPU::print_shared_mem() const {
    std::cout << "\n";
    for (const auto& sm : sms) {
        for (const auto& warp : sm.warps) {
            warp.print_sharedMem();
        }
    }
    std::cout << "\n";
}

void GPU::print_global_mem() const {
    for (const auto& m : global_memory) {
        std::cout << m << ", ";
    }
    std::cout << "\n";
}
int GPU::get_cycle() const
{
    return cycle_count;
}

void GPU::reset() {
    cycle_count = 0;
    for (auto& t : all_threads) {
        t->pc = 0;
        t->active = true;
        std::fill(t->_registers.begin(), t->_registers.end(), 0.0f);
    }
    std::fill(global_memory.begin(), global_memory.end(), 0.0f);
    for (auto& sm : sms) {
        for (auto& warp : sm.warps) {
            std::fill(warp.memory.begin(), warp.memory.end(), 0.0f);
        }
    }
}
#include "gpu.hpp"
#include "operations.hpp"
#include "execution.hpp"
#include <iostream>
#include <algorithm>
static const char *errorName(ErrorCode e)
{
    switch (e)
    {
    case ErrorCode::None:
        return "None";
    case ErrorCode::GlobalOutOfBounds:
        return "GlobalOutOfBounds";
    case ErrorCode::SharedOutOfBounds:
        return "SharedOutOfBounds";
    case ErrorCode::InvalidMemorySpace:
        return "InvalidMemorySpace";
    case ErrorCode::DivByZero:
        return "DivByZero";
    case ErrorCode::StringReq:
        return "StringReq";
    case ErrorCode::VarNotFound:
        return "VarNotFound";
    case ErrorCode::BadOperand:
        return "BadOperand";
    }
    return "?";
}

Thread::Thread(int id) : id_(id), active(true), _registers(NUM_REGISTERS, 0.0f), predicateReg(0) {}
void Thread::printRegisters() const
{
    std::cout << "\nTHREAD: " << id_ << "\n";
    for (size_t x = 0; x < _registers.size(); x++)
    {
        std::cout << "REG: " << x << " VALUE: " << _registers[x] << "\n";
    }
}
void Thread::set_instruction(Instr instr)
{
    instruction = std::move(instr);
}

Warp::Warp() : id_(0), memory(WARP_SIZE, 0.0f)
{
    static int next_id = 0;
    id_ = next_id++;
}

bool Warp::isFinished() const
{
    return splinters.empty();
}

void Warp::addThread(std::shared_ptr<Thread> thread)
{
    if (splinters.empty())
        splinters.push_back({0, std::bitset<WARP_SIZE>()});
    splinters[0].mask.set(threads.size());
    threads.push_back(thread);
}

void Warp::print_sharedMem() const
{
    for (const auto &i : memory)
    {
        std::cout << i << ", ";
    }
    std::cout << "\n";
}

SM::SM(int sm_id, std::vector<float> &memory, VarTable &v, LabelTable &l)
    : id(sm_id), globalMemory(memory), vars(v), labels(l), shared_pc(0) {}

void SM::addWarp(const Warp &warp)
{
    warps.push_back(warp);
}

void SM::cycle(const std::vector<Instr> &program)
{
    for (auto &warp : warps)
    {
        if (warp.isFinished())
            continue;
        execute(warp, program);
    }
}
/*

void SM::execute(Warp& warp, const std::vector<Instr>& program) {
    for (auto& thread : warp.threads) {
        if (!thread->active) continue;
        if (thread->pc >= program.size()) {
            thread->active = false;
            continue;
        }
        const Instr& instruction = program[thread->pc];
        HandlerFn fn = opcode_handlers[static_cast<int>(instruction.op)];
        ExecutionContext ctx{*thread, warp, globalMemory, vars, labels};
        ErrorCode err = fn(ctx, instruction);
        if (err != ErrorCode::None) {
            std::cout << "\n[T" << thread->id() << "] ERROR: " << errorName(err)
                      << " — halting thread\n";
            thread->active = false;
        }
        if (thread->active) thread->pc++;
        shared_pc = thread->pc;
    }
}
*/
static void mergeSplinters(Warp &warp)
{
    for (size_t i = 0; i < warp.splinters.size(); ++i)
    {
        for (size_t j = i + 1; j < warp.splinters.size();)
        {
            if (warp.splinters[j].pc == warp.splinters[i].pc)
            {
                warp.splinters[i].mask |= warp.splinters[j].mask;
                warp.splinters.erase(warp.splinters.begin() + j);
            }
            else
            {
                ++j;
            }
        }
    }
}
void SM::execute(Warp &warp, const std::vector<Instr> &program)
{
    if (warp.splinters.empty())
    {
        return;
    }
    auto it = std::min_element(warp.splinters.begin(), warp.splinters.end(), [](const Splinter &a, const Splinter &b)
                               { return a.pc < b.pc; });
    Splinter &S = *it;
    if (S.pc >= program.size())
    {
        warp.splinters.erase(it);
        return;
    }
    const Instr &instr = program[S.pc];
    if (instr.op != Opcode::JMP)
    {
        for (size_t t = 0; t < warp.threads.size(); ++t)
        {
            if (!S.mask.test(t))
                continue;
            Thread &th = *warp.threads[t];
            ExecutionContext ctx{th, warp, globalMemory, vars, labels};
            ErrorCode err = opcode_handlers[(int)instr.op](ctx, instr);
            if (err != ErrorCode::None)
            {
                std::cout << "[T" << th.id() << "] ERROR: " << errorName(err) << "\n";
                S.mask.reset(t);
            }
        }
    }
    if (instr.op == Opcode::HALT)
    {
        warp.splinters.erase(it); 
        return;
    }
    else if (instr.op == Opcode::JMP)
    {
        const std::string *name = std::get_if<std::string>(&instr.src[0]);
        auto target = labels.getLabel(*name);
        if (!target)
        {
            S.pc++;
            return;
        }
        size_t jump_pc = static_cast<size_t>(*target);
        size_t next_pc = S.pc + 1;

        std::bitset<WARP_SIZE> takers, fallers;
        for (size_t t = 0; t < warp.threads.size(); ++t)
        {
            if (!S.mask.test(t))
                continue;
            if (warp.threads[t]->predicateReg)
                takers.set(t);
            else
                fallers.set(t);
        }

        if (takers.none())
        {
            S.pc = next_pc;
        }
        else if (fallers.none())
        {
            S.pc = jump_pc;
        } 
        else
        {
            S.pc = next_pc;
            S.mask = fallers;
            warp.splinters.push_back({jump_pc, takers});
        }
    }
    else
    {
        S.pc++;
    }

    mergeSplinters(warp);
}
GPU::GPU(const std::vector<Instr> &program) : program(program), global_memory(GLOBAL_MEM_SIZE, 0.0f), cycle_count(0)
{
    sms.emplace_back(0, global_memory, vars, labels);
    for (int i = 0; i < NUM_THREADS; i++)
    {
        all_threads.push_back(std::make_shared<Thread>(i));
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

GPU::~GPU()
{
    stop();
}

void GPU::run()
{
    reset();
    running = true;
    finished = false;

    worker = std::thread([this]()
                         {
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
                for(auto& var : vars.table){
                    int off = var.second.threadIDX ? 0 : var.second.offset;
                    switch (var.second.loc)
                    {
                    case StoreLoc::GLOBAL:
                        if (off >= 0 && off < (int)global_memory.size())
                            var.second.value = global_memory[off];
                        break;
                    case StoreLoc::LOCAL:
                        if (off >= 0 && off < (int)all_threads[0]->_registers.size())
                            var.second.value = all_threads[0]->_registers[off];
                        break;
                    case StoreLoc::SHARED:
                        if (off >= 0 && off < (int)sms[0].warps[0].memory.size())
                            var.second.value = sms[0].warps[0].memory[off];
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
        std::cout << "\n--- Simulation Finished in " << cycle_count << " cycles ---"<< std::endl; });
}

void GPU::stop()
{
    running = false;
    if (worker.joinable())
        worker.join();
}

void GPU::print_shared_mem() const
{
    std::cout << "\n";
    for (const auto &sm : sms)
    {
        for (const auto &warp : sm.warps)
        {
            warp.print_sharedMem();
        }
    }
    std::cout << "\n";
}

void GPU::print_global_mem() const
{
    for (const auto &m : global_memory)
    {
        std::cout << m << ", ";
    }
    std::cout << "\n";
}
int GPU::get_cycle() const
{
    return cycle_count;
}

void GPU::reset()
{
    stop();
    finished = false;
    std::lock_guard<std::mutex> lock(mtx);

    cycle_count = 0;
    for (auto &sms : this->sms)
    {
        sms.shared_pc = 0;
    }
    for (auto &t : all_threads)
    {
        t->active = true;
        std::fill(t->_registers.begin(), t->_registers.end(), 0.0f);
    }
    std::fill(global_memory.begin(), global_memory.end(), 0.0f);
    for (auto &sm : sms)
    {
        for (auto &warp : sm.warps)
        {
            std::fill(warp.memory.begin(), warp.memory.end(), 0.0f);
            warp.splinters.clear();
            std::bitset<WARP_SIZE> mask;
            for (size_t t = 0; t < warp.threads.size(); ++t)
                mask.set(t);
            warp.splinters.push_back({0, mask});
        }
    }

    vars.clear();
    labels.clear();
    for (auto &kv : initial_labels)
        labels.addLabel(kv.first, kv.second);
}

void GPU::loadProgram(std::vector<Instr> instrs, std::unordered_map<std::string, int> labels_map)
{
    stop();
    std::lock_guard<std::mutex> lock(mtx);
    program = std::move(instrs);
    initial_labels = std::move(labels_map);
}
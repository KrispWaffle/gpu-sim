#include "gpu.hpp"
#include "operations.hpp"
#include "execution.hpp"
#include <iostream>
#include <algorithm>
#include <stdexcept>

namespace {
class SimulationExecutionError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
}

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

Thread::Thread(int id, int numRegisters) : id_(id), active(true), _registers(numRegisters, 0.0f), predicateReg(0) {}
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

Warp::Warp(int id, int warpSize) : id_(id), memory(warpSize, 0.0f) {}

bool Warp::isFinished() const
{
    return splinters.empty();
}

void Warp::addThread(std::shared_ptr<Thread> thread)
{
    if (splinters.empty())
        splinters.push_back({0, std::bitset<MAX_WARP_SIZE>()});
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
    : id(sm_id), globalMemory(memory), vars(v), labels(l) {}

void SM::addWarp(const Warp &warp)
{
    warps.push_back(warp);
}

void SM::cycle(const std::vector<Instr> &program, bool logging)
{
    for (auto &warp : warps)
    {
        if (warp.isFinished() || warp.atBarrier)
            continue;
        if (warp.stallCycles > 0)
        {
            warp.stallCycles--;
            if (stats) stats->stallCycles++;
            continue;
        }
        execute(warp, program, logging);
    }
}


static bool touchesGlobal(const Instr &instr, const VarTable &vars)
{
    if (instr.op == Opcode::DEF) return false;
    for (const auto &op : instr.src)
    {
        if (auto pm = std::get_if<MemRef>(&op))
        {
            if (pm->space == StoreLoc::GLOBAL) return true;
        }
        else if (auto ps = std::get_if<std::string>(&op))
        {
            if (ps->size() > 2 && ps->compare(0, 2, "gm") == 0) return true;
            if (auto ov = vars.getVar(*ps); ov && ov->loc == StoreLoc::GLOBAL) return true;
        }
    }
    return false;
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
        HandlerFn fn = opcodeHandlers[static_cast<int>(instruction.op)];
        ExecutionContext ctx{*thread, warp, globalMemory, vars, labels};
        ErrorCode err = fn(ctx, instruction);
        if (err != ErrorCode::None) {
            std::cout << "\n[T" << thread->id() << "] ERROR: " << errorName(err)
                      << " â€” halting thread\n";
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
void SM::execute(Warp &warp, const std::vector<Instr> &program, bool logging)
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
    if (stats)
    {
        stats->instructionsIssued += (long long)S.mask.count();
        stats->issueSlots += (long long)warp.threads.size();
    }
    if (instr.op != Opcode::JMP && instr.op != Opcode::JMPU && instr.op != Opcode::BAR)
    {
        for (size_t t = 0; t < warp.threads.size(); ++t)
        {
            if (!S.mask.test(t))
                continue;
            Thread &th = *warp.threads[t];
            ExecutionContext ctx{th, warp, globalMemory, vars, labels, logging};
            const auto& handlers = opcodeHandlers();
            const size_t opcode = static_cast<size_t>(instr.op);
            HandlerFn handler = opcode < handlers.size() ? handlers[opcode] : nullptr;
            const std::string where = "line " + std::to_string(instr.ln) +
                                      ", thread " + std::to_string(th.id()) + ": ";
            if (!handler)
                throw SimulationExecutionError(where + "missing opcode handler");
            try
            {
                ErrorCode err = handler(ctx, instr);
                if (err != ErrorCode::None)
                    throw SimulationExecutionError(where + errorName(err));
            }
            catch (const SimulationExecutionError&)
            {
                throw;
            }
            catch (const std::exception& e)
            {
                throw SimulationExecutionError(where + e.what());
            }
            catch (...)
            {
                throw SimulationExecutionError(where + "unknown instruction error");
            }
        }
        if (globalLatency > 1 && touchesGlobal(instr, vars))
            warp.stallCycles = globalLatency - 1;
    }
    if (instr.op == Opcode::HALT)
    {
        warp.splinters.erase(it);
        return;
    }
    else if (instr.op == Opcode::BAR)
    {
        if (logging && warp.splinters.size() > 1)
            std::cout << "[W" << warp.id_ << "] WARNING: divergent barrier â€” "
                      << "some lanes already passed it\n";
        warp.atBarrier = true;
        return;
    }
    else if (instr.op == Opcode::JMP || instr.op == Opcode::JMPU)
    {
        const std::string *name = instr.src.empty() ? nullptr : std::get_if<std::string>(&instr.src[0]);
        if (!name)
            throw SimulationExecutionError("line " + std::to_string(instr.ln) + ": BadOperand");
        auto target = labels.getLabel(*name);
        if (!target || *target < 0 || static_cast<size_t>(*target) > program.size())
            throw SimulationExecutionError("line " + std::to_string(instr.ln) + ": invalid jump target");
        size_t jump_pc = static_cast<size_t>(*target);
        size_t next_pc = S.pc + 1;

        if (instr.op == Opcode::JMPU)
        {
            S.pc = jump_pc;
            mergeSplinters(warp);
            return;
        }

        std::bitset<MAX_WARP_SIZE> takers, fallers;
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
            if (stats) stats->divergenceEvents++;
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
GPU::GPU(const std::vector<Instr> &program) : program(program), cycle_count(0)
{
    configure(SimConfig{});
}

void GPU::configure(const SimConfig &config)
{
    stop();
    std::lock_guard<std::mutex> lock(mtx);

    cfg = config;
    cfg.numThreads    = std::max(1, cfg.numThreads);
    cfg.warpSize      = std::clamp(cfg.warpSize, 1, MAX_WARP_SIZE);
    cfg.numSMs        = std::max(1, cfg.numSMs);
    cfg.numRegisters  = std::max(1, cfg.numRegisters);
    cfg.globalMemSize = std::max(1, cfg.globalMemSize);
    cfg.globalLatency = std::max(0, cfg.globalLatency);

    all_threads.clear();
    sms.clear();
    global_memory.assign(cfg.globalMemSize, 0.0f);

    for (int i = 0; i < cfg.numThreads; i++)
        all_threads.push_back(std::make_shared<Thread>(i, cfg.numRegisters));

    for (int s = 0; s < cfg.numSMs; s++)
    {
        sms.emplace_back(s, global_memory, vars, labels);
        sms.back().stats = &stats;
        sms.back().globalLatency = cfg.globalLatency;
    }

    int numWarps = 1 + (cfg.numThreads - 1) / cfg.warpSize;
    for (int w = 0; w < numWarps; w++)
    {
        Warp new_warp(w, cfg.warpSize);
        for (int j = 0; j < cfg.warpSize && (w * cfg.warpSize + j) < cfg.numThreads; j++)
        {
            new_warp.addThread(all_threads[w * cfg.warpSize + j]);
        }
        sms[w % cfg.numSMs].addWarp(new_warp);
    }

    resetLocked();
}

std::tuple<int, int, int> GPU::locateThread(int tid) const
{
    int w = tid / cfg.warpSize;
    return {w % cfg.numSMs, w / cfg.numSMs, tid % cfg.warpSize};
}

GPU::~GPU()
{
    stop();
}

bool GPU::allFinishedLocked() const
{
    for (const auto& sm : sms)
        for (const auto& warp : sm.warps)
            if (!warp.isFinished()) return false;
    return true;
}

void GPU::recordHistoryLocked()
{
    if (history.size() >= HISTORY_CAP) return;
    int numWarps = 1 + (cfg.numThreads - 1) / cfg.warpSize;
    std::vector<WarpCycleRecord> rec(numWarps);
    for (const auto& sm : sms)
        for (const auto& warp : sm.warps)
            rec[warp.id_] = {warp.splinters, warp.stallCycles > 0, warp.atBarrier};
    history.push_back(std::move(rec));
}

void GPU::releaseBarriersLocked()
{
    bool anyAtBar = false, allAtBar = true;
    for (const auto& sm : sms)
        for (const auto& warp : sm.warps)
        {
            if (warp.isFinished()) continue;
            if (warp.atBarrier) anyAtBar = true;
            else allAtBar = false;
        }
    if (!anyAtBar || !allAtBar) return;

    for (auto& sm : sms)
        for (auto& warp : sm.warps)
        {
            if (warp.isFinished() || !warp.atBarrier) continue;
            warp.atBarrier = false;
            auto it = std::min_element(
                warp.splinters.begin(), warp.splinters.end(),
                [](const Splinter& a, const Splinter& b) { return a.pc < b.pc; });
            it->pc++;
            mergeSplinters(warp);
        }
}

void GPU::refreshVariablesLocked()
{
    for (auto& var : vars.table)
    {
        int off = var.second.threadIDX ? 0 : var.second.offset;
        switch (var.second.loc)
        {
        case StoreLoc::GLOBAL:
            if (off >= 0 && off < (int)global_memory.size())
                var.second.value = global_memory[off];
            break;
        case StoreLoc::LOCAL:
            if (!all_threads.empty() && off >= 0 && off < (int)all_threads[0]->_registers.size())
                var.second.value = all_threads[0]->_registers[off];
            break;
        case StoreLoc::SHARED:
            if (!sms.empty() && !sms[0].warps.empty() &&
                off >= 0 && off < (int)sms[0].warps[0].memory.size())
                var.second.value = sms[0].warps[0].memory[off];
            break;
        }
    }
}

SimulationResult GPU::execute(const SimulationOptions& options) noexcept
{
    SimulationResult result;
    try
    {
        if (options.logging)
            std::cout << "--- Simulation Starting ---" << std::endl;

        while (running.load())
        {
            if (options.interactive && paused.load() && pendingSteps.load() == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            bool stepping = options.interactive && pendingSteps.load() > 0;
            bool complete = false;
            {
                std::lock_guard<std::mutex> lock(mtx);
                for (auto& sm : sms)
                    for (auto& warp : sm.warps)
                        std::erase_if(warp.splinters, [this](const Splinter& s) {
                            return s.pc >= program.size();
                        });
                if (allFinishedLocked()) break;
                if (options.maxCycles > 0 && cycle_count >= options.maxCycles)
                {
                    result.status = SimulationStatus::CycleLimit;
                    result.cycles = cycle_count;
                    result.diagnostic = "cycle limit reached (" + std::to_string(options.maxCycles) + ")";
                    break;
                }
                if (options.captureHistory) recordHistoryLocked();
                for (auto& sm : sms) sm.cycle(program, options.logging);
                releaseBarriersLocked();
                refreshVariablesLocked();
                cycle_count++;
                for (auto& sm : sms)
                    for (auto& warp : sm.warps)
                        std::erase_if(warp.splinters, [this](const Splinter& s) {
                            return s.pc >= program.size();
                        });
                complete = allFinishedLocked();
                result.cycles = cycle_count;
            }

            if (options.logging) std::cout.flush();
            if (stepping)
                pendingSteps.fetch_sub(1);
            else if (options.interactive && !complete)
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs.load()));

            if (complete) break;
        }

        std::lock_guard<std::mutex> lock(mtx);
        if (!running.load() && !allFinishedLocked())
        {
            result.status = SimulationStatus::Stopped;
            result.diagnostic = "simulation stopped";
        }
        if (options.logging)
            std::cout << "\n--- Simulation Finished in " << cycle_count << " cycles ---" << std::endl;
    }
    catch (const SimulationExecutionError& e)
    {
        result.status = SimulationStatus::ExecutionError;
        result.diagnostic = e.what();
    }
    catch (const std::exception& e)
    {
        result.status = SimulationStatus::InternalError;
        result.diagnostic = e.what();
    }
    catch (...)
    {
        result.status = SimulationStatus::InternalError;
        result.diagnostic = "unknown simulation error";
    }

    if (options.logging && (result.status == SimulationStatus::ExecutionError ||
                            result.status == SimulationStatus::InternalError))
        std::cout << "ERROR: " << result.diagnostic << std::endl;

    result.cycles = cycle_count;
    return result;
}

void GPU::run(bool startPaused, bool doReset)
{
    stop();
    if (doReset)
    {
        std::lock_guard<std::mutex> lock(mtx);
        resetLocked();
    }
    running = true;
    finished = false;
    paused = startPaused;
    pendingSteps = 0;

    SimulationOptions options;
    try
    {
        worker = std::thread([this, options]() {
            SimulationResult result = execute(options);
            {
                std::lock_guard<std::mutex> lock(mtx);
                lastRunResult = std::move(result);
            }
            running = false;
            finished = true;
        });
    }
    catch (...)
    {
        running = false;
        finished = true;
        throw;
    }
}

SimulationResult GPU::runSynchronous(const SimulationOptions& options, bool doReset)
{
    stop();
    if (doReset)
    {
        std::lock_guard<std::mutex> lock(mtx);
        resetLocked();
    }
    running = true;
    finished = false;
    paused = false;

    SimulationOptions synchronousOptions = options;
    synchronousOptions.interactive = false;
    pendingSteps = 0;
    SimulationResult result = execute(synchronousOptions);
    {
        std::lock_guard<std::mutex> lock(mtx);
        lastRunResult = result;
    }
    running = false;
    finished = true;
    return result;
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
    std::lock_guard<std::mutex> lock(mtx);
    resetLocked();
}

void GPU::resetLocked()
{
    finished = false;
    cycle_count = 0;
    stats = SimStats{};
    history.clear();
    pendingSteps = 0;
    for (auto &t : all_threads)
    {
        t->active = true;
        t->predicateReg = 0;
        std::fill(t->_registers.begin(), t->_registers.end(), 0.0f);
    }
    std::fill(global_memory.begin(), global_memory.end(), 0.0f);
    for (auto &sm : sms)
    {
        for (auto &warp : sm.warps)
        {
            std::fill(warp.memory.begin(), warp.memory.end(), 0.0f);
            warp.atBarrier = false;
            warp.stallCycles = 0;
            warp.splinters.clear();
            std::bitset<MAX_WARP_SIZE> mask;
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

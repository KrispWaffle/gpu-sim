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
#include <tuple>
struct Splinter{
    size_t pc;
    std::bitset<MAX_WARP_SIZE> mask;
};
class Thread {
public:
    int id_;
    bool active;
    std::vector<float> _registers;
    Instr instruction;
    int predicateReg;
    Thread(int id, int numRegisters);
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
    bool atBarrier = false;
    int stallCycles = 0;
    Warp(int id, int warpSize);
    bool isFinished() const;
    void addThread(std::shared_ptr<Thread> thread);
    void print_sharedMem() const;
};

struct WarpCycleRecord {
    std::vector<Splinter> splinters;
    bool stalled = false;
    bool atBarrier = false;
};
constexpr size_t HISTORY_CAP = 8192;

struct SimStats {
    long long instructionsIssued = 0;  // lanes that actually executed an instruction
    long long issueSlots = 0;          // warpSize per warp-issue (efficiency denominator)
    long long divergenceEvents = 0;    // splinter splits at JMP
    long long stallCycles = 0;         // warp-cycles lost to memory latency
};

enum class SimulationStatus {
    Completed,
    Stopped,
    CycleLimit,
    ExecutionError,
    InternalError,
};

struct SimulationResult {
    SimulationStatus status = SimulationStatus::Completed;
    long long cycles = 0;
    std::string diagnostic;

    bool ok() const { return status == SimulationStatus::Completed; }
};

struct SimulationOptions {
    long long maxCycles = 0;
    bool captureHistory = true;
    bool logging = true;
    bool interactive = true;
};

class SM {
public:
    int id;
    std::vector<Warp> warps;
    std::vector<float>& globalMemory;
    VarTable& vars;
    LabelTable& labels;
    SimStats* stats = nullptr;
    int globalLatency = 0;
    SM(int sm_id, std::vector<float>& memory, VarTable& vars, LabelTable& labels);
    void addWarp(const Warp& warp);
    void cycle(const std::vector<Instr>& program, bool logging);
private:
    void execute(Warp& warp, const std::vector<Instr>& program, bool logging);
};

class GPU {
public:
    SimConfig cfg;
    std::vector<float> global_memory;
    VarTable vars;
    LabelTable labels;
    std::vector<SM> sms;
    std::vector<std::shared_ptr<Thread>> all_threads;
    std::vector<Instr> program;
    std::unordered_map<std::string, int> initial_labels;
    long long cycle_count;

    SimStats stats;
    // history[cycle][global warp id] — Timeline panel data
    std::vector<std::vector<WarpCycleRecord>> history;

    std::thread worker;
    std::mutex mtx;
    std::atomic<bool> running{false};
    std::atomic<bool> finished{false};
    std::atomic<bool> paused{false};
    std::atomic<int> pendingSteps{0};
    std::atomic<int> delayMs{DELAY_TIME};
    SimulationResult lastRunResult;

    GPU(const std::vector<Instr>& program);
    ~GPU();

    void run(bool startPaused = false, bool doReset = true);
    SimulationResult runSynchronous(const SimulationOptions& options = {}, bool doReset = true);

    void stop();

    void configure(const SimConfig& config);

    std::tuple<int, int, int> locateThread(int tid) const;

    void loadProgram(std::vector<Instr> instrs, std::unordered_map<std::string, int> labels_map);

    void print_shared_mem() const;
    void print_global_mem() const;
    int get_cycle() const;
    void reset();

private:
    SimulationResult execute(const SimulationOptions& options) noexcept;
    bool allFinishedLocked() const;
    void recordHistoryLocked();
    void releaseBarriersLocked();
    void refreshVariablesLocked();
    void resetLocked();
};

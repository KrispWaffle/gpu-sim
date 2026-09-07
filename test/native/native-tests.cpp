#include "gsim.h"
#include "gpu.hpp"
#include "parser.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

class LogCapture : public std::stringbuf {
public:
    std::string flushed;
    int sync() override {
        flushed = str();
        return 0;
    }
};

static int checks = 0;
static void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
    ++checks;
}
static void load(GPU& gpu, const char* source) {
    auto p = parseProgram(source, gpu.cfg);
    gpu.loadProgram(std::move(p.instructions), std::move(p.labels));
    gpu.reset();
}
static void wait(GPU& gpu) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!gpu.finished && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    bool done = gpu.finished;
    gpu.stop();
    check(done, "worker completion timeout");
}
int main() {
    LogCapture quiet;
    auto* previous = std::cout.rdbuf(&quiet);
    try {
        float mem[8] = {10, 20, 30, 40, 50, 60, 70, 80};
        int cycles = -1;
        char err[256];
        auto run = [&](const char* source, int budget) {
            return gsim_run(source, mem, 8, 5, 2, 2, 8, 0, budget, &cycles, err, sizeof(err));
        };
        check(run("add gm[tidx] gm[tidx] .5\nhalt", 2) == GSIM_STATUS_SUCCESS, "numerical success");
        check(cycles == 2 && mem[0] == 10.5f && mem[4] == 50.5f && mem[5] == 60, "partial warp results");
        check(run("mov gm0 -2.5e+1", 1) == GSIM_STATUS_SUCCESS && cycles == 1 && mem[0] == -25, "fallthrough exact budget");
        check(run("; empty", 1) == GSIM_STATUS_SUCCESS && cycles == 0, "empty program cycles");
        check(run("mov gm0 99\nloop:\njmpu loop", 3) == GSIM_STATUS_CYCLE_LIMIT, "loop budget");
        check(cycles == 3 && mem[0] == -25 && std::string(err).find("cycle limit") != std::string::npos, "budget no copyback");
        check(run("mov gm0 99\ndiv r0 1 0\nhalt", 10) == GSIM_STATUS_EXECUTION_ERROR, "exception status");
        check(cycles == 1 && mem[0] == -25 && std::string(err).find("line 2, thread 0") != std::string::npos, "exception diagnostic no copyback");
        check(run("mov r0 missing_variable\nhalt", 10) == GSIM_STATUS_EXECUTION_ERROR && cycles == 0, "instruction error status");
        check(run("not_an_opcode", 10) == GSIM_STATUS_PARSE_ERROR && mem[0] == -25, "parse error");
        check(run("halt", 0) == GSIM_STATUS_INVALID_ARGUMENT && cycles == 0, "invalid budget");
        check(gsim_run(nullptr, mem, 8, 1, 1, 1, 1, 0, 1, &cycles, err, sizeof(err)) == GSIM_STATUS_INVALID_ARGUMENT, "null source");
        check(gsim_run("halt", nullptr, 8, 1, 1, 1, 1, 0, 1, &cycles, err, sizeof(err)) == GSIM_STATUS_INVALID_ARGUMENT, "null memory");
        for (int field = 0; field < 6; ++field) {
            int args[] = {8, 1, 1, 1, 1, 0};
            args[field] = field == 5 ? -1 : 0;
            check(gsim_run("halt", mem, args[0], args[1], args[2], args[3], args[4], args[5], 1, &cycles, err, sizeof(err)) == GSIM_STATUS_INVALID_ARGUMENT, "invalid dimension/latency");
        }
        check(gsim_run("halt", mem, 8, 1, 65, 1, 1, 0, 1, nullptr, nullptr, 0) == GSIM_STATUS_INVALID_ARGUMENT, "warp maximum");
        check(gsim_run("halt", mem, 8, 1, 1, 1, 1, 0, 1, nullptr, nullptr, 1) == GSIM_STATUS_INVALID_ARGUMENT, "null diagnostic buffer length");
        check(gsim_run("halt", mem, 8, 1, 1, 1, 1, 0, 1, nullptr, err, -1) == GSIM_STATUS_INVALID_ARGUMENT, "negative diagnostic length");
        char tiny = 'x';
        check(gsim_run(nullptr, mem, 8, 1, 1, 1, 1, 0, 1, nullptr, &tiny, 1) == GSIM_STATUS_INVALID_ARGUMENT && tiny == '\0', "truncated diagnostic");
        check(gsim_run("halt", mem, 8, 1, 1, 1, 1, 0, 1, nullptr, nullptr, 0) == GSIM_STATUS_SUCCESS, "optional outputs");
        check(quiet.str().empty(), "C ABI must be quiet");

        GPU gpu({});
        SimConfig cfg;
        cfg.numThreads = 5; cfg.warpSize = 2; cfg.numSMs = 2;
        cfg.globalLatency = 0;
        gpu.configure(cfg);
        gpu.delayMs = 0;
        const char* source = "cmp_lt tidx 3\njmp low\nmov gm[tidx] 20\njmpu end\nlow:\nmov gm[tidx] 10\nend:\nbar\nadd gm[tidx] gm[tidx] .5\nhalt";
        load(gpu, source);
        SimulationOptions opts;
        opts.maxCycles = 100; opts.captureHistory = false; opts.logging = false;
        auto result = gpu.runSynchronous(opts, false);
        check(result.ok() && gpu.history.empty(), "headless history disabled");
        auto expected = gpu.global_memory;
        check(expected[0] == 10.5f && expected[4] == 20.5f, "branch barrier arithmetic");
        gpu.run(false, true);
        wait(gpu);
        check(gpu.lastRunResult.ok() && gpu.lastRunResult.cycles == result.cycles && gpu.global_memory == expected, "async sync scheduler parity");
        check(gpu.history.size() == static_cast<size_t>(result.cycles), "GUI history");
        gpu.run(false, false);
        wait(gpu);
        check(gpu.lastRunResult.ok() && gpu.lastRunResult.cycles == result.cycles, "completed restart without reset");
        load(gpu, "halt");
        gpu.run(true, false);
        gpu.pendingSteps = 3;
        gpu.run(true, false);
        check(gpu.pendingSteps == 0, "restart clears stale steps");
        gpu.pendingSteps = 1;
        wait(gpu);
        check(gpu.lastRunResult.ok(), "paused worker step completion");
        gpu.all_threads[0]->predicateReg = 1;
        gpu.reset();
        check(gpu.all_threads[0]->predicateReg == 0, "reset predicate");
        load(gpu, "mov gm0 42\nhalt");
        cfg.globalLatency = 3;
        gpu.configure(cfg);
        opts.maxCycles = 3;
        result = gpu.runSynchronous(opts);
        check(result.status == SimulationStatus::CycleLimit && result.cycles == 3, "stall cycles consume budget");
        opts.maxCycles = 4;
        result = gpu.runSynchronous(opts);
        check(result.ok() && result.cycles == 4, "completion at stall boundary");
        for (const char* failure : {"div r0 1 0", "mov r0 missing_variable"}) {
            load(gpu, failure);
            quiet.str("");
            quiet.flushed.clear();
            opts.logging = false;
            result = gpu.runSynchronous(opts);
            check(result.status == SimulationStatus::ExecutionError && !result.diagnostic.empty(), "silent execution diagnostic retained");
            check(quiet.str().empty() && quiet.flushed.empty(), "headless execution error must be quiet");
            for (bool async : {false, true}) {
                quiet.str("");
                quiet.flushed.clear();
                if (async) {
                    gpu.run(false, true);
                    wait(gpu);
                    result = gpu.lastRunResult;
                } else {
                    opts.logging = true;
                    result = gpu.runSynchronous(opts);
                }
                check(result.status == SimulationStatus::ExecutionError &&
                      result.diagnostic.find("line 1, thread 0: ") == 0, "logged execution error context");
                const std::string message = "ERROR: " + result.diagnostic + "\n";
                const auto log = quiet.str();
                const auto pos = log.find(message);
                check(pos != std::string::npos, "execution diagnostic visible");
                check(log.find("ERROR:") == pos && log.find("ERROR:", pos + message.size()) == std::string::npos,
                      "execution diagnostic printed once");
                check(quiet.flushed == log, "execution diagnostic flushed for GUI capture");
            }
        }
        std::cout.rdbuf(previous);
        std::cout << "PASS " << checks << " native checks\n";
    } catch (const std::exception& e) {
        std::cout.rdbuf(previous);
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}

#include "gsim.h"
#include "gpu.hpp"
#include "parser.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

namespace {
void setError(char* err, int errlen, std::string_view message) noexcept
{
    if (!err || errlen <= 0) return;
    const size_t count = std::min(message.size(), static_cast<size_t>(errlen - 1));
    std::memcpy(err, message.data(), count);
    err[count] = '\0';
}

int invalidArgument(char* err, int errlen, const std::string& message)
{
    setError(err, errlen, "invalid argument: " + message);
    return GSIM_STATUS_INVALID_ARGUMENT;
}

bool exceedsSizeLimit(int count, int elementSize)
{
    return count > std::numeric_limits<int>::max() / elementSize;
}
}

extern "C" int gsim_run(const char* program_src,
                        float* mem, int memsize,
                        int numThreads, int warpSize, int numSMs,
                        int numRegisters, int globalLatency,
                        int maxCycles,
                        int* out_cycles,
                        char* err, int errlen)
{
    if (out_cycles) *out_cycles = 0;
    if (err && errlen > 0) err[0] = '\0';

    try
    {
    if (!program_src) return invalidArgument(err, errlen, "program_src is null");
    if (!mem) return invalidArgument(err, errlen, "mem is null");
    if (memsize <= 0) return invalidArgument(err, errlen, "memsize must be positive");
    if (exceedsSizeLimit(memsize, sizeof(float)))
        return invalidArgument(err, errlen, "memsize is too large");
    if (numThreads <= 0) return invalidArgument(err, errlen, "numThreads must be positive");
    if (warpSize <= 0 || warpSize > MAX_WARP_SIZE)
        return invalidArgument(err, errlen, "warpSize must be between 1 and " +
                                           std::to_string(MAX_WARP_SIZE));
    if (numSMs <= 0) return invalidArgument(err, errlen, "numSMs must be positive");
    if (numRegisters <= 0) return invalidArgument(err, errlen, "numRegisters must be positive");
    if (globalLatency < 0) return invalidArgument(err, errlen, "globalLatency must be non-negative");
    if (maxCycles <= 0) return invalidArgument(err, errlen, "maxCycles must be positive");
    if (!err && errlen != 0) return GSIM_STATUS_INVALID_ARGUMENT;
    if (errlen < 0) return invalidArgument(err, errlen, "errlen must be non-negative");

        SimConfig cfg;
        cfg.numThreads = numThreads;
        cfg.warpSize = warpSize;
        cfg.numSMs = numSMs;
        cfg.numRegisters = numRegisters;
        cfg.globalMemSize = memsize;
        cfg.globalLatency = globalLatency;

        GPU gpu({});
        gpu.configure(cfg);
        Program program = parseProgram(program_src, gpu.cfg);
        gpu.loadProgram(std::move(program.instructions), std::move(program.labels));

        gpu.reset();
        std::copy_n(mem, memsize, gpu.global_memory.begin());

        SimulationOptions options;
        options.maxCycles = maxCycles;
        options.captureHistory = false;
        options.logging = false;
        options.interactive = false;
        SimulationResult result = gpu.runSynchronous(options, false);

        const long long boundedCycles = std::min<long long>(result.cycles,
                                                            std::numeric_limits<int>::max());
        if (out_cycles) *out_cycles = static_cast<int>(boundedCycles);

        switch (result.status)
        {
        case SimulationStatus::Completed:
            std::copy_n(gpu.global_memory.begin(), memsize, mem);
            return GSIM_STATUS_SUCCESS;
        case SimulationStatus::CycleLimit:
            setError(err, errlen, result.diagnostic);
            return GSIM_STATUS_CYCLE_LIMIT;
        case SimulationStatus::ExecutionError:
            setError(err, errlen, result.diagnostic);
            return GSIM_STATUS_EXECUTION_ERROR;
        case SimulationStatus::Stopped:
        case SimulationStatus::InternalError:
            setError(err, errlen, result.diagnostic);
            return GSIM_STATUS_INTERNAL_ERROR;
        }
    }
    catch (const ParseError& e)
    {
        setError(err, errlen, e.what());
        return GSIM_STATUS_PARSE_ERROR;
    }
    catch (const std::exception& e)
    {
        setError(err, errlen, e.what());
        return GSIM_STATUS_INTERNAL_ERROR;
    }
    catch (...)
    {
        setError(err, errlen, "unknown native error");
        return GSIM_STATUS_INTERNAL_ERROR;
    }

    setError(err, errlen, "unknown simulation status");
    return GSIM_STATUS_INTERNAL_ERROR;
}

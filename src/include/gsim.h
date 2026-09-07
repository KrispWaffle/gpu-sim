#ifndef GSIM_H
#define GSIM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gsim_status {
    GSIM_STATUS_SUCCESS = 0,
    GSIM_STATUS_PARSE_ERROR = 1,
    GSIM_STATUS_CYCLE_LIMIT = 2,
    GSIM_STATUS_INVALID_ARGUMENT = 3,
    GSIM_STATUS_EXECUTION_ERROR = 4,
    GSIM_STATUS_INTERNAL_ERROR = 5
} gsim_status;

/*
 * Runs one NUL-terminated GSIM program synchronously.
 *
 * Required arguments:
 *   program_src must be non-NULL.
 *   mem must point to at least memsize floats and memsize must be positive.
 *   numThreads, numSMs, and numRegisters must be positive.
 *   warpSize must be in [1, 64], globalLatency must be non-negative, and
 *   maxCycles must be positive. maxCycles is checked at cycle boundaries, so
 *   no more than maxCycles cycles execute.
 *
 * Optional outputs:
 *   out_cycles may be NULL; otherwise it receives the number of completed
 *   cycles (including on execution or cycle-limit failures).
 *   err may be NULL only when errlen is zero. Otherwise errlen must be positive
 *   and err receives a NUL-terminated diagnostic, truncated to fit.
 *
 * mem is copied back only when GSIM_STATUS_SUCCESS is returned.
 */
int gsim_run(const char* program_src,
             float* mem, int memsize,
             int numThreads, int warpSize, int numSMs,
             int numRegisters, int globalLatency,
             int maxCycles,
             int* out_cycles,
             char* err, int errlen);

#ifdef __cplusplus
}
#endif

#endif

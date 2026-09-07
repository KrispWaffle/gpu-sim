#ifndef CONFIG_HPP
#define CONFIG_HPP
#include <cstddef>

constexpr int MAX_WARP_SIZE = 64;
constexpr int TIDX_RETURN_VAL = -1;
constexpr int DELAY_TIME = 50;

struct SimConfig {
    int numThreads    = 32;
    int warpSize      = 8;
    int numSMs        = 2;
    int numRegisters  = 8;
    int globalMemSize = 128;
    int globalLatency = 4;  
};
#endif

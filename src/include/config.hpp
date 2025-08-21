#ifndef CONFIG_HPP
#define CONFIG_HPP
constexpr int NUM_THREADS = 10;
constexpr int NUM_REGISTERS = 4;
constexpr int GLOBAL_MEM_SIZE = NUM_THREADS;
constexpr int WARP_SIZE = NUM_THREADS;
constexpr int SLEEP_TIME =1; // In seconds 
constexpr size_t NUM_OPCODES = 13; 
constexpr int NUM_VAR_LOCS=3;
constexpr int TIDX_RETURN_VAL = -1;
constexpr int DELAY_TIME = 50;  
#endif 
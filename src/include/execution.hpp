#pragma once
#include "instruction.hpp"
#include "gpu.hpp"

struct ExecutionContext {
    Thread& thread;
    Warp& warp;
    std::vector<float>& globalMem;
};

float fetch(const OpInfo& o, const ExecutionContext& ctx);
float eval(const OpInfo& lhs, const OpInfo& rhs, Opcode op, const ExecutionContext& ctx);
ErrorCode storeInLocation(OpInfo& dst, float result, ExecutionContext& ctx);

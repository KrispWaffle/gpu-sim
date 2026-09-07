#pragma once
#include "instruction.hpp"
#include "execution.hpp"
#include <array>

using HandlerFn = ErrorCode(*)(ExecutionContext&, const Instr&);
constexpr size_t OPCODE_COUNT = static_cast<size_t>(Opcode::BAR) + 1;

const std::array<HandlerFn, OPCODE_COUNT>& opcodeHandlers();
void setup_opcode_handlers();

ErrorCode _binary_(ExecutionContext& ctx, const Instr& instr);
ErrorCode _neg_(ExecutionContext& ctx, const Instr& instr);
ErrorCode _mov_(ExecutionContext& ctx, const Instr& instr);
ErrorCode _ld_(ExecutionContext& ctx, const Instr& instr);
ErrorCode _st_(ExecutionContext& ctx, const Instr& instr);
ErrorCode _halt_(ExecutionContext& ctx, const Instr& instr);
ErrorCode _def_(ExecutionContext& ctx, const Instr& instr);
ErrorCode _label_(ExecutionContext& ctx, const Instr& instr);
ErrorCode _cond_(ExecutionContext& ctx, const Instr& instr);
ErrorCode _jump_(ExecutionContext& ctx, const Instr& instr);
ErrorCode _bar_(ExecutionContext& ctx, const Instr& instr);

#pragma once
#include "instruction.hpp"
#include "gpu.hpp"
#include <array>

using HandlerFn = ErrorCode(*)(Thread&, Warp&, std::vector<float>&, const Instr&);
extern std::array<HandlerFn, 16> opcode_handlers;

void setup_opcode_handlers();

ErrorCode _add_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _sub_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _mul_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _div_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _neg_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _mov_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _ld_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _st_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _halt_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _def_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _label_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _cond_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _jump_(Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _and_ (Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _or_ (Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);
ErrorCode _xor_ (Thread& t, Warp& warp, std::vector<float>& global, const Instr& instr);

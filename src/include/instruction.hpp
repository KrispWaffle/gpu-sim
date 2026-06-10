#pragma once
#include <string>
#include <vector>
#include <variant>
#include <optional>

enum class Opcode { ADD, SUB, MUL, DIV, NEG, LD, ST, MOV, HALT, DEF, LABEL, JMP, CMP_LT, AND, OR, XOR,
                    JMPU, CMP_EQ, CMP_GT, BAR };
enum class StoreLoc { GLOBAL, SHARED, LOCAL };
enum class ErrorCode { None, GlobalOutOfBounds, SharedOutOfBounds, InvalidMemorySpace, DivByZero, StringReq, VarNotFound, BadOperand};
enum class OpKind { Constant, Register, Variable, Global, Shared, Invalid };

struct Variable {
    std::string name;
    float value;
    int offset;
    bool isConstant;
    bool threadIDX;
    StoreLoc loc;
};

struct MemRef {
    StoreLoc space;
    std::string idxTok;
};

using Operand = std::variant<Opcode, std::string, float, Variable, StoreLoc, int, MemRef>;

struct Instr {
    Opcode op;
    std::vector<Operand> src;
    int ln = 0;   
};

struct OpInfo {
    OpKind kind;
    float constVal;
    size_t index;
    Variable var;
};

struct ExecutionContext;

int getRegisterName(const std::string& reg);
int getMemoryLocation(const std::string& mem);
OpInfo decodeOperand(const Operand& op, ExecutionContext& ctx);

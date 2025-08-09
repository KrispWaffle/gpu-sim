#pragma once
#include <string>
#include <vector>
#include <variant>
#include <optional>

enum class Opcode { ADD, SUB, MUL, DIV, NEG, LD, ST, MOV, HALT, DEF };
enum class StoreLoc { GLOBAL, SHARED, LOCAL };
enum class ErrorCode { None, GlobalOutOfBounds, SharedOutOfBounds, InvalidMemorySpace, DivByZero };
enum class OpKind { Constant, Register, Variable, Global, Shared, Invalid };

struct Variable {
    std::string name;
    float value;
    int offset;
    bool isConstant;
    bool threadIDX;
    StoreLoc loc;
};

using Operand = std::variant<Opcode, std::string, float, Variable, StoreLoc>;

struct Instr {
    Opcode op;
    std::vector<Operand> src;
};

struct OpInfo {
    OpKind kind;
    float constVal;
    int index;
    Variable var;
};

// parsing helpers
int getRegisterName(std::string reg);
int getMemoryLocation(std::string mem);
OpInfo decodeOperand(const Operand &op, class Thread &t);

#pragma once
#include "instruction.hpp"
#include <unordered_map>

class VarTable {
public:
    VarTable(const VarTable&) = delete;
    VarTable& operator=(const VarTable&) = delete;
    static VarTable& getInstance();
    void addVar(const Variable& var, int thread_id);
    std::optional<Variable> getVar(const std::string& name, int thread_id);
private:
    VarTable() {}
    std::unordered_map<std::string, Variable> table;
};

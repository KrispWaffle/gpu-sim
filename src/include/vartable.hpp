#pragma once
#include "instruction.hpp"
#include <unordered_map>
#include <optional>

class VarTable {
public:
    std::unordered_map<std::string, Variable> table;

    void addVar(const Variable& var);
    std::optional<Variable> getVar(const std::string& name) const;
    bool has(const std::string& name) const;
    void clear();
};

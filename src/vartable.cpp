#include "vartable.hpp"
#include <string>

VarTable& VarTable::getInstance() {
    static VarTable instance;
    return instance;
}

void VarTable::addVar(const Variable& var, int thread_id) {
    table[var.name + "_" + std::to_string(thread_id)] = var;
}

std::optional<Variable> VarTable::getVar(const std::string& name, int thread_id) {
    auto it = table.find(name + "_" + std::to_string(thread_id));
    if (it != table.end()) return it->second;
    return std::nullopt;
}

#include "vartable.hpp"

void VarTable::addVar(const Variable& var) {
    table[var.name] = var;
}

std::optional<Variable> VarTable::getVar(const std::string& name) const {
    auto it = table.find(name);
    if (it != table.end()) return it->second;
    return std::nullopt;
}

bool VarTable::has(const std::string& name) const {
    return table.find(name) != table.end();
}

void VarTable::clear() {
    table.clear();
}

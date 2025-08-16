#pragma once
#include "instruction.hpp"
#include <unordered_map>
#include <algorithm> 
#include <mutex>
struct Label{
    std::string labelName;
    int pos;
    bool done;
};
class labelTable {
public:

    labelTable(const labelTable&) = delete;
    labelTable& operator=(const labelTable&) = delete;
    static labelTable& getInstance();
    void addLabel(const std::string& label, int pos);
    std::optional<int> getLabel(const std::string& name);

private:
    labelTable() {}
     mutable std::mutex mtx_;
    std::vector<Label> labels;
    
};

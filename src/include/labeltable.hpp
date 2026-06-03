#pragma once
#include "instruction.hpp"
#include <vector>
#include <string>
#include <optional>
#include <mutex>
#include <algorithm>

struct Label {
    std::string labelName;
    int pos;
};

class LabelTable {
public:
    void addLabel(const std::string& label, int pos);
    std::optional<int> getLabel(const std::string& name) const;
    void clear();

private:
    mutable std::mutex mtx_;
    std::vector<Label> labels;
};

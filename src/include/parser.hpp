#pragma once
#include "instruction.hpp"
#include "config.hpp"
#include <vector>
#include <unordered_map>
#include <string>
#include <stdexcept>

struct ParseError : std::runtime_error {
    int line;
    ParseError(int l, const std::string& msg)
        : std::runtime_error("line " + std::to_string(l) + ": " + msg), line(l) {}
};

struct Program {
    std::vector<Instr> instructions;
    std::unordered_map<std::string, int> labels;
};

Program parseProgram(const std::string& source, const SimConfig& cfg = SimConfig{});

std::vector<std::string> tokenizerView(const std::string& source);
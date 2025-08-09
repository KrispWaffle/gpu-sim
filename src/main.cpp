#include "gpu.hpp"
#include "handlers.hpp"
#include "gui.hpp"
#include <iostream>

int main() {
    setup_opcode_handlers();

    std::vector<Instr> program = {
        {Opcode::MOV, {"r0", 3.0f}},
        {Opcode::ADD, {"r1", "r0", 2.0f}},
        {Opcode::HALT, {}}
    };

    GPU gpu(program);
    gpu.run();

    GUI gui;

    while (!gui.shouldClose()) {
        gui.beginFrame();

        ImGui::Begin("GPU Simulator");
        ImGui::Text("Simulation finished.");
        ImGui::End();
       
        gui.endFrame();
    }

    return 0;
}

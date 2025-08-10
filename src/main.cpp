#include "gpu.hpp"
#include "handlers.hpp"
#include "gui.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <mutex>

class ConsoleCapture : public std::stringbuf
{
public:
    std::string log;
    std::mutex mtx;

protected:
    int sync() override
    {
        std::lock_guard<std::mutex> lock(mtx);
        log += str(); // Append to log
        str("");      // Clear buffer
        return 0;
    }
};

ConsoleCapture consoleCapture;
std::streambuf *oldCoutBuf = nullptr;

void startConsoleCapture()
{
    oldCoutBuf = std::cout.rdbuf(&consoleCapture);
}

void stopConsoleCapture()
{
    std::cout.rdbuf(oldCoutBuf);
}
int main()
{
    setup_opcode_handlers();

    std::vector<Instr> program = {
        {Opcode::MOV, {"r0", 3.0f}},
        {Opcode::ADD, {"r0", "r0", 2.0f}},
        {Opcode::HALT, {}}};

    GPU gpu(program);

    GUI gui;
    bool threadView = false;
    bool memoryView = false;
    bool logs = true;
    while (!gui.shouldClose())
    {
        gui.beginFrame();

        ImGui::Begin("GPU Simulator");
        if (ImGui::Button("run sim"))
        {
            startConsoleCapture();
            gpu.run();
        }
        if (ImGui::Button("Open Thread Viewer"))
        {
            threadView = true;
        }
        if (ImGui::Button("Open Mem Viewer"))
        {
            memoryView = true;
        }
        if (threadView)
        {
            ImGui::Begin("Thread Viewer", &threadView);

            for (auto &thread : gpu.sms[0].warps[0].threads)
            {
                ImGui::Separator();
                ImGui::Text("Thread %d", thread->id());

                if (ImGui::BeginTable(("Registers##" + std::to_string(thread->id())).c_str(), 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("Register");
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableHeadersRow();

                    for (int j = 0; j < thread->_registers.size(); j++)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("R%d", j);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%.6f", thread->_registers[j]);
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::End();
        }
        if (memoryView)
        {
            ImGui::Begin("Memory Viewer", &memoryView);

            if (ImGui::CollapsingHeader("Global Memory", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::BeginTable("GlobalMemTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
                ImGui::TableSetupColumn("Address");
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();

                for (size_t addr = 0; addr < gpu.global_memory.size(); addr++)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("0x%04zx", addr);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%f", gpu.global_memory[addr]); 
                }
                ImGui::EndTable();
            }

            if (ImGui::CollapsingHeader("Warp Memory", ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (size_t w = 0; w < gpu.sms[0].warps.size(); w++)
                {
                    ImGui::SeparatorText(("Warp " + std::to_string(w)).c_str());
                    ImGui::BeginTable(("WarpTable" + std::to_string(w)).c_str(), 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
                    ImGui::TableSetupColumn("Address");
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableHeadersRow();

                    for (size_t addr = 0; addr < gpu.sms[0].warps[w].memory.size(); addr++)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("0x%04zx", addr);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%f", gpu.sms[0].warps[w].memory[addr]);
                    }
                    ImGui::EndTable();
                }
            }

            ImGui::End();
        }
        if (logs)
        {
            ImGui::Begin("Logs", &logs);
            {
                std::lock_guard<std::mutex> lock(consoleCapture.mtx);
                ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
                ImGui::TextUnformatted(consoleCapture.log.c_str());
                ImGui::EndChild();
            }
            ImGui::End();
        }
        ImGui::Text("Current Cycle: %i", gpu.get_cycle());
        ImGui::End();

        gui.endFrame();
    }

    return 0;
}

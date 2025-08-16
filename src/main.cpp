#include "gpu.hpp"
#include "operations.hpp"
#include "gui.hpp"
#include "vartable.hpp"
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
        // Define registers

        // END x = 30
        {Opcode::DEF, {Variable{"x", 0.0f, 0, false, true, StoreLoc::SHARED}}},
        {Opcode::DEF, {Variable{"i", 0.0f, 0, false, true,StoreLoc::GLOBAL }}},
        {Opcode::DEF, {Variable{"z", 10.0f, 2 ,false , false, StoreLoc::LOCAL}}},
        {Opcode::LABEL, {"LOOP",4}},
        {Opcode::ADD, {"r0", "r0", 3.0f}},
        {Opcode::ADD, {"i", "i", 1.0f}},
        {Opcode::CMP_LT, {"i", "z"}},
        {Opcode::JNZ, {"LOOP"}},
        {Opcode::HALT, {}}};


         std::vector<Instr> test_program = {
        // Define registers

        // END x = 30
        {Opcode::DEF, {Variable{"x", 0.0f, 0, false, true, StoreLoc::SHARED}}},
        {Opcode::DEF, {Variable{"i", 0.0f, 0, false, true,StoreLoc::GLOBAL }}},
        {Opcode::DEF, {Variable{"z", 10.0f, 2 ,false , false, StoreLoc::LOCAL}}},
        {Opcode::ADD, {"i", "i", 1.0f}},
        {Opcode::HALT, {}}};

    GPU gpu(program);

    GUI gui;
    bool threadView = true;
    bool memoryView = true;
    bool logs = true;
    bool simRunning = false;
    bool vars = false;
    while (!gui.shouldClose())
    {
        gui.beginFrame();

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Gpu"))
            {
                if (ImGui::MenuItem("Run Program"))
                {
                    startConsoleCapture();
                    gpu.run();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Thread Viewer", nullptr, &threadView);
                ImGui::MenuItem("Memory Viewer", nullptr, &memoryView);
                ImGui::MenuItem("Logs", nullptr, &logs);
                ImGui::MenuItem("Vars", nullptr, &vars);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        if (vars)
        {
            ImGui::Begin("Vars", &vars);
          
                if (ImGui::BeginTable("VarTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableHeadersRow();
                    const auto &table = VarTable::getInstance().table;
                    for (const auto &pair : table)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", pair.first.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%f", pair.second.value);
                    }

                    ImGui::EndTable();
                }
            
            ImGui::End();
        }
        if (threadView)
        {
            ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(360, 450), ImGuiCond_Once);

            ImGui::Begin("Thread Viewer", &threadView);
            for (auto &thread : gpu.all_threads)
            {
                ImGui::SeparatorText(("Thread " + std::to_string(thread->id())).c_str());

                if (ImGui::BeginTable(("Registers##" + std::to_string(thread->id())).c_str(), 2,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
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
            ImGui::SetNextWindowPos(ImVec2(370, 30), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_Once);

            ImGui::Begin("Memory Viewer", &memoryView);
            if (ImGui::CollapsingHeader("Global Memory", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::BeginTable("GlobalMemTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
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
            }

            if (ImGui::CollapsingHeader("Warp Memory", ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (size_t w = 0; w < gpu.sms[0].warps.size(); w++)
                {
                    ImGui::SeparatorText(("Warp " + std::to_string(w)).c_str());
                    if (ImGui::BeginTable(("WarpTable" + std::to_string(w)).c_str(), 2,
                                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                    {
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
            }

            ImGui::End();
        }

        if (logs)
        {
            ImGui::SetNextWindowPos(ImVec2(10, 490), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(860, 200), ImGuiCond_Once);

            ImGui::Begin("Logs", &logs);
            {
                std::lock_guard<std::mutex> lock(consoleCapture.mtx);
                ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
                ImGui::TextUnformatted(consoleCapture.log.c_str());

                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    ImGui::SetScrollHereY(1.0f);

                ImGui::EndChild();
            }
            ImGui::End();
        }
        float height = 70 + (gpu.all_threads.size() * ImGui::GetTextLineHeightWithSpacing());
        ImGui::SetNextWindowPos(ImVec2(880, 30), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(250, height), ImGuiCond_Once);

        ImGui::Begin("Status");

        ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
        if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
        {
            if (ImGui::BeginTabItem("Main"))
            {
                ImGui::Text("Current Cycle: %i", gpu.get_cycle());
                ImGui::Text("PC: %lu",gpu.sms[0].shared_pc );
                if (ImGui::Button("reset"))
                {
                    gpu.reset();
                }
            
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Thread"))
            {
                for (auto &thread : gpu.all_threads)
                {
                    ImGui::Text("Thread %i status: %s", thread->id(), thread->active ? "active" : "inactive");
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();

        gui.endFrame();
    }
    gpu.stop();
    return 0;
}

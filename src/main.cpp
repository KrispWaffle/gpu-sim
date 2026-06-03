#include "gpu.hpp"
#include "operations.hpp"
#include "gui.hpp"
#include "parser.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include <filesystem>
#include <cstdio>

class ConsoleCapture : public std::stringbuf
{
public:
    std::string log;
    std::mutex mtx;

protected:
    int sync() override
    {
        std::lock_guard<std::mutex> lock(mtx);
        log += str();
        str("");
        return 0;
    }
};

ConsoleCapture consoleCapture;
std::streambuf *oldCoutBuf = nullptr;

void startConsoleCapture() { oldCoutBuf = std::cout.rdbuf(&consoleCapture); }
void stopConsoleCapture()  { std::cout.rdbuf(oldCoutBuf); }

static constexpr size_t EDITOR_CAPACITY = 32 * 1024;

static std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::stringstream ss; ss << in.rdbuf();
    return ss.str();
}

static std::vector<std::string> listGsimFiles(const std::string& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) return out;
    for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        auto p = entry.path();
        if (p.extension() == ".gsim") out.push_back(p.string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

static std::string compileInto(GPU& gpu, const std::string& source) {
    try {
        Program p = parseProgram(source);
    
        gpu.loadProgram(std::move(p.instructions), std::move(p.labels));
        return "OK";
    } catch (const ParseError& e) {
        return std::string("Parse error: ") + e.what();
    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

int main()
{
    setup_opcode_handlers();

    const std::string defaultPath = "ex/loop.gsim";
    std::string initialSrc = readFile(defaultPath);
    if (initialSrc.empty()) {
        std::cerr << "warning: could not read " << defaultPath << "\n";
        initialSrc = "; write your program here\nhalt\n";
    }
    
    GPU gpu({});
    std::string compileStatus = compileInto(gpu, initialSrc);

    std::vector<char> editorBuf(EDITOR_CAPACITY, 0);
    std::snprintf(editorBuf.data(), editorBuf.size(), "%s", initialSrc.c_str());
    std::string gsimDir = "ex";
    std::vector<std::string> programFiles = listGsimFiles(gsimDir);
    int selectedFile = 0;

    GUI gui;
    bool threadView = true;
    bool memoryView = true;
    bool logs = true;
    bool editor = true;
    bool vars = false;

    while (!gui.shouldClose())
    {
        gui.beginFrame();

        bool doRun = false;
        bool doReset = false;
        bool doStop = false;
        bool doCompile = false;
        bool doOpen = false;
        {
        std::lock_guard<std::mutex> lock(gpu.mtx);

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Gpu"))
            {
                if (ImGui::MenuItem("Run Program"))  doRun = true;
                if (ImGui::MenuItem("Reset"))        doReset = true;
                if (ImGui::MenuItem("Stop"))         doStop = true;
                if(ImGui::MenuItem("Clear")) consoleCapture.log.clear();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Editor",        nullptr, &editor);
                ImGui::MenuItem("Thread Viewer", nullptr, &threadView);
                ImGui::MenuItem("Memory Viewer", nullptr, &memoryView);
                ImGui::MenuItem("Logs",          nullptr, &logs);
                ImGui::MenuItem("Vars",          nullptr, &vars);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (editor)
        {
            ImGui::SetNextWindowPos(ImVec2(880, 300), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(520, 480), ImGuiCond_Once);
            ImGui::Begin("Editor", &editor);

            ImGui::Text("File:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(280);
            const char* preview = programFiles.empty() ? "(no files)"
                                  : programFiles[selectedFile].c_str();
            if (ImGui::BeginCombo("##filebox", preview))
            {
                for (int i = 0; i < (int)programFiles.size(); i++) {
                    bool sel = (i == selectedFile);
                    if (ImGui::Selectable(programFiles[i].c_str(), sel))
                        selectedFile = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (ImGui::Button("Open")) doOpen = true;
            ImGui::SameLine();
            if (ImGui::Button("Refresh##files")) programFiles = listGsimFiles(gsimDir);

            ImGui::Separator();
            ImGui::InputTextMultiline("##src", editorBuf.data(), editorBuf.size(),
                ImVec2(-FLT_MIN, -ImGui::GetTextLineHeightWithSpacing() * 3.5f));

            ImGui::Separator();
            if (ImGui::Button("Compile & Load")) doCompile = true;
            ImGui::SameLine();
            ImGui::TextWrapped("%s", compileStatus.c_str());

            ImGui::End();
        }

        if (vars)
        {
            ImGui::Begin("Vars", &vars);
            if (ImGui::BeginTable("VarTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();
                for (const auto& pair : gpu.vars.table)
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
            for (auto& thread : gpu.all_threads)
            {
                ImGui::SeparatorText(("Thread " + std::to_string(thread->id())).c_str());
                if (ImGui::BeginTable(("Registers##" + std::to_string(thread->id())).c_str(), 2,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("Register");
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableHeadersRow();
                    for (size_t j = 0; j < thread->_registers.size(); j++)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("R%zu", j);
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

        float height = 90 + (gpu.all_threads.size() * ImGui::GetTextLineHeightWithSpacing());
        ImGui::SetNextWindowPos(ImVec2(880, 30), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(250, height), ImGuiCond_Once);
        ImGui::Begin("Status");
        if (ImGui::BeginTabBar("MyTabBar"))
        {
            if (ImGui::BeginTabItem("Main"))
            {
                ImGui::Text("Cycle: %i", gpu.get_cycle());
                ImGui::Text("State: %s", gpu.running ? "running"
                                       : gpu.finished ? "finished" : "idle");
                ImGui::Text("Last PC: %lu", gpu.sms[0].shared_pc);
                if (ImGui::Button("reset")) doReset = true;
                ImGui::SameLine();
                if (ImGui::Button("stop"))  doStop = true;
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Thread"))
            {
                for (size_t idx = 0; idx < gpu.all_threads.size(); ++idx) {
                    auto& thread = gpu.all_threads[idx];
                    size_t warp_id = idx / WARP_SIZE;
                    size_t wi      = idx % WARP_SIZE;
                    std::string pc_str = "halted";
                    if (!gpu.sms.empty() && warp_id < gpu.sms[0].warps.size()) {
                        for (const auto& s : gpu.sms[0].warps[warp_id].splinters) {
                            if (s.mask.test(wi)) { pc_str = std::to_string(s.pc); break; }
                        }
                    }
                    ImGui::Text("T%i %s pc=%s", thread->id(),
                                thread->active ? "active" : "inactive", pc_str.c_str());
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        }  
        if (doStop)  gpu.stop();
        if (doReset) gpu.reset();
        if (doOpen && !programFiles.empty()) {
            std::string s = readFile(programFiles[selectedFile]);
            if (s.empty()) {
                compileStatus = "Could not read " + programFiles[selectedFile];
            } else {
                std::snprintf(editorBuf.data(), editorBuf.size(), "%s", s.c_str());
                compileStatus = compileInto(gpu, s);
            }
        }
        if (doCompile) {
            compileStatus = compileInto(gpu, std::string(editorBuf.data()));
        }
        if (doRun) { startConsoleCapture(); gpu.run(); }

        gui.endFrame();
    }
    gpu.stop();
    return 0;
}

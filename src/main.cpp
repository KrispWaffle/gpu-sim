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
#include <set>

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

static std::string compileInto(GPU& gpu, const std::string& source,
                               std::vector<std::string>& loadedLines) {
    try {
        Program p = parseProgram(source, gpu.cfg);

        gpu.loadProgram(std::move(p.instructions), std::move(p.labels));
        loadedLines.clear();
        std::istringstream in(source);
        std::string line;
        while (std::getline(in, line)) loadedLines.push_back(line);
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
    std::vector<std::string> loadedLines;
    std::string compileStatus = compileInto(gpu, initialSrc, loadedLines);

    std::vector<char> editorBuf(EDITOR_CAPACITY, 0);
    std::snprintf(editorBuf.data(), editorBuf.size(), "%s", initialSrc.c_str());
    std::string gsimDir = "ex";
    std::vector<std::string> programFiles = listGsimFiles(gsimDir);
    int selectedFile = 0;

    GUI gui;
    SimConfig pendingCfg = gpu.cfg;
    bool threadView = true;
    bool memoryView = true;
    bool logs = true;
    bool editor = true;
    bool vars = false;
    bool timeline = true;
    bool programView = true;

    while (!gui.shouldClose())
    {
        gui.beginFrame();

        bool doRun = false;
        bool doReset = false;
        bool doStop = false;
        bool doCompile = false;
        bool doOpen = false;
        bool doConfigure = false;
        bool doStep = false;
        {
        std::lock_guard<std::mutex> lock(gpu.mtx);

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Gpu"))
            {
                if (ImGui::MenuItem("Run Program"))  doRun = true;
                if (ImGui::MenuItem("Step"))         doStep = true;
                if (ImGui::MenuItem(gpu.paused ? "Resume" : "Pause"))
                    gpu.paused = !gpu.paused;
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
                ImGui::MenuItem("Timeline",      nullptr, &timeline);
                ImGui::MenuItem("Program",       nullptr, &programView);
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
            for (auto& sm : gpu.sms)
            {
                for (auto& warp : sm.warps)
                {
                    std::string header = "SM " + std::to_string(sm.id) +
                                         " / Warp " + std::to_string(warp.id_);
                    if (!ImGui::CollapsingHeader(header.c_str()))
                        continue;
                    for (auto& thread : warp.threads)
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
                for (auto& sm : gpu.sms)
                {
                    for (auto& warp : sm.warps)
                    {
                        std::string label = "SM " + std::to_string(sm.id) +
                                            " / Warp " + std::to_string(warp.id_);
                        ImGui::SeparatorText(label.c_str());
                        if (ImGui::BeginTable(("WarpTable" + std::to_string(warp.id_)).c_str(), 2,
                                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                        {
                            ImGui::TableSetupColumn("Address");
                            ImGui::TableSetupColumn("Value");
                            ImGui::TableHeadersRow();
                            for (size_t addr = 0; addr < warp.memory.size(); addr++)
                            {
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("0x%04zx", addr);
                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text("%f", warp.memory[addr]);
                            }
                            ImGui::EndTable();
                        }
                    }
                }
            }
            ImGui::End();
        }

        if (timeline)
        {
            ImGui::SetNextWindowPos(ImVec2(10, 700), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(1390, 320), ImGuiCond_Once);
            ImGui::Begin("Timeline", &timeline);

            static const ImU32 palette[] = {
                IM_COL32(86, 180, 233, 255), IM_COL32(230, 159, 0, 255),
                IM_COL32(0, 158, 115, 255),  IM_COL32(204, 121, 167, 255),
                IM_COL32(240, 228, 66, 255), IM_COL32(213, 94, 0, 255),
                IM_COL32(0, 114, 178, 255),  IM_COL32(155, 89, 182, 255),
                IM_COL32(46, 204, 113, 255), IM_COL32(231, 76, 60, 255),
                IM_COL32(26, 188, 156, 255), IM_COL32(241, 148, 138, 255),
            };
            constexpr int paletteN = (int)(sizeof(palette) / sizeof(palette[0]));
            const ImU32 idleCol  = IM_COL32(60, 60, 60, 255);
            const ImU32 stallCol = IM_COL32(35, 60, 150, 255);
            const ImU32 barCol   = IM_COL32(210, 180, 40, 255);

            ImGui::TextDisabled("rows: threads (grouped by warp) | columns: cycles | "
                                "color: PC | gray: halted | blue: mem stall | yellow: barrier");
            if (gpu.history.size() >= HISTORY_CAP)
                ImGui::TextDisabled("history capped at %zu cycles", HISTORY_CAP);

            const float cellW = 6.0f, cellH = 10.0f, warpGap = 6.0f;
            const float labelW = 34.0f;
            int warpSize = gpu.cfg.warpSize;
            int numWarps = (gpu.cfg.numThreads + warpSize - 1) / warpSize;
            float totalH = numWarps * (warpSize * cellH + warpGap);
            float totalW = labelW + (float)gpu.history.size() * cellW;

            ImGui::BeginChild("timeline_scroll", ImVec2(0, 0), true,
                              ImGuiWindowFlags_HorizontalScrollbar);
            ImVec2 origin = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            float scrollX = ImGui::GetScrollX();
            float viewW = ImGui::GetWindowSize().x;

            size_t c0 = (size_t)(scrollX > labelW ? (scrollX - labelW) / cellW : 0.0f);
            size_t c1 = std::min(gpu.history.size(),
                                 (size_t)((scrollX + viewW) / cellW) + 1);
            for (size_t c = c0; c < c1; ++c)
            {
                const auto& rec = gpu.history[c];
                float x0 = origin.x + labelW + (float)c * cellW;
                for (size_t w = 0; w < rec.size(); ++w)
                {
                    int lanes = std::min(warpSize, gpu.cfg.numThreads - (int)w * warpSize);
                    float wy = origin.y + (float)w * (warpSize * cellH + warpGap);
                    for (int lane = 0; lane < lanes; ++lane)
                    {
                        ImU32 col = idleCol;
                        for (const auto& s : rec[w].splinters)
                        {
                            if (!s.mask.test(lane)) continue;
                            col = rec[w].stalled   ? stallCol
                                : rec[w].atBarrier ? barCol
                                : palette[s.pc % paletteN];
                            break;
                        }
                        float y0 = wy + lane * cellH;
                        dl->AddRectFilled(ImVec2(x0, y0),
                                          ImVec2(x0 + cellW - 1.0f, y0 + cellH - 1.0f), col);
                    }
                }
            }
            for (int w = 0; w < numWarps; ++w)
            {
                float wy = origin.y + (float)w * (warpSize * cellH + warpGap);
                dl->AddText(ImVec2(ImGui::GetWindowPos().x + 4.0f, wy),
                            IM_COL32(200, 200, 200, 255), ("W" + std::to_string(w)).c_str());
            }
            ImGui::Dummy(ImVec2(std::max(totalW, 1.0f), std::max(totalH, 1.0f)));
            if (gpu.running && !gpu.paused)
                ImGui::SetScrollX(ImGui::GetScrollMaxX());
            ImGui::EndChild();
            ImGui::End();
        }

        if (programView)
        {
            ImGui::SetNextWindowPos(ImVec2(1410, 30), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(420, 480), ImGuiCond_Once);
            ImGui::Begin("Program", &programView);
            std::set<int> activeLines;
            for (auto& sm : gpu.sms)
                for (auto& warp : sm.warps)
                    for (auto& s : warp.splinters)
                        if (s.pc < gpu.program.size())
                            activeLines.insert(gpu.program[s.pc].ln);
            for (size_t i = 0; i < loadedLines.size(); i++)
            {
                int ln = (int)i + 1;
                char buf[512];
                std::snprintf(buf, sizeof buf, "%3d| %s", ln, loadedLines[i].c_str());
                if (activeLines.count(ln))
                {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.9f, 0.7f, 0.1f, 0.45f));
                    ImGui::Selectable(buf, true);
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::TextUnformatted(buf);
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
                ImGui::Text("State: %s", !gpu.running ? (gpu.finished ? "finished" : "idle")
                                       : gpu.paused   ? "paused" : "running");
                ImGui::Text("Instructions: %lld", gpu.stats.instructionsIssued);
                double eff = gpu.stats.issueSlots > 0
                    ? 100.0 * (double)gpu.stats.instructionsIssued / (double)gpu.stats.issueSlots
                    : 0.0;
                ImGui::Text("SIMD efficiency: %.1f%%", eff);
                ImGui::Text("Divergences: %lld", gpu.stats.divergenceEvents);
                ImGui::Text("Stall cycles: %lld", gpu.stats.stallCycles);

                if (ImGui::Button("run"))   doRun = true;
                ImGui::SameLine();
                if (ImGui::Button("step"))  doStep = true;
                ImGui::SameLine();
                if (ImGui::Button(gpu.paused ? "resume" : "pause"))
                    gpu.paused = !gpu.paused;
                if (ImGui::Button("reset")) doReset = true;
                ImGui::SameLine();
                if (ImGui::Button("stop"))  doStop = true;

                int d = gpu.delayMs.load();
                ImGui::SetNextItemWidth(150);
                if (ImGui::SliderInt("delay ms", &d, 0, 500))
                    gpu.delayMs = d;
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Thread"))
            {
                for (auto& thread : gpu.all_threads) {
                    auto [smIdx, warpIdx, lane] = gpu.locateThread(thread->id());
                    std::string pc_str = "halted";
                    if (smIdx < (int)gpu.sms.size() &&
                        warpIdx < (int)gpu.sms[smIdx].warps.size()) {
                        for (const auto& s : gpu.sms[smIdx].warps[warpIdx].splinters) {
                            if (s.mask.test(lane)) { pc_str = std::to_string(s.pc); break; }
                        }
                    }
                    ImGui::Text("T%i %s pc=%s", thread->id(),
                                thread->active ? "active" : "inactive", pc_str.c_str());
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Config"))
            {
                ImGui::InputInt("Threads",       &pendingCfg.numThreads);
                ImGui::InputInt("Warp size",     &pendingCfg.warpSize);
                ImGui::InputInt("SMs",           &pendingCfg.numSMs);
                ImGui::InputInt("Registers",     &pendingCfg.numRegisters);
                ImGui::InputInt("Global mem",    &pendingCfg.globalMemSize);
                ImGui::InputInt("Gmem latency",  &pendingCfg.globalLatency);
                if (ImGui::Button("Apply")) doConfigure = true;
                ImGui::SameLine();
                ImGui::TextDisabled("(rebuilds GPU, recompiles)");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        }  
        if (doStop)  gpu.stop();
        if (doReset) gpu.reset();
        if (doConfigure) {
            gpu.configure(pendingCfg);
            pendingCfg = gpu.cfg;
            compileStatus = compileInto(gpu, std::string(editorBuf.data()), loadedLines);
        }
        if (doOpen && !programFiles.empty()) {
            std::string s = readFile(programFiles[selectedFile]);
            if (s.empty()) {
                compileStatus = "Could not read " + programFiles[selectedFile];
            } else {
                std::snprintf(editorBuf.data(), editorBuf.size(), "%s", s.c_str());
                compileStatus = compileInto(gpu, s, loadedLines);
            }
        }
        if (doCompile) {
            compileStatus = compileInto(gpu, std::string(editorBuf.data()), loadedLines);
        }
        if (doRun) { startConsoleCapture(); gpu.run(); }
        if (doStep) {
            if (gpu.running) {
                gpu.paused = true;
                gpu.pendingSteps++;
            } else {
                startConsoleCapture();
                gpu.run(/*startPaused=*/true);
                gpu.pendingSteps = 1;
            }
        }

        gui.endFrame();
    }
    gpu.stop();
    return 0;
}

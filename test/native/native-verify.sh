#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p .review-gsim
sources='src/gsim_capi.cpp src/gpu.cpp src/operations.cpp src/labeltable.cpp src/instruction.cpp src/vartable.cpp src/execution.cpp src/parser.cpp'
g++ -std=c++20 -Isrc/include -fPIC -shared -pthread $sources -o .review-gsim/libgsim-native.so
g++ -std=c++20 -Isrc/include -pthread test/native/native-tests.cpp -L.review-gsim -lgsim-native -o .review-gsim/native-tests
LD_LIBRARY_PATH=.review-gsim .review-gsim/native-tests
# Remove only the completion-normalization fix in a scratch copy.
python3 - <<'PY'
from pathlib import Path
source = Path('src/gpu.cpp').read_text()
block = '''                for (auto& sm : sms)
                    for (auto& warp : sm.warps)
                        std::erase_if(warp.splinters, [this](const Splinter& s) {
                            return s.pc >= program.size();
                        });
'''
assert source.count(block) == 2
Path('.review-gsim/native-negative-gpu.cpp').write_text(source.replace(block, ''))
PY
g++ -std=c++20 -Isrc/include -pthread test/native/native-tests.cpp src/gsim_capi.cpp .review-gsim/native-negative-gpu.cpp src/operations.cpp src/labeltable.cpp src/instruction.cpp src/vartable.cpp src/execution.cpp src/parser.cpp -o .review-gsim/native-negative
if .review-gsim/native-negative > .review-gsim/native-negative.log 2>&1; then
    echo 'FAIL: negative control unexpectedly passed'
    exit 1
fi
grep -Fx 'FAIL: fallthrough exact budget' .review-gsim/native-negative.log
echo 'PASS: cycle-boundary negative control'
# Remove only result-boundary logging, never mutate the working source.
python3 - <<'PY'
from pathlib import Path
source = Path('src/gpu.cpp').read_text()
block = '''    if (options.logging && (result.status == SimulationStatus::ExecutionError ||
                            result.status == SimulationStatus::InternalError))
        std::cout << "ERROR: " << result.diagnostic << std::endl;
'''
assert source.count(block) == 1
Path('.review-gsim/native-negative-logging-gpu.cpp').write_text(source.replace(block, ''))
PY
g++ -std=c++20 -Isrc/include -pthread test/native/native-tests.cpp src/gsim_capi.cpp .review-gsim/native-negative-logging-gpu.cpp src/operations.cpp src/labeltable.cpp src/instruction.cpp src/vartable.cpp src/execution.cpp src/parser.cpp -o .review-gsim/native-negative-logging
if .review-gsim/native-negative-logging > .review-gsim/native-negative-logging.log 2>&1; then
    echo 'FAIL: logging negative control unexpectedly passed'
    exit 1
fi
grep -Fx 'FAIL: execution diagnostic visible' .review-gsim/native-negative-logging.log
echo 'PASS: logging negative control'
make -j2 BUILD_DIR=.review-gsim/native-build .review-gsim/native-build/src/main.o .review-gsim/native-build/src/gpu.o .review-gsim/native-build/src/gui.o .review-gsim/native-build/src/operations.o .review-gsim/native-build/src/labeltable.o .review-gsim/native-build/src/instruction.o .review-gsim/native-build/src/vartable.o .review-gsim/native-build/src/execution.o .review-gsim/native-build/src/parser.o .review-gsim/native-build/imgui/imgui.o .review-gsim/native-build/imgui/imgui_draw.o .review-gsim/native-build/imgui/imgui_tables.o .review-gsim/native-build/imgui/imgui_widgets.o .review-gsim/native-build/imgui/backends/imgui_impl_glfw.o .review-gsim/native-build/imgui/backends/imgui_impl_opengl3.o
g++ .review-gsim/native-build/src/*.o .review-gsim/native-build/imgui/*.o .review-gsim/native-build/imgui/backends/*.o -lGL -lglfw -pthread -o .review-gsim/native-main
printf '#include "gsim.h"\nint main(void) { return GSIM_STATUS_SUCCESS; }\n' > .review-gsim/native-header.c
cc -Isrc/include -fsyntax-only .review-gsim/native-header.c
echo 'PASS: GUI compile/link and C header'

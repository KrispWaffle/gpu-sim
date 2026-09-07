CXX = g++
CXXFLAGS = -std=c++20 -Iimgui -Iimgui/backends -Isrc/include -I/usr/include/GLFW -g -MMD -MP -pthread
LIBS = -lGL -lglfw -pthread

BUILD_DIR = build

SRC = src/main.cpp \
      src/gpu.cpp \
      src/gui.cpp \
      src/operations.cpp \
      src/labeltable.cpp \
      src/instruction.cpp \
      src/vartable.cpp \
      src/execution.cpp \
      src/parser.cpp \
      imgui/imgui.cpp \
      imgui/imgui_draw.cpp \
      imgui/imgui_tables.cpp \
      imgui/imgui_widgets.cpp \
      imgui/backends/imgui_impl_glfw.cpp \
      imgui/backends/imgui_impl_opengl3.cpp

OBJ = $(SRC:%.cpp=$(BUILD_DIR)/%.o)
DEP = $(OBJ:.o=.d)

all: main

main: $(OBJ)
	$(CXX) $^ $(LIBS) -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEP)

# Headless shared library for the tinygrad GSIM backend (no GUI/ImGui).
LIB_SRC = src/gsim_capi.cpp \
          src/gpu.cpp \
          src/operations.cpp \
          src/labeltable.cpp \
          src/instruction.cpp \
          src/vartable.cpp \
          src/execution.cpp \
          src/parser.cpp
LIB_OBJ = $(LIB_SRC:%.cpp=$(BUILD_DIR)/pic/%.o)
LIB_DEP = $(LIB_OBJ:.o=.d)
LIB_CXXFLAGS = -std=c++20 -Isrc/include -fPIC -g -MMD -MP -pthread
LIB_LDFLAGS = -shared -pthread

libgsim.so: $(LIB_OBJ)
	$(CXX) $(LIB_LDFLAGS) $^ -o $@

$(BUILD_DIR)/pic/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(LIB_CXXFLAGS) -c $< -o $@

-include $(LIB_DEP)

clean:
	rm -rf main libgsim.so $(BUILD_DIR)

.PHONY: all clean

CXX = g++
CXXFLAGS = -std=c++20 -Iimgui -Iimgui/backends -Isrc/include -I/usr/include/GLFW -g -MMD -MP
LIBS = -lGL -lglfw

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

clean:
	rm -rf main $(BUILD_DIR)

.PHONY: all clean

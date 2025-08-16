CXX = g++
CXXFLAGS = -Iimgui -Iimgui/backends -Isrc/include -I/usr/include -I/usr/include/GLFW -g
LIBS = -lGL -lGLU -lglfw

SRC = src/main.cpp \
      src/gpu.cpp \
      src/gui.cpp \
      src/operations.cpp \
      src/labeltable.cpp \
      src/instruction.cpp \
      src/vartable.cpp \
      src/execution.cpp \
      imgui/imgui.cpp \
      imgui/imgui_draw.cpp \
      imgui/imgui_tables.cpp \
      imgui/imgui_widgets.cpp \
      imgui/backends/imgui_impl_glfw.cpp \
      imgui/backends/imgui_impl_opengl3.cpp

all:
	$(CXX) $(SRC) $(CXXFLAGS) $(LIBS) -o main

clean:
	rm -f main *.o

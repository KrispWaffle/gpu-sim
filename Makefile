CXX = g++
CXXFLAGS = -Iimgui -Iimgui/backends -I/usr/include -I/usr/include/GLFW -g
LIBS = -lGL -lGLU -lglfw

SRC = src/main.cpp \
      imgui/imgui.cpp \
      imgui/imgui_draw.cpp \
      imgui/imgui_tables.cpp \
      imgui/imgui_widgets.cpp \
      imgui/backends/imgui_impl_glfw.cpp \
      imgui/backends/imgui_impl_opengl3.cpp

all:
	$(CXX) $(SRC) $(CXXFLAGS) $(LIBS) -o main
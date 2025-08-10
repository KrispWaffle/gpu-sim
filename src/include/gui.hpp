#pragma once
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
class GUI {
public:
    GUI();
    ~GUI();
    bool shouldClose() const;
    void beginFrame();
    void endFrame();
private:
    GLFWwindow* window;
};

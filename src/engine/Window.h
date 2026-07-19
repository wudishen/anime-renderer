#pragma once
#include <GLFW/glfw3.h>
#include <string>

class Window {
public:
    Window(const std::string& title, int width, int height);
    ~Window();

    bool ShouldClose();
    void PollEvents();
    void SwapBuffers();

    GLFWwindow* getGLFWwindow() { return window; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    float GetAspect() const;

private:
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* window = nullptr;
    int m_Width = 0;
    int m_Height = 0;
};
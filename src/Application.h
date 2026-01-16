#pragma once

#include <GLFW/glfw3.h>

class Application
{
public:
    Application();

    bool Initialize();
    void RunLoop();
    void Shutdown();

private:
    void ProcessInput();
    void UpdateGame();
    void GenerateOutput();

private:
    GLFWwindow* mWindow;
};

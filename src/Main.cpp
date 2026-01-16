#include <GLFW/glfw3.h>
#include <iostream>

int main(int argc, char* argv[])
{
    // initialize glfw
    if (glfwInit() == GLFW_FALSE)
    {
        return EXIT_FAILURE;
    }

    // create window
    GLFWwindow* window = glfwCreateWindow(640, 480, "WebGPU Ocean", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwSwapInterval(1);

    // game loop
    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // terminate glfw
    glfwTerminate();

    return 0;
}

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class GLFWObject
{
public:
    GLFWwindow* window;

    uint32_t WIDTH, HEIGHT;

    GLFWObject(uint32_t width, uint32_t height);

    void init();
};
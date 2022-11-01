#include "app/GLFWObject.h"
#include "app/VulkanObject.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

GLFWObject::GLFWObject(uint32_t width, uint32_t height) : HEIGHT(height), WIDTH(width) {}

void GLFWObject::init()
{
    // initialise GLFW library
    glfwInit();

    // we are not using OpenGL, so tell GLFW not to either!
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // resizing windows is a pain, ban it.
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // create the window! (With width WIDTH, height HEIGHT, header Vulkan, and nullptr for window (we are not full screen) and shared resources (we aren't sharing)
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    //glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, VulkanObject::framebufferResizeCallback);
    //glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, VulkanObject::mouseMoveCallback);
    //glfwSetWindowUserPointer(window, this);
    glfwSetScrollCallback(window, VulkanObject::scrollMoveCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}
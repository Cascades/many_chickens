#include "app/VulkanObject.h"
#include "app/GLFWObject.h"

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_vulkan.h>

int main() {
    // function used to create a window with GLFW
    GLFWObject glfw_object(1920, 1080);
    glfw_object.init();

    std::unique_ptr<VulkanObject> vulkan_object = std::make_unique<VulkanObject>();

    // create vulkan instance
    vulkan_object->initVulkan(glfw_object.window);

    try {
        // while the window is not closed by the user
        while (!glfwWindowShouldClose(glfw_object.window)) {
            // poll for user inputs
            glfwPollEvents();
            // draws our frame
            vulkan_object->drawFrame();
        }

        // wait for device to finish before exiting. Stop post-exit layer erros
        vkDeviceWaitIdle(vulkan_object->device);

        vulkan_object->cleanup();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
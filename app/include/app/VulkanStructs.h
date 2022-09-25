#pragma once

#include <optional>

// struct containing information on GPU queues
struct QueueFamilyIndices {
    // information on graphics queue
    // this is used to check that there is a queue we can call graphics commands on
    std::optional<uint32_t> graphicsFamily;
    // information on presentation queue
    // this is used to check that we can draw to our surface
    std::optional<uint32_t> presentFamily;

    // query whether all families have a value
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// struct to hold info on our swap chain
struct SwapChainSupportDetails {
    // things like min/max number of images in chain, width and height limitations, etc.
    VkSurfaceCapabilitiesKHR capabilities;
    // pixel format and colour space supported
    std::vector<VkSurfaceFormatKHR> formats;
    // available presentation modes
    std::vector<VkPresentModeKHR> presentModes;
};
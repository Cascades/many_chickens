// Vulkan0.cpp : Defines the entry point for the application.
//

//includes various vulkan, glm and glfw headers
#include "app/VulkanObject.h"
#include "app/Vertex.h"
#include "app/HelperFunctions.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>

//includes C++ headers
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <set>
#include <unordered_map>
#include <random>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "app/Model.h"
#include "app/DescriptorInfo.h"
#include "app/Shader.h"
#include "app/ShaderProgram.h"

// vector of validation layers to be used.

// by default VK_LAYER_KHRONOS_validation should be included, containing a 
// bundle of common validation layers

// validation layers are used enable optional components which can assist 
// with debugging. The alternative is simply crashing out of potentially trivial bugs.
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// vector of required device extensions.

// the default VK_KHR_SWAPCHAIN_EXTENSION_NAME
// checks that a swapchain is supported by the device.

// a swap chain is set of framebuffers that can be swapped for added stability.
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// below is a pre-processor directive which when a debug build is run, enables validation
// (and when in any other build type, does not)
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// create debug messenger within VkInstance instance
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    // function not automatically loaded, so we look up address
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    // if we found the address
    if (func != nullptr) {
        // call the function
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        // return an error to be dealt with by caller
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

// destroy debug messenger
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {

    // locate address of vkDestroyDebugUtilsMessengerEXT in extension
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    // if we find it
    if (func != nullptr) {
        // call it!
        func(instance, debugMessenger, pAllocator);
    }
}

void VulkanObject::createCommandPool(VkCommandPool* commandPool, VkCommandPoolCreateFlags flags) {
    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.queueFamilyIndex = findQueueFamilies(physicalDevice).graphicsFamily.value();
    commandPoolCreateInfo.flags = flags;

    if (vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Could not create graphics command pool");
    }
}

void VulkanObject::createCommandBuffers(VkCommandBuffer* commandBuffer, uint32_t commandBufferCount, VkCommandPool& commandPool) {
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.commandBufferCount = commandBufferCount;
    vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffer);
}

void VulkanObject::initVulkan(GLFWwindow* window, std::shared_ptr<mc::Camera> camera) {
    this->window = window;
    this->camera = camera;

    imgui_clear_value = { 0.6f, 0.4f, 0.0f, 1.0f };

    MODEL_PATH = "../assets/chicken/chicken.obj";
    TEXTURE_PATH = "../assets/chicken/chicken.png";

    // function to create an instance of the vulkan library
    createInstance();
    // setup our debugger to control output
    setupDebugMessenger();
    // create our surface
    createSurface();
    // pick a physical device to use
    pickPhysicalDevice();
    // create a logical device to use based off physical device
    createLogicalDevice();
    // create a swap chain
    createSwapChain();
    // create our image views
    createImageViews();
    // create render pass object using previous information
    createRenderPass();
    createComputePipeline();
    // create graphics pipeline
    createGraphicsPipeline();
    // create our command pool
    createCommandPool();
    createDepthResources();
    // function to create framebuffers and populate swapChainFramebuffers vector
    createFramebuffers();

    imgui_frame_buffers.resize(swapChainImages.size());
	
    {
        VkImageView attachment[1];
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = imgui_render_pass;
        info.attachmentCount = 1;
        info.pAttachments = attachment;
        info.width = swapChainExtent.width;
        info.height = swapChainExtent.height;
        info.layers = 1;
        for (uint32_t i = 0; i < swapChainImages.size(); i++)
        {
            attachment[0] = swapChainImageViews[i];
            vkCreateFramebuffer(device, &info, VK_NULL_HANDLE, &imgui_frame_buffers[i]);
        }
    }

    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    loadModel();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createSSBOs();
    updateSSBO();
    createDescriptorPool();
    createDescriptorSets();

    VkDescriptorPoolSize imgui_pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo imgui_pool_info = {};
    imgui_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    imgui_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    imgui_pool_info.maxSets = 1000 * IM_ARRAYSIZE(imgui_pool_sizes);
    imgui_pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(imgui_pool_sizes);
    imgui_pool_info.pPoolSizes = imgui_pool_sizes;
    vkCreateDescriptorPool(device, &imgui_pool_info, VK_NULL_HANDLE, &imgui_descriptor_pool);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physicalDevice;
    init_info.Device = device;
    init_info.QueueFamily = findQueueFamilies(physicalDevice).graphicsFamily.value();
    init_info.Queue = graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imgui_descriptor_pool;
    init_info.Allocator = VK_NULL_HANDLE;
    init_info.MinImageCount = static_cast<uint32_t>(swapChainImages.size());
    init_info.ImageCount = static_cast<uint32_t>(swapChainImages.size());
    init_info.CheckVkResultFn = VK_NULL_HANDLE;
    ImGui_ImplVulkan_Init(&init_info, imgui_render_pass);

    VkCommandBuffer command_buffer = beginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    endSingleTimeCommands(command_buffer);

    createCommandPool(&imgui_command_pool, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    imgui_command_buffers.resize(swapChainImageViews.size());
    createCommandBuffers(imgui_command_buffers.data(), static_cast<uint32_t>(imgui_command_buffers.size()), imgui_command_pool);

    // create command buffers
    createCommandBuffers();
    // create and set up semaphores and fences
    createSyncObjects();
}

VkFormat VulkanObject::findDepthFormat() {
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool VulkanObject::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkFormat VulkanObject::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

void VulkanObject::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();
    createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);


}

void VulkanObject::createTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }

    VkSamplerCreateInfo createDepthInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    createDepthInfo.magFilter = VK_FILTER_LINEAR;
    createDepthInfo.minFilter = VK_FILTER_LINEAR;
    createDepthInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    createDepthInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createDepthInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createDepthInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createDepthInfo.minLod = 0;
    createDepthInfo.maxLod = 16.f;

    VkSamplerReductionModeCreateInfoEXT createInfoReduction = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };

    createInfoReduction.reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX;

    createDepthInfo.pNext = &createInfoReduction;

    if (vkCreateSampler(device, &createDepthInfo, 0, &depthSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create depth sampler!");
    }

    VkSamplerCreateInfo createNearestDepthInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    createNearestDepthInfo.magFilter = VK_FILTER_NEAREST;
    createNearestDepthInfo.minFilter = VK_FILTER_NEAREST;
    createNearestDepthInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    createNearestDepthInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createNearestDepthInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createNearestDepthInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createNearestDepthInfo.minLod = 0;
    createNearestDepthInfo.maxLod = 16.f;

    if (vkCreateSampler(device, &createNearestDepthInfo, 0, &depthNearestSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create depth sampler!");
    }

    VkSamplerCreateInfo createNearestMinDepthInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    createNearestMinDepthInfo.magFilter = VK_FILTER_NEAREST;
    createNearestMinDepthInfo.minFilter = VK_FILTER_NEAREST;
    createNearestMinDepthInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    createNearestMinDepthInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createNearestMinDepthInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createNearestMinDepthInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createNearestMinDepthInfo.minLod = 0;
    createNearestMinDepthInfo.maxLod = 16.f;

    createInfoReduction = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };

    createInfoReduction.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN;

    createNearestMinDepthInfo.pNext = &createInfoReduction;

    if (vkCreateSampler(device, &createNearestMinDepthInfo, 0, &depthNearestMinSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create depth sampler!");
    }
}

VkImageView VulkanObject::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t baseMipLevel, uint32_t levelCount) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
    viewInfo.subresourceRange.levelCount = levelCount;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.aspectMask = aspectFlags;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

void VulkanObject::loadModel()
{
    dragon_model.loadModel("../assets/chicken/chicken.obj");
}

void VulkanObject::createTextureImageView() {
    textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void VulkanObject::createTextureImage() {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    stbi_image_free(pixels);

    createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanObject::createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory,
        uint32_t mipLevels)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

void VulkanObject::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 1> computePoolSizes{};
    computePoolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computePoolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainImages.size() * 20);

    VkDescriptorPoolCreateInfo computePoolInfo{};
    computePoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    computePoolInfo.poolSizeCount = static_cast<uint32_t>(computePoolSizes.size());
    computePoolInfo.pPoolSizes = computePoolSizes.data();
    computePoolInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());

    if (vkCreateDescriptorPool(device, &computePoolInfo, nullptr, &computeDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    std::array<VkDescriptorPoolSize, 2> depthPyramidComputePoolSizes{};
    depthPyramidComputePoolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    depthPyramidComputePoolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainImages.size() * 50);
    depthPyramidComputePoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    depthPyramidComputePoolSizes[1].descriptorCount = static_cast<uint32_t>(swapChainImages.size() * 50);

    VkDescriptorPoolCreateInfo depthPyramidComputePoolInfo{};
    depthPyramidComputePoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    depthPyramidComputePoolInfo.poolSizeCount = static_cast<uint32_t>(depthPyramidComputePoolSizes.size());
    depthPyramidComputePoolInfo.pPoolSizes = depthPyramidComputePoolSizes.data();
    depthPyramidComputePoolInfo.maxSets = static_cast<uint32_t>(swapChainImages.size() * 50);

    if (vkCreateDescriptorPool(device, &depthPyramidComputePoolInfo, nullptr, &depthPyramidComputeDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainImages.size());
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(swapChainImages.size());
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = static_cast<uint32_t>(swapChainImages.size());

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    std::array<VkDescriptorPoolSize, 6> lightingPoolSizes{};
    lightingPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightingPoolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainImages.size() * 20);
    lightingPoolSizes[1].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    lightingPoolSizes[1].descriptorCount = static_cast<uint32_t>(swapChainImages.size());
    lightingPoolSizes[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    lightingPoolSizes[2].descriptorCount = static_cast<uint32_t>(swapChainImages.size());
    lightingPoolSizes[3].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    lightingPoolSizes[3].descriptorCount = static_cast<uint32_t>(swapChainImages.size());
    lightingPoolSizes[4].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    lightingPoolSizes[4].descriptorCount = static_cast<uint32_t>(swapChainImages.size());
    lightingPoolSizes[5].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    lightingPoolSizes[5].descriptorCount = static_cast<uint32_t>(swapChainImages.size());

    VkDescriptorPoolCreateInfo lightingPoolInfo{};
    lightingPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    lightingPoolInfo.poolSizeCount = static_cast<uint32_t>(lightingPoolSizes.size());
    lightingPoolInfo.pPoolSizes = lightingPoolSizes.data();
    lightingPoolInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());

    if (vkCreateDescriptorPool(device, &lightingPoolInfo, nullptr, &lightingDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    std::array<VkDescriptorPoolSize, 1> shadowPoolSizes{};
    shadowPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    shadowPoolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainImages.size());

    VkDescriptorPoolCreateInfo shadowPoolInfo{};
    shadowPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    shadowPoolInfo.poolSizeCount = static_cast<uint32_t>(shadowPoolSizes.size());
    shadowPoolInfo.pPoolSizes = shadowPoolSizes.data();
    shadowPoolInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());

    if (vkCreateDescriptorPool(device, &shadowPoolInfo, nullptr, &shadowDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void VulkanObject::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(swapChainImages.size());
    uniformBuffersMemory.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
    }

    VkDeviceSize shadowBufferSize = sizeof(ShadowUniformBufferObject);

    shadowUniformBuffers.resize(swapChainImages.size());
    shadowUniformBuffersMemory.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        createBuffer(shadowBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, shadowUniformBuffers[i], shadowUniformBuffersMemory[i]);
    }
}

void VulkanObject::createSSBOs() {
    VkDeviceSize bufferSize = sizeof(ModelTransforms);

    createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        SSBO,
        SSBOMemory);

    bufferSize = modelScales->size() * sizeof(float);

    createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        scaleSSBO,
        scaleSSBOMemory);

    bufferSize = modelTransforms->modelMatricies.size() * 32;

    indirectLodSSBO.resize(swapChainImages.size());
    indirectLodSSBOMemory.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            indirectLodSSBO[i],
            indirectLodSSBOMemory[i]);
    }

    bufferSize = dragon_model.getTotalLodLevels() * sizeof(LodConfigData);

    lodConfigSSBO.resize(swapChainImages.size());
    lodConfigSSBOMemory.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            lodConfigSSBO[i],
            lodConfigSSBOMemory[i]);
    }

    bufferSize = modelTransforms->modelMatricies.size() * sizeof(VkBool32);

    drawnLastFrameSSBO.resize(swapChainImages.size());
    drawnLastFrameSSBOMemory.resize(swapChainImages.size());

    //for (size_t i = 0; i < swapChainImages.size(); i++) {
        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            drawnLastFrameSSBO[0],
            drawnLastFrameSSBOMemory[0]);
    //}

    struct sphereProjectionDebugData
    {
        alignas(16) glm::vec4 aabb;
        //alignas(16) glm::vec4 depthData;
        //glm::vec2 depthData;
        //glm::vec2 depthLookUpCoord;
        //uint32_t lodLevel;
    };

    bufferSize = modelTransforms->modelMatricies.size() * sizeof(sphereProjectionDebugData);

    sphereProjectionDebugSSBO.resize(swapChainImages.size());
    sphereProjectionDebugSSBOMemory.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            sphereProjectionDebugSSBO[i],
            sphereProjectionDebugSSBOMemory[i]);
    }
}

void VulkanObject::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(dragon_model.getIndices()[0]) * dragon_model.getIndices().size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, dragon_model.getIndices().data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanObject::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void VulkanObject::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(dragon_model.getVertices()[0]) * dragon_model.getVertices().size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, dragon_model.getVertices().data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanObject::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;

    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (hasStencilComponent(format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(commandBuffer);
}

void VulkanObject::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
        width,
        height,
        1
    };

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer VulkanObject::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanObject::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void VulkanObject::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

uint32_t VulkanObject::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

// clean up swap chain for a clean recreate
void VulkanObject::cleanupSwapChain() {
    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);

    // destroy all framebuffers in swap chain
    for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
        vkDestroyFramebuffer(device, imgui_frame_buffers[i], nullptr);
        vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
    }

    vkFreeCommandBuffers(device, imgui_command_pool, static_cast<uint32_t>(imgui_command_buffers.size()), imgui_command_buffers.data());
    vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

    //destroy pipeline
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    // destroy render pass resources
    vkDestroyRenderPass(device, imgui_render_pass, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);

    // Destroy each image view we own
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        vkDestroyImageView(device, swapChainImageViews[i], nullptr);
    }

    // destroy our swapchain
    vkDestroySwapchainKHR(device, swapChain, nullptr);

    for (size_t i = 0; i < uniformBuffers.size(); i++) {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
        vkDestroyBuffer(device, indirectLodSSBO[i], nullptr);
        vkFreeMemory(device, indirectLodSSBOMemory[i], nullptr);
        vkDestroyBuffer(device, lodConfigSSBO[i], nullptr);
        vkFreeMemory(device, lodConfigSSBOMemory[i], nullptr);
        vkDestroyBuffer(device, drawnLastFrameSSBO[0], nullptr);
        vkFreeMemory(device, drawnLastFrameSSBOMemory[0], nullptr);
        vkDestroyBuffer(device, sphereProjectionDebugSSBO[i], nullptr);
        vkFreeMemory(device, sphereProjectionDebugSSBOMemory[i], nullptr);
    }

    vkDestroyBuffer(device, SSBO, nullptr);
    vkFreeMemory(device, SSBOMemory, nullptr);
    vkDestroyBuffer(device, scaleSSBO, nullptr);
    vkFreeMemory(device, scaleSSBOMemory, nullptr);

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
}

void VulkanObject::cleanup() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // cleanup swap chain
    cleanupSwapChain();

    vkDestroyImageView(device, offScreenPass.position.view, nullptr);
    vkDestroyImage(device, offScreenPass.position.image, nullptr);
    vkFreeMemory(device, offScreenPass.position.mem, nullptr);

    vkDestroyImageView(device, offScreenPass.albedo.view, nullptr);
    vkDestroyImage(device, offScreenPass.albedo.image, nullptr);
    vkFreeMemory(device, offScreenPass.albedo.mem, nullptr);

    vkDestroyImageView(device, offScreenPass.normal.view, nullptr);
    vkDestroyImage(device, offScreenPass.normal.image, nullptr);
    vkFreeMemory(device, offScreenPass.normal.mem, nullptr);

    vkDestroyImageView(device, offScreenPass.depth.view, nullptr);
    vkDestroyImage(device, offScreenPass.depth.image, nullptr);
    vkFreeMemory(device, offScreenPass.depth.mem, nullptr);

    vkDestroyFramebuffer(device, geometryFrameBuffer, nullptr);

    computeProgram.reset();
    depthPyramidComputeProgram.reset();
    lightingProgram.reset();
    geometryProgram.reset();
    shadowProgram.reset();

    vkDestroyRenderPass(device, earlyGeometryPass, nullptr);
    vkDestroyRenderPass(device, lateGeometryPass, nullptr);
    vkDestroyPipeline(device, lightingPipeline, nullptr);

    vkDestroyDescriptorPool(device, lightingDescriptorPool, nullptr);


    vkDestroySampler(device, textureSampler, nullptr);
    vkDestroyImageView(device, textureImageView, nullptr);

    vkDestroyImage(device, textureImage, nullptr);
    vkFreeMemory(device, textureImageMemory, nullptr);

    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);

    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);

    // destroy all semaphores and fences
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    // destory command pool memory
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyCommandPool(device, imgui_command_pool, nullptr);

    vkDestroyDescriptorPool(device, imgui_descriptor_pool, VK_NULL_HANDLE);

    // destory logical device
    vkDestroyDevice(device, nullptr);

    // if we're using validation layers we will have to clean up debug messenger
    if (enableValidationLayers) {
        // destroy debug messenger
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    // destory our surface BEFORE our instance
    vkDestroySurfaceKHR(instance, surface, nullptr);

    // destory our instance of vulkan using the default deallocator
    vkDestroyInstance(instance, nullptr);

    // These were created first, so we delete them last!
    // frees the memory allocated for our memory and invalidates the pointer
    glfwDestroyWindow(window);

    // frees all resources GLFW had taken up
    glfwTerminate();
}

void VulkanObject::createInstance() {
    // if we want to enable validation AND we cant support all the layers we want
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        // throw an error
        throw std::runtime_error("validation layers requested, but not available!");
    }

    // struct to be filled with all information about app that vulkan needs.
    // technically optional, but could provide optimisation options to GPU
    // {} is value (zero) initialisation
    VkApplicationInfo appInfo{};
    // the type of this struct is application information
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    // the name of our application is Hello Triangle
    appInfo.pApplicationName = "Hello Triangle";
    // the version of our application is 1.0.0 (major, minor, patch)
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    // we are not using an engine at this moment in time, otherwise name here
    appInfo.pEngineName = "No Engine";
    // somewhat redundant, but our (non-existant) engine's version is 1.0.0 (major, minor, patch)
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    // we are going to use vulkan 1.3
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // required struct for notifying vulkan of global validation layers and extensions to use
    VkInstanceCreateInfo createInfo{};
    // the type of this struct is instance create info
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    // reference to our application info struct above
    createInfo.pApplicationInfo = &appInfo;

    // generate vector of desired extensions
    std::vector<const char*> extensions = getRequiredExtensions();
    // add count of extensions to creatInfo as a uint32_t
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    // get vector of extensions as pointer to raw array of extension names
    createInfo.ppEnabledExtensionNames = extensions.data();


    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    // if we are using validation layers
    if (enableValidationLayers) {
        // add count of validation layers to creatInfo as a uint32_t
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        // get vector of validation layers as pointer to raw array of validation layer names
        createInfo.ppEnabledLayerNames = validationLayers.data();

        // special case for debug messenger for before full VkInstance instanciation
        populateDebugMessengerCreateInfo(debugCreateInfo);
        // vulkan allows debug messenger to be attached through pNext before instanciation
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else {
        // do not enable any validation layers
        createInfo.enabledLayerCount = 0;

        // nothing is extending this structure in this case, so nullptr
        createInfo.pNext = nullptr;
    }

    // we try to create an instance of vulkan.
    // we pass in reference to our createInfo struct, we leave the allocator as 
    // default, and a reference to our instance variable to populate

    // if that didn't work
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        // throw an exception
        throw std::runtime_error("failed to create instance!");
    }
}

// populate a given struct with callback info
void VulkanObject::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    // zero initialise
    createInfo = {};
    // type of struct
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    // specify which values of severity the debugger should be called for
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    // specify types of message the callback should be called for
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    // pointer to callback function
    createInfo.pfnUserCallback = debugCallback;
}

void VulkanObject::setupDebugMessenger() {
    // if we arent using validation layers, return
    if (!enableValidationLayers) return;

    // create a struct with debugger info in
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    // populate that structure
    populateDebugMessengerCreateInfo(createInfo);

    // attempt to create debug messenger for instance
    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        // throw if fails
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

// create our surface
void VulkanObject::createSurface() {
    // use glfw to create the window surface. This will do all of the OS specific 
    // operations that we need to get a surface and window up and running
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        // if it fails, throw error
        throw std::runtime_error("failed to create window surface!");
    }
}

void VulkanObject::pickPhysicalDevice() {
    // first we find the number of cards
    uint32_t deviceCount = 0;
    // this is the actual query
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    // no GPUs means no Vulkan!
    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    // vector of devices
    std::vector<VkPhysicalDevice> devices(deviceCount);
    // populate vector of devices
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // for each device
    for (const auto& device : devices) {
        // check if device is suitable
        if (isDeviceSuitable(device)) {
            // if it is, assign it and break
            physicalDevice = device;
            break;
        }
    }

    // if we didn't found a device
    if (physicalDevice == VK_NULL_HANDLE) {
        // throw an error
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

// create a logical device to use based off physical
void VulkanObject::createLogicalDevice() {
    // indicies of queues on physical device
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    // vector of device queue info. One for each family
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    // set of our desired queue family's values
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    // set queue priority
    float queuePriority = 1.0f;
    // for each queue family we care about
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        // create relavent queue info struct
        VkDeviceQueueCreateInfo queueCreateInfo{};
        // assign queue infor type
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        // assign family index
        queueCreateInfo.queueFamilyIndex = queueFamily;
        // assign queue count
        queueCreateInfo.queueCount = 1;
        // assign previously set priority
        queueCreateInfo.pQueuePriorities = &queuePriority;
        // push back onto our vector of queue infos
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // zero initialise device features
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.multiDrawIndirect = VK_TRUE;

    VkPhysicalDeviceVulkan11Features vulkan11Features{};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Features.shaderDrawParameters = VK_TRUE;

    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.drawIndirectCount = true;
    vulkan12Features.storageBuffer8BitAccess = true;
    vulkan12Features.uniformAndStorageBuffer8BitAccess = true;
    vulkan12Features.shaderFloat16 = true;
    vulkan12Features.shaderInt8 = true;
    vulkan12Features.samplerFilterMinmax = true;
    vulkan12Features.scalarBlockLayout = true;

    VkPhysicalDeviceVulkan13Features vulkan13Features;
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    //vulkan13Features.synchronization2 = true;

    // struct to hold device info
    VkDeviceCreateInfo createInfo{};
    // set device type
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    // number of queues
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    // pointer to queue information
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    // enabled features is set to our struct containing that information
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.pNext = &vulkan11Features;
    vulkan11Features.pNext = &vulkan12Features;
    //vulkan12Features.pNext = &vulkan13Features;

    // number of extensions to enable
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    // array of extensions to enable
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // if we are using validation layers
    if (enableValidationLayers) {
        // number of layers to enable
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        // array of layers to enable
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        // otherwise just say no validation layers
        createInfo.enabledLayerCount = 0;
    }

    // try and create logical device with above data, and if fails
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        // throw error
        throw std::runtime_error("failed to create logical device!");
    }

    VkPhysicalDeviceProperties output_props{};
	
    vkGetPhysicalDeviceProperties(physicalDevice, &output_props);

    std::cout << output_props.apiVersion << std::endl;

    // finally, get the graphics queue handle and assign it to graphicsQueue
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    // and get the presentation queue handle and assign it to presentQueue
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

// create a swap chain
void VulkanObject::createSwapChain() {
    // check that we support a swap chain, and if so, what kind
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    // store the best surface format for us
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    // store the best presentation mode for us
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    // store the best extent for us
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    // number of images we want in the swap chain
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        // make sure we dont overflow
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    // create a swap chain struct for filling in
    VkSwapchainCreateInfoKHR createInfo{};
    // assign type to swapchain info
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    // assign our surface
    createInfo.surface = surface;

    // assign our image count
    createInfo.minImageCount = imageCount;
    // assign our chosen image format
    createInfo.imageFormat = surfaceFormat.format;
    // assign our image space
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    // assign our chosen extent (size)
    createInfo.imageExtent = extent;
    // assign the number of layers our image will have (almost always 1)
    createInfo.imageArrayLayers = 1;
    // assign what we will use the images in the swap chain for. Here, their colour.
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // struct that holds queue families
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    // array of queue family indicies we will use
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    // if our presentation queue is not our graphics queue
    if (indices.graphicsFamily != indices.presentFamily) {
        // make sure that the images can be used across multiple queues without special ownership transfers
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        // specify number of queues for concurrency
        createInfo.queueFamilyIndexCount = 2;
        // specify their indicies
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        // otherwise ownership is explicitely required to access images
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    // we will nto being doing any special transform (rotations etc.)
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    // ignore alpha channel
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // assign our chosen presentation mode
    createInfo.presentMode = presentMode;
    // we don't care about obscurred pixels. Optimisation
    createInfo.clipped = VK_TRUE;

    // we presume no swap chain failure, so dont need this
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    // create the swap chain based off of our struct and assign to swapChain
    // if it fails
    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        // throw error
        throw std::runtime_error("failed to create swap chain!");
    }

    // get the count of swap chain images
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    // resize our handle ot them
    swapChainImages.resize(imageCount);
    // fill the vector with them
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    // store format and extent of the swap chain images for later use
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

// create our image views
void VulkanObject::createImageViews() {
    // create enough space in our container for the number of images in our swap chain
    swapChainImageViews.resize(swapChainImages.size());

    for (uint32_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanObject::createImguiPass()
{
    ///
    ///
    /// imgui pass
    /// 
    ///

    VkAttachmentDescription imgui_attachment = {};
    imgui_attachment.format = swapChainImageFormat;
    imgui_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    imgui_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    imgui_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    imgui_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    imgui_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    imgui_attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imgui_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference imgui_color_attachment = {};
    imgui_color_attachment.attachment = 0;
    imgui_color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription imgui_subpass = {};
    imgui_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    imgui_subpass.colorAttachmentCount = 1;
    imgui_subpass.pColorAttachments = &imgui_color_attachment;

    VkSubpassDependency imgui_dependency = {};
    imgui_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    imgui_dependency.dstSubpass = 0;
    imgui_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    imgui_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    imgui_dependency.srcAccessMask = 0;
    imgui_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &imgui_attachment;
    info.subpassCount = 1;
    info.pSubpasses = &imgui_subpass;
    info.dependencyCount = 1;
    info.pDependencies = &imgui_dependency;
    if (vkCreateRenderPass(device, &info, nullptr, &imgui_render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Could not create Dear ImGui's render pass");
    }
}

uint32_t VulkanObject::getPow2Size(uint32_t width, uint32_t height)
{
    uint32_t imageHeightPow2 = std::pow(2, static_cast<uint32_t>(std::ceil(std::log2f(swapChainExtent.height))));
    uint32_t imageWidthPow2 = std::pow(2, static_cast<uint32_t>(std::ceil(std::log2f(swapChainExtent.width))));

    return std::max(imageHeightPow2, imageWidthPow2);
}

void VulkanObject::createGeometryPass(bool const clearAttachmentsOnLoad, VkRenderPass& geometryPass)
{
    static bool firstRun = true;
    std::array<VkAttachmentDescription, 4> attachmentDescriptions{};
    std::array<VkImageView, 4> imageViews{};

    std::array<VkAttachmentReference, 2> colorAttachmentRefs{};
	
	// color 1
    if (firstRun)
    {
        createImage(swapChainExtent.width,
            swapChainExtent.height,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            offScreenPass.albedo.image,
            offScreenPass.albedo.mem);

        imageViews[0] = createImageView(offScreenPass.albedo.image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
        offScreenPass.albedo.view = imageViews[0];
    }

    attachmentDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[0].loadOp = clearAttachmentsOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[0].initialLayout = clearAttachmentsOnLoad ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    colorAttachmentRefs[0].attachment = 0;
    colorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // color 2
    if (firstRun)
    {
        createImage(swapChainExtent.width,
            swapChainExtent.height,
            VK_FORMAT_A2R10G10B10_UNORM_PACK32,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            offScreenPass.normal.image,
            offScreenPass.normal.mem);

        imageViews[1] = createImageView(offScreenPass.normal.image, VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_IMAGE_ASPECT_COLOR_BIT);
        offScreenPass.normal.view = imageViews[1];
    }

    attachmentDescriptions[1].format = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[1].loadOp = clearAttachmentsOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[1].initialLayout = clearAttachmentsOnLoad ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    colorAttachmentRefs[1].attachment = 1;
    colorAttachmentRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // output color
    attachmentDescriptions[2].format = swapChainImageFormat;
    attachmentDescriptions[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[2].loadOp = clearAttachmentsOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    attachmentDescriptions[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[2].initialLayout = clearAttachmentsOnLoad ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentDescriptions[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// depth
    if (firstRun)
    {
        uint32_t imageMaxSizePow2 = getPow2Size(swapChainExtent.width, swapChainExtent.height);

        auto mipLevels = static_cast<uint32_t>(std::log2(imageMaxSizePow2)) - 1;
        assert(mipLevels >= 1);
        createImage(imageMaxSizePow2 / 2,
            imageMaxSizePow2 / 2,
            VK_FORMAT_R32_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depthPyramidImage,
            depthPyramidMem,
            mipLevels);

        for (size_t mipLevel = 0; mipLevel < mipLevels; ++mipLevel)
        {
            depthPyramidViews.emplace_back(
                createImageView(
                    depthPyramidImage,
                    VK_FORMAT_R32_SFLOAT,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    mipLevel)
            );
        }

        depthPyramidMultiMipView = createImageView(
            depthPyramidImage,
            VK_FORMAT_R32_SFLOAT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            mipLevels);

        createImage(swapChainExtent.width,
            swapChainExtent.height,
            findDepthFormat(),
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            offScreenPass.depth.image,
            offScreenPass.depth.mem);

        imageViews[imageViews.size() - 1] = createImageView(offScreenPass.depth.image, findDepthFormat(), VK_IMAGE_ASPECT_DEPTH_BIT);
        offScreenPass.depth.view = imageViews[imageViews.size() - 1];
    }
	
    attachmentDescriptions[attachmentDescriptions.size() - 1].format = findDepthFormat();
    attachmentDescriptions[attachmentDescriptions.size() - 1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[attachmentDescriptions.size() - 1].loadOp = clearAttachmentsOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    attachmentDescriptions[attachmentDescriptions.size() - 1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[attachmentDescriptions.size() - 1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[attachmentDescriptions.size() - 1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[attachmentDescriptions.size() - 1].initialLayout = clearAttachmentsOnLoad ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentDescriptions[attachmentDescriptions.size() - 1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = static_cast<uint32_t>(attachmentDescriptions.size() - 1);
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
    std::array<VkSubpassDescription, 2> subpassDescriptions{};
    subpassDescriptions[0].flags = 0;
    subpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescriptions[0].inputAttachmentCount = 0;
    subpassDescriptions[0].pInputAttachments = nullptr;
    subpassDescriptions[0].colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
    subpassDescriptions[0].pColorAttachments = colorAttachmentRefs.data();
    subpassDescriptions[0].pResolveAttachments = nullptr;
    subpassDescriptions[0].pDepthStencilAttachment = &depthAttachmentRef;
    subpassDescriptions[0].preserveAttachmentCount = 0;
    subpassDescriptions[0].pResolveAttachments = nullptr;

    std::array<VkAttachmentReference, 3> deferredIntputAttachmentRefs{};
    deferredIntputAttachmentRefs[0].attachment = 0;
    deferredIntputAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    deferredIntputAttachmentRefs[1].attachment = 1;
    deferredIntputAttachmentRefs[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    deferredIntputAttachmentRefs[2].attachment = 3;
    deferredIntputAttachmentRefs[2].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference outputAttachmentRef{};
    outputAttachmentRef.attachment = 2;
    outputAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	
    subpassDescriptions[1].flags = 0;
    subpassDescriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescriptions[1].inputAttachmentCount = static_cast<uint32_t>(deferredIntputAttachmentRefs.size());
    subpassDescriptions[1].pInputAttachments = deferredIntputAttachmentRefs.data();
    subpassDescriptions[1].colorAttachmentCount = 1;
    subpassDescriptions[1].pColorAttachments = &outputAttachmentRef;
    subpassDescriptions[1].pResolveAttachments = nullptr;
    subpassDescriptions[1].pDepthStencilAttachment = nullptr;
    subpassDescriptions[1].preserveAttachmentCount = 0;
    subpassDescriptions[1].pResolveAttachments = nullptr;

    // a dependancy. these specify memory and execution dependencies between subpasses
    std::array<VkSubpassDependency, 2> dependencies{};
    // implicit subpass before render pass
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    // this is our subpass
    dependencies[0].dstSubpass = 0;
    // the operation to wait on before we can use image
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    // no mask
    dependencies[0].srcAccessMask = 0;
    // operations which should wait on this subpass are the colour attachment
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // implicit subpass before render pass
    dependencies[1].srcSubpass = 0;
    // this is our subpass
    dependencies[1].dstSubpass = 1;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pNext = nullptr;
    renderPassInfo.flags = 0;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
	renderPassInfo.pAttachments = attachmentDescriptions.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(subpassDescriptions.size());
    renderPassInfo.pSubpasses = subpassDescriptions.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();
    
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &geometryPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

    firstRun = false;
}

void VulkanObject::createEarlyGeometryPass()
{
    createGeometryPass(true, earlyGeometryPass);
}
void VulkanObject::createLateGeometryPass()
{
    createGeometryPass(false, lateGeometryPass);
}

void VulkanObject::createShadowPass()
{
    std::array<VkAttachmentDescription, 1> attachmentDescriptions{};
	
    createImage(swapChainExtent.width,
        swapChainExtent.height,
        findDepthFormat(),
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        shadowPass.depth.image,
        shadowPass.depth.mem);

    shadowPass.depth.view = createImageView(shadowPass.depth.image, findDepthFormat(), VK_IMAGE_ASPECT_DEPTH_BIT);
	
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &shadowPass.sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }

    VkSamplerCreateInfo pcfSamplerInfo{};
    pcfSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    pcfSamplerInfo.magFilter = VK_FILTER_LINEAR;
    pcfSamplerInfo.minFilter = VK_FILTER_LINEAR;
    pcfSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    pcfSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    pcfSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    pcfSamplerInfo.anisotropyEnable = VK_TRUE;
    VkPhysicalDeviceProperties pcfproperties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &pcfproperties);
    pcfSamplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    pcfSamplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    pcfSamplerInfo.unnormalizedCoordinates = VK_FALSE;
    pcfSamplerInfo.compareEnable = VK_TRUE;
    pcfSamplerInfo.compareOp = VK_COMPARE_OP_LESS;
    pcfSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    pcfSamplerInfo.mipLodBias = 0.0f;
    pcfSamplerInfo.minLod = 0.0f;
    pcfSamplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &pcfSamplerInfo, nullptr, &shadowPass.pcfsampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }

    attachmentDescriptions[attachmentDescriptions.size() - 1].format = findDepthFormat();
    attachmentDescriptions[attachmentDescriptions.size() - 1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[attachmentDescriptions.size() - 1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[attachmentDescriptions.size() - 1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[attachmentDescriptions.size() - 1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[attachmentDescriptions.size() - 1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[attachmentDescriptions.size() - 1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[attachmentDescriptions.size() - 1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = static_cast<uint32_t>(attachmentDescriptions.size() - 1);
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
    std::array<VkSubpassDescription, 1> subpassDescriptions{};
    subpassDescriptions[0].flags = 0;
    subpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescriptions[0].inputAttachmentCount = 0;
    subpassDescriptions[0].pInputAttachments = nullptr;
    subpassDescriptions[0].colorAttachmentCount = 0;
    subpassDescriptions[0].pColorAttachments = nullptr;
    subpassDescriptions[0].pResolveAttachments = nullptr;
    subpassDescriptions[0].pDepthStencilAttachment = &depthAttachmentRef;
    subpassDescriptions[0].preserveAttachmentCount = 0;
    subpassDescriptions[0].pResolveAttachments = nullptr;
	
    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	
    VkRenderPassCreateInfo shadowPassInfo{};
    shadowPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    shadowPassInfo.pNext = nullptr;
    shadowPassInfo.flags = 0;
    shadowPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
    shadowPassInfo.pAttachments = attachmentDescriptions.data();
    shadowPassInfo.subpassCount = static_cast<uint32_t>(subpassDescriptions.size());
    shadowPassInfo.pSubpasses = subpassDescriptions.data();
    shadowPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    shadowPassInfo.pDependencies = dependencies.data();
	
    if (vkCreateRenderPass(device, &shadowPassInfo, nullptr, &shadowPass.renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

// create our render pass object
void VulkanObject::createRenderPass()
{
    createShadowPass();
    createEarlyGeometryPass();
    createLateGeometryPass();
    createImguiPass();
}

// recreate swap chain incase it is invalidated
void VulkanObject::recreateSwapChain() {
    // if minimised
    int width = 0, height = 0;
    while (width == 0 || height == 0) {
        // minimise
        glfwGetFramebufferSize(window, &width, &height);
        // pause
        glfwWaitEvents();
    }

    ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(swapChainImages.size()));

    // wait for device to finish
    vkDeviceWaitIdle(device);

    // clear swap chain
    cleanupSwapChain();

    // create swap chain
    createSwapChain();
    // create image views off of swap chain
    createImageViews();
    // create render pass
    createRenderPass();
    createComputePipeline();
    // create graphics pipeline
    createGraphicsPipeline();
    createDepthResources();
    // create framebuffers
    createFramebuffers();

    imgui_frame_buffers.resize(swapChainImages.size());

    {
        VkImageView attachment[1];
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = imgui_render_pass;
        info.attachmentCount = 1;
        info.pAttachments = attachment;
        info.width = swapChainExtent.width;
        info.height = swapChainExtent.height;
        info.layers = 1;
        for (uint32_t i = 0; i < swapChainImages.size(); i++)
        {
            attachment[0] = swapChainImageViews[i];
            vkCreateFramebuffer(device, &info, VK_NULL_HANDLE, &imgui_frame_buffers[i]);
        }
    }

    createUniformBuffers();
    createSSBOs();
    updateSSBO();
    createDescriptorPool();

    ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(swapChainImages.size()));

    VkCommandBuffer command_buffer = beginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    endSingleTimeCommands(command_buffer);

    createCommandPool(&imgui_command_pool, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    imgui_command_buffers.resize(swapChainImageViews.size());
    createCommandBuffers(imgui_command_buffers.data(), static_cast<uint32_t>(imgui_command_buffers.size()), imgui_command_pool);

    createDescriptorSets();
    // create command buffers
    createCommandBuffers();
}

void VulkanObject::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> computeLayouts(swapChainImages.size(), computeProgram->getSetLayout());
    VkDescriptorSetAllocateInfo computeAllocInfo{};
    computeAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    computeAllocInfo.descriptorPool = computeDescriptorPool;
    computeAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
    computeAllocInfo.pSetLayouts = computeLayouts.data();

    computeDescriptorSets.resize(swapChainImages.size());
    if (vkAllocateDescriptorSets(device, &computeAllocInfo, computeDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    std::vector<VkDescriptorSetLayout> depthPyramidComputeLayouts(swapChainImages.size(), depthPyramidComputeProgram->getSetLayout());
    VkDescriptorSetAllocateInfo depthPyramidComputeAllocInfo{};
    depthPyramidComputeAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    depthPyramidComputeAllocInfo.descriptorPool = depthPyramidComputeDescriptorPool;
    depthPyramidComputeAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
    depthPyramidComputeAllocInfo.pSetLayouts = depthPyramidComputeLayouts.data();

    depthPyramidComputeDescriptorSets.resize(swapChainImages.size());
    if (vkAllocateDescriptorSets(device, &depthPyramidComputeAllocInfo, depthPyramidComputeDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), geometryProgram->getSetLayout());
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(swapChainImages.size());
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    std::vector<VkDescriptorSetLayout> lightingLayouts(swapChainImages.size(), lightingProgram->getSetLayout());
    VkDescriptorSetAllocateInfo lightingAllocInfo{};
    lightingAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    lightingAllocInfo.descriptorPool = lightingDescriptorPool;
    lightingAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
    lightingAllocInfo.pSetLayouts = lightingLayouts.data();

    lightingDescriptorSets.resize(swapChainImages.size());
    if (vkAllocateDescriptorSets(device, &lightingAllocInfo, lightingDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    std::vector<VkDescriptorSetLayout> shadowLayouts(swapChainImages.size(), shadowProgram->getSetLayout());
    VkDescriptorSetAllocateInfo shadowAllocInfo{};
    shadowAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    shadowAllocInfo.descriptorPool = shadowDescriptorPool;
    shadowAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
    shadowAllocInfo.pSetLayouts = shadowLayouts.data();

    shadowDescriptorSets.resize(swapChainImages.size());
    if (vkAllocateDescriptorSets(device, &shadowAllocInfo, shadowDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        mc::DescriptorInfo<VkDescriptorBufferInfo> uboInfo{
            uniformBuffers[i],
            0,
            sizeof(UniformBufferObject)};

        mc::DescriptorInfo<VkDescriptorBufferInfo> ssboInfo{
            SSBO,
            0,
            sizeof(ModelTransforms)};

        mc::DescriptorInfo<VkDescriptorBufferInfo> scaleSsboInfo{
            scaleSSBO,
            0,
            modelScales->size() * sizeof(float) };

        mc::DescriptorInfo<VkDescriptorBufferInfo> indirectSsboInfo{
            indirectLodSSBO[i],
            0,
            modelTransforms->modelMatricies.size() * 32};

        mc::DescriptorInfo<VkDescriptorBufferInfo> lodConfigSsboInfo{
            lodConfigSSBO[i],
            0,
            dragon_model.getTotalLodLevels() * sizeof(LodConfigData)};

        mc::DescriptorInfo<VkDescriptorBufferInfo> drawnLastFrameSsboInfo{
            drawnLastFrameSSBO[0],
            0,
            modelTransforms->modelMatricies.size() * sizeof(uint32_t)};

        mc::DescriptorInfo<VkDescriptorBufferInfo> sphereProjectionDebugSsboInfo{
            sphereProjectionDebugSSBO[i],
            0,
            modelTransforms->modelMatricies.size() * sizeof(glm::vec4) };

        mc::DescriptorInfo<VkDescriptorBufferInfo> shadowBufferInfo{
            shadowUniformBuffers[i],
            0,
            sizeof(ShadowUniformBufferObject)};

        mc::DescriptorInfo<VkDescriptorImageInfo> shadowImageInfo{
            shadowPass.sampler,
            shadowPass.depth.view,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };

        mc::DescriptorInfo<VkDescriptorImageInfo> PCFShadowImageInfo{
            shadowPass.pcfsampler,
            shadowPass.depth.view,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};

        mc::DescriptorInfo<VkDescriptorImageInfo> imageInfo{
            textureSampler,
            textureImageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        mc::DescriptorInfo<VkDescriptorImageInfo> colorDescriptorInfo{
            offScreenPass.albedo.view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        mc::DescriptorInfo<VkDescriptorImageInfo> normalDescriptorInfo{
            offScreenPass.normal.view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        mc::DescriptorInfo<VkDescriptorImageInfo> depthDescriptorInfo{
            offScreenPass.depth.view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        
        for (size_t depthPyramidDepth = 0; depthPyramidDepth < depthPyramidViews.size(); ++depthPyramidDepth)
        {
            depthPyramidDescriptorInfo.emplace_back(
                depthSampler,
                depthPyramidViews[depthPyramidDepth],
                VK_IMAGE_LAYOUT_GENERAL);
        }

        mc::DescriptorInfo<VkDescriptorImageInfo> depthMultiMipDescriptorInfo{
            depthNearestSampler,
            depthPyramidMultiMipView,
            VK_IMAGE_LAYOUT_GENERAL};

        mc::DescriptorInfo<VkDescriptorImageInfo> depthMultiMipReduceDescriptorInfo{
            depthNearestMinSampler,
            depthPyramidMultiMipView,
            VK_IMAGE_LAYOUT_GENERAL };

        std::array<VkWriteDescriptorSet, 8> computeDescriptorWrites{};

        computeDescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        computeDescriptorWrites[0].dstSet = computeDescriptorSets[i];
        computeDescriptorWrites[0].dstBinding = 0;
        computeDescriptorWrites[0].dstArrayElement = 0;
        computeDescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        computeDescriptorWrites[0].descriptorCount = 1;
        computeDescriptorWrites[0].pBufferInfo = ssboInfo.getPtr();

        computeDescriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        computeDescriptorWrites[1].dstSet = computeDescriptorSets[i];
        computeDescriptorWrites[1].dstBinding = 1;
        computeDescriptorWrites[1].dstArrayElement = 0;
        computeDescriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        computeDescriptorWrites[1].descriptorCount = 1;
        computeDescriptorWrites[1].pBufferInfo = indirectSsboInfo.getPtr();

        computeDescriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        computeDescriptorWrites[2].dstSet = computeDescriptorSets[i];
        computeDescriptorWrites[2].dstBinding = 2;
        computeDescriptorWrites[2].dstArrayElement = 0;
        computeDescriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        computeDescriptorWrites[2].descriptorCount = 1;
        computeDescriptorWrites[2].pBufferInfo = uboInfo.getPtr();

        computeDescriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        computeDescriptorWrites[3].dstSet = computeDescriptorSets[i];
        computeDescriptorWrites[3].dstBinding = 3;
        computeDescriptorWrites[3].dstArrayElement = 0;
        computeDescriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        computeDescriptorWrites[3].descriptorCount = 1;
        computeDescriptorWrites[3].pBufferInfo = lodConfigSsboInfo.getPtr();

        computeDescriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        computeDescriptorWrites[4].dstSet = computeDescriptorSets[i];
        computeDescriptorWrites[4].dstBinding = 4;
        computeDescriptorWrites[4].dstArrayElement = 0;
        computeDescriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        computeDescriptorWrites[4].descriptorCount = 1;
        computeDescriptorWrites[4].pBufferInfo = scaleSsboInfo.getPtr();

        computeDescriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        computeDescriptorWrites[5].dstSet = computeDescriptorSets[i];
        computeDescriptorWrites[5].dstBinding = 5;
        computeDescriptorWrites[5].dstArrayElement = 0;
        computeDescriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        computeDescriptorWrites[5].descriptorCount = 1;
        computeDescriptorWrites[5].pImageInfo = depthMultiMipReduceDescriptorInfo.getPtr();

        computeDescriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        computeDescriptorWrites[6].dstSet = computeDescriptorSets[i];
        computeDescriptorWrites[6].dstBinding = 6;
        computeDescriptorWrites[6].dstArrayElement = 0;
        computeDescriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        computeDescriptorWrites[6].descriptorCount = 1;
        computeDescriptorWrites[6].pBufferInfo = drawnLastFrameSsboInfo.getPtr();

        computeDescriptorWrites[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        computeDescriptorWrites[7].dstSet = computeDescriptorSets[i];
        computeDescriptorWrites[7].dstBinding = 7;
        computeDescriptorWrites[7].dstArrayElement = 0;
        computeDescriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        computeDescriptorWrites[7].descriptorCount = 1;
        computeDescriptorWrites[7].pBufferInfo = sphereProjectionDebugSsboInfo.getPtr();

        vkUpdateDescriptorSets(
            device,
            static_cast<uint32_t>(computeDescriptorWrites.size()),
            computeDescriptorWrites.data(),
            0,
            nullptr);

        std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = uboInfo.getPtr();

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = imageInfo.getPtr();

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = ssboInfo.getPtr();

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = descriptorSets[i];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = sphereProjectionDebugSsboInfo.getPtr();

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        std::array<VkWriteDescriptorSet, 8> lightingDescriptorWrites{};

        lightingDescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingDescriptorWrites[0].dstSet = lightingDescriptorSets[i];
        lightingDescriptorWrites[0].dstBinding = 0;
        lightingDescriptorWrites[0].dstArrayElement = 0;
        lightingDescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lightingDescriptorWrites[0].descriptorCount = 1;
        lightingDescriptorWrites[0].pBufferInfo = uboInfo.getPtr();

        lightingDescriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingDescriptorWrites[1].dstSet = lightingDescriptorSets[i];
        lightingDescriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        lightingDescriptorWrites[1].descriptorCount = 1;
        lightingDescriptorWrites[1].dstBinding = 1;
        lightingDescriptorWrites[1].pImageInfo = colorDescriptorInfo.getPtr();

        lightingDescriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingDescriptorWrites[2].dstSet = lightingDescriptorSets[i];
        lightingDescriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        lightingDescriptorWrites[2].descriptorCount = 1;
        lightingDescriptorWrites[2].dstBinding = 2;
        lightingDescriptorWrites[2].pImageInfo = normalDescriptorInfo.getPtr();

        lightingDescriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingDescriptorWrites[3].dstSet = lightingDescriptorSets[i];
        lightingDescriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        lightingDescriptorWrites[3].descriptorCount = 1;
        lightingDescriptorWrites[3].dstBinding = 4;
        lightingDescriptorWrites[3].pImageInfo = depthDescriptorInfo.getPtr();

        lightingDescriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingDescriptorWrites[4].dstSet = lightingDescriptorSets[i];
        lightingDescriptorWrites[4].dstBinding = 5;
        lightingDescriptorWrites[4].dstArrayElement = 0;
        lightingDescriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        lightingDescriptorWrites[4].descriptorCount = 1;
        lightingDescriptorWrites[4].pImageInfo = shadowImageInfo.getPtr();

        lightingDescriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingDescriptorWrites[5].dstSet = lightingDescriptorSets[i];
        lightingDescriptorWrites[5].dstBinding = 6;
        lightingDescriptorWrites[5].dstArrayElement = 0;
        lightingDescriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        lightingDescriptorWrites[5].descriptorCount = 1;
        lightingDescriptorWrites[5].pImageInfo = PCFShadowImageInfo.getPtr();

        lightingDescriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingDescriptorWrites[6].dstSet = lightingDescriptorSets[i];
        lightingDescriptorWrites[6].dstBinding = 7;
        lightingDescriptorWrites[6].dstArrayElement = 0;
        lightingDescriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        lightingDescriptorWrites[6].descriptorCount = 1;
        lightingDescriptorWrites[6].pImageInfo = depthMultiMipDescriptorInfo.getPtr();

        lightingDescriptorWrites[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingDescriptorWrites[7].dstSet = lightingDescriptorSets[i];
        lightingDescriptorWrites[7].dstBinding = 8;
        lightingDescriptorWrites[7].dstArrayElement = 0;
        lightingDescriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        lightingDescriptorWrites[7].descriptorCount = 1;
        lightingDescriptorWrites[7].pBufferInfo = sphereProjectionDebugSsboInfo.getPtr();

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(lightingDescriptorWrites.size()), lightingDescriptorWrites.data(), 0, nullptr);

        std::array<VkWriteDescriptorSet, 1> shadowDescriptorWrites{};

        shadowDescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        shadowDescriptorWrites[0].dstSet = shadowDescriptorSets[i];
        shadowDescriptorWrites[0].dstBinding = 0;
        shadowDescriptorWrites[0].dstArrayElement = 0;
        shadowDescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        shadowDescriptorWrites[0].descriptorCount = 1;
        shadowDescriptorWrites[0].pBufferInfo = shadowBufferInfo.getPtr();

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(shadowDescriptorWrites.size()), shadowDescriptorWrites.data(), 0, nullptr);
    }
}

void VulkanObject::createComputePipeline()
{
    {
        auto lodIndirectShaderModule = std::make_shared<mc::Shader>(
            device,
            "../shaders/vulkan3/lod_indirect.spv",
            VK_SHADER_STAGE_COMPUTE_BIT);
        computeProgram = std::make_shared<mc::ShaderProgram>(device, mc::Shaders{ lodIndirectShaderModule }, sizeof(vk::Bool32));

        VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        info.stage.module = lodIndirectShaderModule->get();
        info.stage.pName = "main";
        info.layout = computeProgram->getLayout();
        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &computePipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline!");
        }
    }

    {
        auto depthPyramidShaderModule = std::make_shared<mc::Shader>(
            device,
            "../shaders/vulkan3/depth_pyramid_generate.spv",
            VK_SHADER_STAGE_COMPUTE_BIT);
        depthPyramidComputeProgram = std::make_shared<mc::ShaderProgram>(device, mc::Shaders{ depthPyramidShaderModule }, sizeof(glm::vec2));

        VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        info.stage.module = depthPyramidShaderModule->get();
        info.stage.pName = "main";
        info.layout = depthPyramidComputeProgram->getLayout();
        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &depthPyramidComputePipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline!");
        }
    }
}

// create the graphics pipeline.
void VulkanObject::createGraphicsPipeline() {
    // create shader module per shader
    auto geometryVertShaderModule = std::make_shared< mc::Shader>(device, "../shaders/vulkan3/geometry_pass_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    auto geometryFragShaderModule = std::make_shared< mc::Shader>(device, "../shaders/vulkan3/geometry_pass_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    geometryProgram = std::make_shared<mc::ShaderProgram>(device, mc::Shaders{geometryVertShaderModule, geometryFragShaderModule});

    // create a shader stage info struct for the vertex shader
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    // assign it's type
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    // assign vertex shader to it's stage
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    // add the shader code
    vertShaderStageInfo.module = geometryVertShaderModule->get();
    // add standard name
    vertShaderStageInfo.pName = "main";

    // create a shader stage info struct for the fragment shader
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    // assign it's type
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    // assign fragment shader to it's stage
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    // add the shader code
    fragShaderStageInfo.module = geometryFragShaderModule->get();
    // add standard name
    fragShaderStageInfo.pName = "main";

    // an array which contains both shaders for convenience 
    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // a struct to store information about vertex data we will be passing to the vertex shader
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    // set type of struct
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    // for now, this data is hard coded

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // create struct to describe how geometry should be drawn. Points, lines, strips, etc.
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    // assign struct type
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    // assign drawing type
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // we dont intend on splitting geometry up with special operations
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // struct to describe region to be rendered to
    VkViewport viewport{};
    // position in window
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    // width and height in window
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    // range of depth values to use
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // struct to describe scissor rectangle (for cropping the output without transforming)
    VkRect2D scissor{};
    // position in window
    scissor.offset = { 0, 0 };
    // width and ehight
    scissor.extent = swapChainExtent;

    // create viewport state struct as a combination of viewport and scissor
    VkPipelineViewportStateCreateInfo viewportState{};
    // assign type
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    // assign viewport count
    viewportState.viewportCount = 1;
    // set the viewport to be our previously created viewport
    viewportState.pViewports = &viewport;
    // assign scissor count
    viewportState.scissorCount = 1;
    // set the scissor rectangle to be our previously created scissor rectangle
    viewportState.pScissors = &scissor;

    // create struct describing the rasteriser (i will spell it english-style!)
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    // assign type
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // fragments beyond the near and far plane are not clamped as opposed to deleted
    rasterizer.depthClampEnable = VK_FALSE;
    // we want output data from the rasteriser, so we set this false
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    // we are filling polygons we draw (so generating fragments within boundaries
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    // set the width of our boundary lines
    rasterizer.lineWidth = 1.0f;
    // we will be culling the back faces
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    // we consider vertex order to be clockwise and front facing
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    // no shadow mapping, no need for depth biasing
    rasterizer.depthBiasEnable = VK_FALSE;

    // used for anti-aliasing. struct to store info on multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    // assign struct type
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    // we disable it
    multisampling.sampleShadingEnable = VK_FALSE;
    // one 1 sample count
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // colour blend state per attached frame buffer
    std::array<VkPipelineColorBlendAttachmentState, 2> colorBlendAttachments{};
    // blend RGBA
    colorBlendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // disable
    colorBlendAttachments[0].blendEnable = VK_FALSE;
    // blend RGBA
    colorBlendAttachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // disable
    colorBlendAttachments[1].blendEnable = VK_FALSE;

    // used for all framebuffers. allows setting of blend constants that can be used as blend factors.
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    // assign type
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    // disable logicOp
    colorBlending.logicOpEnable = VK_FALSE;
    // bitwise operation specified here
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    // number of attachments
    colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
    // set as previously defined attachment
    colorBlending.pAttachments = colorBlendAttachments.data();
    // blend constants
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // create pipeline info struct
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    // assign type
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    // set number of pipeline stages (vertex and fragment in this case)
    pipelineInfo.stageCount = 2;
    // assign list of stages
    pipelineInfo.pStages = shaderStages;
    // assign vertex input info
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    // assign input assembly info
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    // assign viewport data info
    pipelineInfo.pViewportState = &viewportState;
    // assign rasteriser info
    pipelineInfo.pRasterizationState = &rasterizer;
    // assign multisampling info
    pipelineInfo.pMultisampleState = &multisampling;
    // assign colour blend info
    pipelineInfo.pColorBlendState = &colorBlending;
    // assign layout (for passing uniforms)
    pipelineInfo.layout = geometryProgram->getLayout();
    // assign renderpass
    pipelineInfo.renderPass = earlyGeometryPass;
    // number of subpasses
    pipelineInfo.subpass = 0;
    // we wont fail, so NULL
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};

    pipelineInfo.pDepthStencilState = &depthStencil;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    pipelineInfo.renderPass = lateGeometryPass;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &lateGraphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

	///////////////////////////////////////////////////// geometry

    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    // we will be culling the back faces
    rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
    // we consider vertex order to be clockwise and front facing
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // number of attachments
    colorBlending.attachmentCount = 1;
    // set as previously defined attachment
    colorBlending.pAttachments = &colorBlendAttachments[0];

    // create shader module per shader
    auto lightingVertShaderModule = std::make_shared< mc::Shader>(device, "../shaders/vulkan3/lighting_pass_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    auto lightingFragShaderModule = std::make_shared< mc::Shader>(device, "../shaders/vulkan3/lighting_pass_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    lightingProgram = std::make_shared<mc::ShaderProgram>(device, mc::Shaders{lightingVertShaderModule, lightingFragShaderModule});

    // add the shader code
    vertShaderStageInfo.module = lightingVertShaderModule->get();
    // add the shader code
    fragShaderStageInfo.module = lightingFragShaderModule->get();

    shaderStages[0] = vertShaderStageInfo;
	shaderStages[1] = fragShaderStageInfo;

    pipelineInfo.pStages = shaderStages;
    // assign layout (for passing uniforms)
    pipelineInfo.layout = lightingProgram->getLayout();
    // assign renderpass
    pipelineInfo.renderPass = earlyGeometryPass;
    // number of subpasses
    pipelineInfo.subpass = 1;
    // we wont fail, so NULL
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};

    pipelineInfo.pDepthStencilState = &depthStencil;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &lightingPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    ///////////////////////////////////////////////////////// shadow

	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    bindingDescription = Vertex::getBindingDescription();
    attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // we will be culling the back faces
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    // we consider vertex order to be clockwise and front facing
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // number of attachments
    colorBlending.attachmentCount = 0;
    // set as previously defined attachment
    colorBlending.pAttachments = &colorBlendAttachments[0];

    // create shader module per shader
    auto shadowVertShaderModule = std::make_shared<mc::Shader>(device, "../shaders/vulkan3/shadow_pass_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    auto shadowFragShaderModule = std::make_shared<mc::Shader>(device, "../shaders/vulkan3/shadow_pass_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shadowProgram = std::make_shared<mc::ShaderProgram>(device, mc::Shaders{shadowVertShaderModule, shadowFragShaderModule});

    // add the shader code
    vertShaderStageInfo.module = shadowVertShaderModule->get();
    // add the shader code
    fragShaderStageInfo.module = shadowFragShaderModule->get();

    shaderStages[0] = vertShaderStageInfo;
    shaderStages[1] = fragShaderStageInfo;

    pipelineInfo.pStages = shaderStages;
    // assign layout (for passing uniforms)
    pipelineInfo.layout = shadowProgram->getLayout();
    // assign renderpass
    pipelineInfo.renderPass = shadowPass.renderPass;
    // number of subpasses
    pipelineInfo.subpass = 0;
    // we wont fail, so NULL
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};

    pipelineInfo.pDepthStencilState = &depthStencil;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &shadowPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
}

// function to create all of our framebuffers
void VulkanObject::createFramebuffers() {
    // resize our vector to be of adaqute size
    swapChainFramebuffers.resize(swapChainImageViews.size());

    // for each image view
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 4> attachments = {
            offScreenPass.albedo.view,
        	offScreenPass.normal.view,
            swapChainImageViews[i],
            offScreenPass.depth.view,
        };

        // struct to store frame buffer info
        VkFramebufferCreateInfo framebufferInfo{};
        // type of struct
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        // specify our render pass data
        framebufferInfo.renderPass = earlyGeometryPass;
        // our attament count
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        // our attachment
        framebufferInfo.pAttachments = attachments.data();
        // width of our frame buffer
        framebufferInfo.width = swapChainExtent.width;
        // height of our frame buffer
        framebufferInfo.height = swapChainExtent.height;
        // number of layers
        framebufferInfo.layers = 1;

        // create the pipeline!! 
        // if it fails
        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            // throw error
            throw std::runtime_error("failed to create framebuffer!");
        }
    }

    std::array<VkImageView, 1> attachments = {
		shadowPass.depth.view
    };

    // struct to store frame buffer info
    VkFramebufferCreateInfo framebufferInfo{};
    // type of struct
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    // specify our render pass data
    framebufferInfo.renderPass = shadowPass.renderPass;
    // our attament count
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    // our attachment
    framebufferInfo.pAttachments = attachments.data();
    // width of our frame buffer
    framebufferInfo.width = swapChainExtent.width;
    // height of our frame buffer
    framebufferInfo.height = swapChainExtent.height;
    // number of layers
    framebufferInfo.layers = 1;

    // create the pipeline!! 
    // if it fails
    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &shadowPass.frameBuffer) != VK_SUCCESS) {
        // throw error
        throw std::runtime_error("failed to create framebuffer!");
    }
}

// create our command pool
void VulkanObject::createCommandPool() {
    // find qeues we can execute commands through
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    // struct to contain command pool info
    VkCommandPoolCreateInfo poolInfo{};
    // type of struct
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // index for our graphics queue to run graphics commands
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    // create command pool
    // if fails
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        // throws error
        throw std::runtime_error("failed to create command pool!");
    }
}

// create command buffers
void VulkanObject::createCommandBuffers() {
    // resize vector to store all command buffers
    commandBuffers.resize(swapChainFramebuffers.size());

    // struct to specify how to generate command buffers and fill command pool
    VkCommandBufferAllocateInfo allocInfo{};
    // struct type
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    // specify our command pool for storage
    allocInfo.commandPool = commandPool;
    // can be submitted to a queue directly
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    // number of command buffers to generate
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    // create command buffers
    // if fails
    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        // throw error
        throw std::runtime_error("failed to allocate command buffers!");
    }

    transitionImageLayout(depthPyramidImage, VK_FORMAT_R32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // for each command buffer generated
    for (size_t i = 0; i < commandBuffers.size(); i++) {
        // specify some info about the usage of this command buffer
        VkCommandBufferBeginInfo beginInfo{};
        // assign struct type
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        // create initial command buffer
        // if fails
        if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
            // throw error
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        std::array<VkImageMemoryBarrier, 1> initialPyramidBarriersTransition{};
        initialPyramidBarriersTransition[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        initialPyramidBarriersTransition[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        initialPyramidBarriersTransition[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        initialPyramidBarriersTransition[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        initialPyramidBarriersTransition[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        initialPyramidBarriersTransition[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        initialPyramidBarriersTransition[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        initialPyramidBarriersTransition[0].image = depthPyramidImage;
        initialPyramidBarriersTransition[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        initialPyramidBarriersTransition[0].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        initialPyramidBarriersTransition[0].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        vkCmdPipelineBarrier(
            commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            0,
            0,
            0,
            initialPyramidBarriersTransition.size(),
            initialPyramidBarriersTransition.data());

        // Bind the compute pipeline.
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        // Bind descriptor set.
        vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, computeProgram->getLayout(), 0, 1,
            &computeDescriptorSets[i], 0, nullptr);
        vk::Bool32 cullStageConstant = true;
        vkCmdPushConstants(
            commandBuffers[i],
            computeProgram->getLayout(),
            computeProgram->getPushConstantStages(),
            0,
            sizeof(cullStageConstant),
            &cullStageConstant);
        // Dispatch compute job.
        vkCmdDispatch(commandBuffers[i], modelTransforms->modelMatricies.size(), 1, 1);
        // Barrier between compute and vertex shading.
        vkCmdPipelineBarrier(
            commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,

            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            0,
            0,
            VK_NULL_HANDLE,
            0,
            VK_NULL_HANDLE,
            0,
            VK_NULL_HANDLE);
        
        // struct to specify render pass info
        VkRenderPassBeginInfo shadowRenderPassInfo{};
        // assign type
        shadowRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        // assign our previously created render pass
        shadowRenderPassInfo.renderPass = shadowPass.renderPass;
        // assign the current framebuffer
        //shadowRenderPassInfo.framebuffer = swapChainFramebuffers[i
        shadowRenderPassInfo.framebuffer = shadowPass.frameBuffer;
        // screen space offset
        shadowRenderPassInfo.renderArea.offset = { 0, 0 };
        // width and height of render
        shadowRenderPassInfo.renderArea.extent = swapChainExtent;

        std::array<VkClearValue, 1> shadowClearValues{};
        shadowClearValues[0].depthStencil = { 1.0f, 0 };

        // number of clear colour
        shadowRenderPassInfo.clearValueCount = static_cast<uint32_t>(shadowClearValues.size());
        // clear colour value
        shadowRenderPassInfo.pClearValues = shadowClearValues.data();

        vkCmdBeginRenderPass(commandBuffers[i], &shadowRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);
        
        VkBuffer vertexBuffers[] = { vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        
        vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, shadowProgram->getLayout(), 0, 1, &shadowDescriptorSets[i], 0, nullptr);

        vkCmdDrawIndexedIndirect(commandBuffers[i], indirectLodSSBO[i], 0, modelTransforms->modelMatricies.size(), 32);

        vkCmdEndRenderPass(commandBuffers[i]);

        // struct to specify render pass info
        VkRenderPassBeginInfo renderPassInfo{};
        // assign type
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        // assign our previously created render pass
        renderPassInfo.renderPass = earlyGeometryPass;
        // assign the current framebuffer
        renderPassInfo.framebuffer = swapChainFramebuffers[i];
        // screen space offset
        renderPassInfo.renderArea.offset = { 0, 0 };
        // width and height of render
        renderPassInfo.renderArea.extent = swapChainExtent;

        std::array<VkClearValue, 4> clearValues{};
        clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
        clearValues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };
        clearValues[2].color = { 0.0f, 0.0f, 0.0f, 1.0f };
        clearValues[3].depthStencil = { 1.0f, 0 };

        // number of clear colour
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        // clear colour value
        renderPassInfo.pClearValues = clearValues.data();

        // functions starting in vkCmd record commands. This ebgins the process
        vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // bind the graphics pipeline we set up
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, geometryProgram->getLayout(), 0, 1, &descriptorSets[i], 0, nullptr);

        vkCmdDrawIndexedIndirect(commandBuffers[i], indirectLodSSBO[i], 0, modelTransforms->modelMatricies.size(), 32);

        vkCmdNextSubpass(commandBuffers[i], VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipeline);

        vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, lightingProgram->getLayout(), 0, 1, &lightingDescriptorSets[i], 0, nullptr);

        vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

        // finish the render pass
        vkCmdEndRenderPass(commandBuffers[i]);

        vkCmdPipelineBarrier(
            commandBuffers[i],
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0,
            VK_NULL_HANDLE,
            0,
            VK_NULL_HANDLE,
            0,
            VK_NULL_HANDLE);
        // Bind the compute pipeline.
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, depthPyramidComputePipeline);

        std::array<VkImageMemoryBarrier, 2> initialPyramidBarriers{};
        initialPyramidBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        initialPyramidBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        initialPyramidBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        initialPyramidBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        initialPyramidBarriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        initialPyramidBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        initialPyramidBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        initialPyramidBarriers[0].image = depthPyramidImage;
        initialPyramidBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        initialPyramidBarriers[0].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        initialPyramidBarriers[0].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        initialPyramidBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        initialPyramidBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        initialPyramidBarriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        initialPyramidBarriers[1].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        initialPyramidBarriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        initialPyramidBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        initialPyramidBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        initialPyramidBarriers[1].image = offScreenPass.depth.image;
        initialPyramidBarriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        initialPyramidBarriers[1].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        initialPyramidBarriers[1].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        vkCmdPipelineBarrier(
            commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            0,
            0,
            0,
            initialPyramidBarriers.size(),
            initialPyramidBarriers.data());

        for (size_t depthPyramidLevel = 0; depthPyramidLevel < depthPyramidViews.size(); ++depthPyramidLevel)
        {
            VkDescriptorSetAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };

            allocateInfo.descriptorPool = depthPyramidComputeDescriptorPool;
            allocateInfo.descriptorSetCount = 1;
            allocateInfo.pSetLayouts = &(depthPyramidComputeProgram->getSetLayout());

            VkDescriptorSet set = 0;
            if (vkAllocateDescriptorSets(device, &allocateInfo, &set) != VK_SUCCESS)
            {
                std::cout << "could nto allocate descriptor sets" << std::endl;
                throw std::runtime_error("could nto allocate descriptor sets");
            }

            std::array<VkWriteDescriptorSet, 2> depthPyramidComputeDescriptorWrites{};

            depthPyramidComputeDescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            depthPyramidComputeDescriptorWrites[0].dstSet = set;
            depthPyramidComputeDescriptorWrites[0].dstBinding = 0;
            depthPyramidComputeDescriptorWrites[0].dstArrayElement = 0;
            depthPyramidComputeDescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            depthPyramidComputeDescriptorWrites[0].descriptorCount = 1;
            depthPyramidComputeDescriptorWrites[0].pImageInfo = depthPyramidDescriptorInfo[depthPyramidLevel].getPtr();

            depthPyramidComputeDescriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            depthPyramidComputeDescriptorWrites[1].dstSet = set;
            depthPyramidComputeDescriptorWrites[1].dstBinding = 1;
            depthPyramidComputeDescriptorWrites[1].dstArrayElement = 0;
            depthPyramidComputeDescriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            depthPyramidComputeDescriptorWrites[1].descriptorCount = 1;
            auto initialDepthDescInfo = mc::DescriptorInfo<VkDescriptorImageInfo>{
                depthSampler,
                offScreenPass.depth.view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            depthPyramidComputeDescriptorWrites[1].pImageInfo =
                depthPyramidLevel == 0 ? initialDepthDescInfo.getPtr() : depthPyramidDescriptorInfo[depthPyramidLevel - 1].getPtr();

            //vkUpdateDescriptorSetWithTemplate(device, set, depthPyramidComputeProgram.updateTemplate, descriptors);

            vkUpdateDescriptorSets(
                device,
                static_cast<uint32_t>(depthPyramidComputeDescriptorWrites.size()),
                depthPyramidComputeDescriptorWrites.data(),
                0,
                nullptr);

            /*vkUpdateDescriptorSets(
                device,
                static_cast<uint32_t>(depthPyramidComputeDescriptorWrites.size()),
                depthPyramidComputeDescriptorWrites.data(),
                0,
                nullptr);*/
            /*vkCmdPushDescriptorSetKHR(
                commandBuffers[i],
                VK_PIPELINE_BIND_POINT_COMPUTE,
                depthPyramidComputeProgram->getLayout(),
                1,
                depthPyramidComputeDescriptorWrites.size(),
                depthPyramidComputeDescriptorWrites.data());*/
            // Bind descriptor set.
            vkCmdBindDescriptorSets(
                commandBuffers[i],
                VK_PIPELINE_BIND_POINT_COMPUTE,
                depthPyramidComputeProgram->getLayout(),
                0,
                1,
                &set,
                0,
                nullptr);
            glm::vec2 reduceData;
            if (depthPyramidLevel == 0)
            {
                reduceData = {
                    static_cast<float>(swapChainExtent.width) / 2.0f,
                    static_cast<float>(swapChainExtent.height) / 2.0f };
            }
            else
            {
                reduceData = {
                    getPow2Size(swapChainExtent.width, swapChainExtent.height) / std::pow(2, depthPyramidLevel + 1),
                    getPow2Size(swapChainExtent.width, swapChainExtent.height) / std::pow(2, depthPyramidLevel + 1)};
            }

            vkCmdPushConstants(
                commandBuffers[i],
                depthPyramidComputeProgram->getLayout(),
                depthPyramidComputeProgram->getPushConstantStages(),
                0,
                sizeof(reduceData),
                &reduceData);
            // Dispatch compute job.
            vkCmdDispatch(
                commandBuffers[i],
                std::max(uint32_t{1}, (static_cast<uint32_t>(swapChainExtent.width / std::pow(2, depthPyramidLevel + 1)) / 32) + 1),
                std::max(uint32_t{1}, (static_cast<uint32_t>(swapChainExtent.height / std::pow(2, depthPyramidLevel + 1)) / 32) + 1),
                1);

            VkImageMemoryBarrier reduceBarrier{};
            reduceBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            reduceBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            reduceBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            reduceBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            reduceBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            reduceBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            reduceBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            reduceBarrier.image = depthPyramidImage;
            reduceBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            reduceBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            reduceBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

            vkCmdPipelineBarrier(
                commandBuffers[i],
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_DEPENDENCY_BY_REGION_BIT,
                0,
                0,
                0,
                0,
                1,
                &reduceBarrier);
        }
        std::array<VkImageMemoryBarrier, 1> finalPyramidBarriers{};
        finalPyramidBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        finalPyramidBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        finalPyramidBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        finalPyramidBarriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        finalPyramidBarriers[0].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        finalPyramidBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        finalPyramidBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        finalPyramidBarriers[0].image = offScreenPass.depth.image;
        finalPyramidBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        finalPyramidBarriers[0].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        finalPyramidBarriers[0].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        // Barrier between compute and vertex shading.
        vkCmdPipelineBarrier(
            commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            0,
            0,
            0,
            finalPyramidBarriers.size(),
            finalPyramidBarriers.data());

        // Bind the compute pipeline.
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        // Bind descriptor set.
        vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, computeProgram->getLayout(), 0, 1,
            &computeDescriptorSets[i], 0, nullptr);
        cullStageConstant = false;
        vkCmdPushConstants(
            commandBuffers[i],
            computeProgram->getLayout(),
            computeProgram->getPushConstantStages(),
            0,
            sizeof(cullStageConstant),
            &cullStageConstant);
        // Dispatch compute job.
        vkCmdDispatch(commandBuffers[i], modelTransforms->modelMatricies.size(), 1, 1);
        // Barrier between compute and vertex shading.
        vkCmdPipelineBarrier(
            commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,

            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            0,
            0,
            VK_NULL_HANDLE,
            0,
            VK_NULL_HANDLE,
            0,
            VK_NULL_HANDLE);

        // assign our previously created render pass
        renderPassInfo.renderPass = lateGeometryPass;
        // assign the current framebuffer
        renderPassInfo.framebuffer = swapChainFramebuffers[i];
        // screen space offset
        renderPassInfo.renderArea.offset = { 0, 0 };
        // width and height of render
        renderPassInfo.renderArea.extent = swapChainExtent;

        // number of clear colour
        renderPassInfo.clearValueCount = 0;
        // clear colour value
        renderPassInfo.pClearValues = nullptr;

        // functions starting in vkCmd record commands. This ebgins the process
        vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // bind the graphics pipeline we set up
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, lateGraphicsPipeline);

        vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, geometryProgram->getLayout(), 0, 1, &descriptorSets[i], 0, nullptr);

        vkCmdDrawIndexedIndirect(commandBuffers[i], indirectLodSSBO[i], 0, modelTransforms->modelMatricies.size(), 32);

        vkCmdNextSubpass(commandBuffers[i], VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipeline);

        vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, lightingProgram->getLayout(), 0, 1, &lightingDescriptorSets[i], 0, nullptr);

        vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

        // finish the render pass
        vkCmdEndRenderPass(commandBuffers[i]);

        // finish recording commands
        // if fails
        if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
            // throw error
            throw std::runtime_error("failed to record command buffer!");
        }
    }
}

void VulkanObject::createSyncObjects() {
    // resize semaphore and fence vector to appropriate sizes
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

    // struct to hold information on semaphore creation
    VkSemaphoreCreateInfo semaphoreInfo{};
    // assign struct type
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // info for help with creation of fences
    VkFenceCreateInfo fenceInfo{};
    // assign struct type
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // create semaphores amd fence for each image
        // if fails
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            // throw error
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

// get image from swap chain, execute command buffer, put image back in chain
void VulkanObject::drawFrame() {
    // wait for all (VK_TRUE) fences before continueing.
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // variable to store the index of an available swap chain image
    uint32_t imageIndex;
    // aquire a swap chain image. This takes the device, swap chain, no timeout, semaphore to trigger, and the variable to store the index in
    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    // if our swap chain is incompatible with surface
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // recreate swap chain
        recreateSwapChain();
        return;
    }
    // if swap chain no longer matched our surface exactly
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        // throw error
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // check if previous frame is using this image
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    // mark image as now being used by this frame
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    updateUniformBuffer(imageIndex);
    updateLODSSBO();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    ImGui::Begin("George Tattersall | Many Chickens");

    ImGuiColorEditFlags flags = ImGuiColorEditFlags_DisplayRGB;

    ImGui::Checkbox("model", &model_stage_on);
    ImGui::Checkbox("texture", &texture_stage_on);
    ImGui::Checkbox("lighting", &lighting_stage_on);
    ImGui::SliderFloat("ambient", &dragon_model.ambient, 0.0f, 1.0f);
    ImGui::SliderFloat("diffuse", &dragon_model.diffuse, 0.0f, 1.0f);
    ImGui::SliderFloat("specular", &dragon_model.specular, 0.0f, 1.0f);
    //ImGui::SliderFloat("zoom", &zoom, 10.0f, 100.0f);
    //ImGui::SliderFloat("scale", &scale, 0.1f, 2.0f);
    //ImGui::SliderFloat("X offset", &x_offset, -100.0f, 100.0f);
    //ImGui::SliderFloat("Y offset", &y_offset, -100.0f, 100.0f);
    //ImGui::SliderFloat("Z offset", &z_offset, -100.0f, 100.0f);
    //ImGui::SliderFloat("X model rotation", &x_rotation, 0.0f, 2 * glm::pi<float>());
    //ImGui::SliderFloat("Y model rotation", &y_rotation, 0.0f, 2 * glm::pi<float>());
    //ImGui::SliderFloat("Z model rotation", &z_rotation, 0.0f, 2 * glm::pi<float>());
    ImGui::SliderFloat("Y light rotation", &y_light_rotation, 0.0f, 2 * glm::pi<float>());
    ImGui::SliderFloat("Z light rotation", &z_light_rotation, 0.0f, 2 * glm::pi<float>());
    //ImGui::SliderFloat("X camera rotation", &camera_x_rotation, 0.0f, 2 * glm::pi<float>());
    //ImGui::SliderFloat("Y camera rotation", &camera_y_rotation, 0.0f, 2 * glm::pi<float>());
    //ImGui::SliderFloat("Z camera rotation", &camera_z_rotation, 0.0f, 2 * glm::pi<float>());
    //ImGui::SliderFloat("shininess (Ns)", &dragon_model.Ns, 0.00f, 128.0f);
    ImGui::SliderFloat("Shadow bias", &shadow_bias, -0.001f, 0.001f);
    ImGui::ColorEdit3("ambient (Ka)", (float*)&dragon_model.Ka[0], flags);
    ImGui::ColorEdit3("diffuse (Kd)", (float*)&dragon_model.Kd[0], flags);
    ImGui::ColorEdit3("specular (Ks)", (float*)&dragon_model.Ks[0], flags);
    ImGui::ColorEdit3("emission (Ke)", (float*)&dragon_model.Ke[0], flags);
    auto& max_dists = dragon_model.getMaxDistances();
    if (ImGui::GetFrameCount() <= 3)
    {
        max_dists[0] = 0.0f;
        max_dists[1] = 0.0f;
        max_dists[2] = 2.2f;
        max_dists[3] = 5.5f;
        max_dists[4] = 50.0f;
    }
    ImGui::SliderFloat("LOD level 0 max dist", &max_dists[0], 0.00f, max_dists[1]);
    ImGui::SliderFloat("LOD level 1 max dist", &max_dists[1], max_dists[0], max_dists[2]);
    ImGui::SliderFloat("LOD level 2 max dist", &max_dists[2], max_dists[1], max_dists[3]);
    ImGui::SliderFloat("LOD level 3 max dist", &max_dists[3], max_dists[2], max_dists[4]);
    ImGui::SliderFloat("LOD level 4 max dist", &max_dists[4], max_dists[3], 10000.0f);
    ImGui::RadioButton("normals", &display_mode, 0);
    ImGui::RadioButton("depth", &display_mode, 1);
    ImGui::RadioButton("specularity", &display_mode, 2);
    ImGui::RadioButton("albedo", &display_mode, 3);
    ImGui::RadioButton("shadow", &display_mode, 4);
    ImGui::RadioButton("position", &display_mode, 5);
    bool pyramidJustActivated = ImGui::RadioButton("Depth pyramid", &display_mode, 6);
    if(display_mode >= 6 && display_mode < 20)
    {
        if (pyramidJustActivated)
        {
            display_mode = 7;
        }
        ImGui::NewLine();
        for (size_t lodLevel = 0; lodLevel < depthPyramidViews.size(); ++lodLevel)
        {
            std::string radioButtomName = "Mip " + std::to_string(lodLevel);
            ImGui::Indent(1.0f); ImGui::SameLine();
            ImGui::RadioButton(radioButtomName.c_str(), &display_mode, 7 + lodLevel);
            if (lodLevel + 1 != depthPyramidViews.size())
            {
                ImGui::NewLine();
            }
            ImGui::Unindent(1.0f);
        }
    }
    bool SSAABBJustActivated = ImGui::RadioButton("SSAABB", &display_mode, 20);
    if (display_mode >= 20 && display_mode < 23)
    {
        if (SSAABBJustActivated)
        {
            display_mode = 21;
        }
        ImGui::NewLine();
        ImGui::Indent(1.0f); ImGui::SameLine();
        ImGui::RadioButton("chicken chickens", &display_mode, 21);
        ImGui::NewLine();
        ImGui::Indent(1.0f); ImGui::SameLine();
        ImGui::RadioButton("chicken balls", &display_mode, 22);
        ImGui::Unindent(1.0f);
    }
    ImGui::RadioButton("lodLevel", &display_mode, 23);
    ImGui::RadioButton("composed", &display_mode, 24); ImGui::SameLine();
    ImGui::Checkbox("PCF", &pcf);
   
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();

    ImGui::Render();

    vkResetCommandBuffer(imgui_command_buffers[imageIndex], 0);

    {
        VkCommandBufferBeginInfo imgui_info = {};
        imgui_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        imgui_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(imgui_command_buffers[imageIndex], &imgui_info);
    }

    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = imgui_render_pass;
        info.framebuffer = imgui_frame_buffers[imageIndex];
        info.renderArea.extent.width = swapChainExtent.width;
        info.renderArea.extent.height = swapChainExtent.height;
        info.clearValueCount = 1;
        info.pClearValues = &imgui_clear_value;
        vkCmdBeginRenderPass(imgui_command_buffers[imageIndex], &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), imgui_command_buffers[imageIndex]);

    vkCmdEndRenderPass(imgui_command_buffers[imageIndex]);
    vkEndCommandBuffer(imgui_command_buffers[imageIndex]);

    std::array<VkCommandBuffer, 2> submitCommandBuffers = { commandBuffers[imageIndex], imgui_command_buffers[imageIndex] };
    // struct to hold info about queue submissions
    VkSubmitInfo submitInfo{};
    // assign type
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // which semaphores to wait for before using image
    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    // what the correesponding semaphore in waitSemaphores is waiting ot do
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    // semaphore count
    submitInfo.waitSemaphoreCount = 1;
    // list of semaphores
    submitInfo.pWaitSemaphores = waitSemaphores;
    // list of what they do
    submitInfo.pWaitDstStageMask = waitStages;

    // number of command buffers
    submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers.size());
    // c array of command buffers
    submitInfo.pCommandBuffers = submitCommandBuffers.data();

    // which semaphores to signal when we are done with image
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    // number of them
    submitInfo.signalSemaphoreCount = 1;
    // assign semaphores
    submitInfo.pSignalSemaphores = signalSemaphores;

    // reset state of all fences
    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    // submit command buffer to graphics queue. takes array of command buffers.
    // if fails
    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        // throw error
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    // presentation configuration struct
    VkPresentInfoKHR presentInfo{};
    // assign struct type
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    // number of semaphores to wait for before presenting
    presentInfo.waitSemaphoreCount = 1;
    // semaphores to wait for before presenting
    presentInfo.pWaitSemaphores = signalSemaphores;

    // swap chains to present to
    VkSwapchainKHR swapChains[] = { swapChain };
    // number of swap chains
    presentInfo.swapchainCount = 1;
    // swap chains
    presentInfo.pSwapchains = swapChains;

    // image indices
    presentInfo.pImageIndices = &imageIndex;

    // queue the presentation info on the presentation queue!
    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    // if we failed to draw because of changes to surface
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        // recreate chain
        recreateSwapChain();
    }
    // if failed
    else if (result != VK_SUCCESS) {
        // throw error
        throw std::runtime_error("failed to present swap chain image!");
    }

    // update current frame
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanObject::updateUniformBuffer(uint32_t currentImage) {
    UniformBufferObject ubo{};
    glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0), glm::vec3(x_offset, y_offset, z_offset));
    glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0), glm::vec3(scale));
    glm::mat4 rotation_matrix = glm::rotate(x_rotation, glm::vec3(1.0, 0.0, 0.0));
    rotation_matrix *= glm::rotate(y_rotation, glm::vec3(0.0, 1.0, 0.0));
    rotation_matrix *= glm::rotate(z_rotation, glm::vec3(0.0, 0.0, 1.0));

    glm::mat4 camera_rotation_matrix = glm::rotate(camera_x_rotation, glm::vec3(1.0, 0.0, 0.0));
    camera_rotation_matrix *= glm::rotate(camera_y_rotation, glm::vec3(0.0, 1.0, 0.0));
    camera_rotation_matrix *= glm::rotate(camera_z_rotation, glm::vec3(0.0, 0.0, 1.0));

    ubo.model = translation_matrix * rotation_matrix * scale_matrix;
    //ubo.view = glm::lookAt(glm::vec3(camera_rotation_matrix * glm::vec4(-2.0f, 0.0f, 0.0f, 1.0f)), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.view = camera->GetViewMatrix();
    ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.001f, 250.0f);
    ubo.proj[1][1] *= -1;

    ubo.p00 = ubo.proj[0][0];
    ubo.p11 = ubo.proj[1][1];
    ubo.zNear = 0.001f;

    ubo.light = glm::rotate(x_light_rotation, glm::vec3(1.0, 0.0, 0.0));
    ubo.light *= glm::rotate(y_light_rotation, glm::vec3(0.0, 1.0, 0.0));
    ubo.light *= glm::rotate(z_light_rotation, glm::vec3(0.0, 0.0, 1.0));

    ubo.Ns = dragon_model.Ns;
    ubo.Ka = glm::vec4(dragon_model.Ka, 1.0f);
    ubo.Kd = glm::vec4(dragon_model.Kd, 1.0f);
    ubo.Ks = glm::vec4(dragon_model.Ks, 1.0f);
    ubo.Ke = glm::vec4(dragon_model.Ke, 1.0f);
    ubo.ambient = dragon_model.ambient;
    ubo.diffuse = dragon_model.diffuse;
    ubo.specular = dragon_model.specular;

    ubo.shadow_bias = shadow_bias;

    ubo.model_stage_on = model_stage_on;
    ubo.texture_stage_on = texture_stage_on;
    ubo.lighting_stage_on = lighting_stage_on;

    ubo.display_mode = display_mode;

    ubo.pcf_on = pcf;

    ubo.win_dim = glm::vec2(swapChainExtent.width, swapChainExtent.height);

    glm::mat4 light_view = glm::lookAt(glm::vec3(ubo.light * glm::vec4(-2.5f, 0.0f, 0.0f, 1.0f)), glm::vec3(0.0), glm::vec3(0.0, 1.0, 0.0));
    glm::mat4 light_proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.001f, 4.0f);
    light_proj[1][1] *= -1;

    ubo.lightVP = light_proj * light_view;

    void* data;
    vkMapMemory(device, uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(device, uniformBuffersMemory[currentImage]);

    ShadowUniformBufferObject subo{};
	
    subo.depthMVP = ubo.lightVP * ubo.model;

    vkMapMemory(device, shadowUniformBuffersMemory[currentImage], 0, sizeof(subo), 0, &data);
    memcpy(data, &subo, sizeof(subo));
    vkUnmapMemory(device, shadowUniformBuffersMemory[currentImage]);
}

void VulkanObject::updateSSBO() {
    modelTransforms = std::make_unique<ModelTransforms>();
    modelScales = std::make_unique<decltype(modelScales)::element_type>();

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_real_distribution<float> translation_dist(-5.0f, 5.0f);
    std::uniform_real_distribution<float> scale_dist(0.1f, 5.0f);
    std::uniform_real_distribution<float> rotation_dist(0.0f, 2.0f * glm::pi<float>());

    //std::fill(std::begin(ssbo->modelMatricies), std::end(ssbo->modelMatricies), glm::mat4{ 1.0f });
    for (size_t matrixIndex = 0; matrixIndex < modelTransforms->modelMatricies.size(); ++matrixIndex)
    {
        glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(translation_dist(rng), translation_dist(rng), translation_dist(rng)))
            * glm::translate(glm::mat4(1.0f), glm::vec3(17.0f, 0.0f, 0.0f));
        float scale = scale_dist(rng);
        modelScales->operator[](matrixIndex) = scale;
        glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0), glm::vec3(modelScales->operator[](matrixIndex)));
        glm::mat4 rotation_matrix = glm::rotate(rotation_dist(rng), glm::vec3(1.0f, 0.0f, 0.0f));
        rotation_matrix *= glm::rotate(rotation_dist(rng), glm::vec3(0.0f, 1.0f, 0.0f));
        rotation_matrix *= glm::rotate(rotation_dist(rng), glm::vec3(0.0f, 0.0f, 1.0f));

        modelTransforms->modelMatricies[matrixIndex] = translation_matrix * rotation_matrix * scale_matrix;
    }

    void* data;
    vkMapMemory(device, SSBOMemory, 0, sizeof(ModelTransforms), 0, &data);
    memcpy(data, modelTransforms.get(), sizeof(ModelTransforms));
    vkUnmapMemory(device, SSBOMemory);

    data = nullptr;
    vkMapMemory(device, scaleSSBOMemory, 0, modelScales->size() * sizeof(float), 0, &data);
    memcpy(data, modelScales.get(), modelScales->size() * sizeof(float));
    vkUnmapMemory(device, scaleSSBOMemory);

    updateLODSSBO();


    std::cout << "Updating drawnLastFrameBuffer" << std::endl;
    drawnLastFrameSSBO.resize(swapChainImages.size());
    drawnLastFrameSSBOMemory.resize(swapChainImages.size());

    auto zerodVisibility = std::make_unique<std::array<vk::Bool32, chickenCount>>();
    zerodVisibility->fill(false);

    //for (size_t i = 0; i < swapChainImages.size(); i++)
    //{
        data = nullptr;
        vkMapMemory(device, drawnLastFrameSSBOMemory[0], 0, chickenCount * sizeof(vk::Bool32), 0, &data);
        memcpy(data, zerodVisibility.get(), chickenCount * sizeof(vk::Bool32));
        vkUnmapMemory(device, drawnLastFrameSSBOMemory[0]);
    //}
}

void VulkanObject::updateLODSSBO()
{
    void* data;
    auto const lodConfig = dragon_model.getLodConfigData();
    for (size_t i = 0; i < swapChainImageViews.size(); i++)
    {
        vkMapMemory(device, lodConfigSSBOMemory[i], 0, dragon_model.getTotalLodLevels() * sizeof(LodConfigData), 0, &data);
        memcpy(data, lodConfig.data(), dragon_model.getTotalLodLevels() * sizeof(LodConfigData));
        vkUnmapMemory(device, lodConfigSSBOMemory[i]);
    }
}

// create a VkShaderModule to encapsulate our shaders
VkShaderModule VulkanObject::createShaderModule(const std::vector<char>& code) {

    // create struct to hold shader module info
    VkShaderModuleCreateInfo createInfo{};
    // assign type to struct
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    // assign the size of our shader code
    createInfo.codeSize = code.size();
    // assign the code itself
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    // declare a shader module
    VkShaderModule shaderModule;
    // create a shader module based off of our info struct
    // if it fails
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        // throw an error
        throw std::runtime_error("failed to create shader module!");
    }

    //return shader module
    return shaderModule;
}

// function for choosing the format to use from available formats
VkSurfaceFormatKHR VulkanObject::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    // for each available format
    for (const auto& availableFormat : availableFormats) {
        // if the format supports 8 bit BGRA_SRGB and SRGB non linear colour space
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            // return suitable format
            return availableFormat;
        }
    }

    // if we dont have the prefered format, just pick the first
    return availableFormats[0];
}

// function to choose a prefere presentation mode
VkPresentModeKHR VulkanObject::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {

    // for all available presentation modes
    for (const auto& availablePresentMode : availablePresentModes) {
        // if it has a FIFO mode which replaces with new frames if full
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            // select this and return
            return availablePresentMode;
        }
    }

    // otherwise default to FIFO  with blocking
    return VK_PRESENT_MODE_FIFO_KHR;
}

// function to choose a good width and height for images
VkExtent2D VulkanObject::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    // if everything is normal
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        // screen coordinates are not always the same as pixels.
        // to account for this we can use glfwGetFramebufferSize
        // to get a reasonable pixel width and height to fit the
        // screen coordinate size of our window.
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        // we convert to the correct types SAFELY and then assign
        // to a vulkan type for proer processing
        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        // we now make sure that the GPU can actually write to the size we want, cropping if required
        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        // return final size
        return actualExtent;
    }
}

// populate swap chain struct
SwapChainSupportDetails VulkanObject::querySwapChainSupport(VkPhysicalDevice device) {
    // swap chain struct to populate
    SwapChainSupportDetails details;

    // get capabilities from a VkPhysicalDevice "device" and VkSurfaceKHR window surface "surface", outputting to our struct
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    // count of available formats
    uint32_t formatCount;
    // get format count
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    // if there are supported formats
    if (formatCount != 0) {
        // store formats in our struct
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    // count of presentation modes
    uint32_t presentModeCount;
    // get presentation mode count
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    // if there are presentation mdoes
    if (presentModeCount != 0) {
        // stroe presentation modes in our struct
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    // return our data struct
    return details;
}

// check a physical device to see if it is suitable
bool VulkanObject::isDeviceSuitable(VkPhysicalDevice device) {
    // struct with indicies of our queue families (if available)
    QueueFamilyIndices indices = findQueueFamilies(device);

    VkPhysicalDeviceProperties props;

    vkGetPhysicalDeviceProperties(device, &props);

    if (props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        return false;

    // check that we can support all extensions
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    // bool to check if our swap chain is usable
    bool swapChainAdequate = false;
    // if we can support all extensions
    if (extensionsSupported) {
        // check that we have at least one image format and presentation mode to use
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    // if we have all queues, extension support and swap chain support
    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy && supportedFeatures.multiDrawIndirect;
}

// check that our device has support for the set of extensions we are interested in
bool VulkanObject::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    // number of extensions
    uint32_t extensionCount;
    // assign to variable
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    // vector of available device extensions
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    // populate vector
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    // use set for easy validation
    // set of our required extensions
    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    // if our device supports them
    for (const auto& extension : availableExtensions) {
        // remove from set
        requiredExtensions.erase(extension.extensionName);
    }

    // return true iff all supported
    return requiredExtensions.empty();
}

// search for queue family support
QueueFamilyIndices VulkanObject::findQueueFamilies(VkPhysicalDevice device) {
    // struct to hold queue family data
    QueueFamilyIndices indices;

    // sotre number of queue families available
    uint32_t queueFamilyCount = 0;
    // assign to that value
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    // vector to store queue families
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    // populate that vector
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    // loop over all available queue families
    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        // if queue family contains graphics flag
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            // assign that queue index to our queue struct
            indices.graphicsFamily = i;
        }

        // check whether we support drawing to surface
        VkBool32 presentSupport = false;
        // actually perform check
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        // if we can
        if (presentSupport) {
            // set index that we can
            indices.presentFamily = i;
        }

        // early exit check on queue struct that we have found all
        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

// return required list of extensions
std::vector<const char*> VulkanObject::getRequiredExtensions() {
    // count of GLFW extensions
    uint32_t glfwExtensionCount = 0;
    // C style array of GLFW extensions
    const char** glfwExtensions;

    // GLFW provides this function that returns the extensions required for the interface between vulkan and itself
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // create vector of extensions
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // is we have enabled validation layers (debug)
    if (enableValidationLayers) {
        // add VK_EXT_DEBUG_UTILS_EXTENSION_NAME extension
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // return the vector of desired extensions
    return extensions;
}

// check whether all requested layers are available. return true iff they are
bool VulkanObject::checkValidationLayerSupport() {
    // number of layers available
    uint32_t layerCount;
    // first get the layercount
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    // vector to store all available layers
    std::vector<VkLayerProperties> availableLayers(layerCount);
    // populate vector with all available layers
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    // for each layer in the available layers
    for (const char* layerName : validationLayers) {
        // bool to store if current layer is found
        bool layerFound = false;

        // for each property in a given layer
        for (const auto& layerProperties : availableLayers) {
            // check if the layer we want is in the availbale layers
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                // if found, set true
                layerFound = true;
                // early break
                break;
            }
        }

        // if we are missing ANY layer
        if (!layerFound) {
            // return false for whole function
            return false;
        }
    }

    // If we are here we have all layers, return true
    return true;
}

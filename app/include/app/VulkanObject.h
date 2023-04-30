#pragma once

#include "vulkan/vulkan.hpp"
#include "VulkanStructs.h"
#include "UBO.h"
#include "Vertex.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <iostream>
#include <optional>

#include "app/Camera.h"
#include "app/Model.h"
#include "app/ShaderProgram.h"
#include "app/DescriptorInfo.h"

class VulkanObject {
public:
    void initVulkan(GLFWwindow* window, std::shared_ptr<mc::Camera> camera);
    void drawFrame();
    void cleanup();

    VkDevice device;

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        VulkanObject* app = reinterpret_cast<VulkanObject*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    static void mouseMoveCallback(GLFWwindow* window, double xposIn, double yposIn)
    {
        VulkanObject* app = reinterpret_cast<VulkanObject*>(glfwGetWindowUserPointer(window));
        if (app->camera)
        {
            app->camera->mouse_callback(window, xposIn, yposIn);
        }
    }

    static void scrollMoveCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        VulkanObject* app = reinterpret_cast<VulkanObject*>(glfwGetWindowUserPointer(window));
        if (app->camera)
        {
            app->camera->scroll_callback(window, xoffset, yoffset);
        }
    }

    std::shared_ptr<mc::Camera> camera;

private:
    // pointer to GLFW window instance
    GLFWwindow* window;

    VkClearValue imgui_clear_value;

    int MAX_FRAMES_IN_FLIGHT = 2;

    // vulkan library instance
    VkInstance instance;
    // create instance of debug messenger
    VkDebugUtilsMessengerEXT debugMessenger;
    VkDebugReportCallbackEXT reportCallbackMessengerEXT;
    // create a "surface" to interface with any window system
    VkSurfaceKHR surface;

    // physical device to use
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    // logical device to use

    // handle to graphics queue
    VkQueue graphicsQueue;
    // handle to graphics queue
    VkQueue presentQueue;

    // our swap chain object
    VkSwapchainKHR swapChain;
    // vector of our swap chain images
    std::vector<VkImage> swapChainImages;
    // the format of our swap chain
    VkFormat swapChainImageFormat;
    // the extent (dimensions) of our swap chain images
    VkExtent2D swapChainExtent;

    struct FrameBufferAttachment {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
        VkFormat format;
    };
    struct FrameBuffer {
        int32_t width, height;
        VkFramebuffer frameBuffer;
        FrameBufferAttachment position, normal, albedo;
        FrameBufferAttachment depth;
        VkRenderPass renderPass;
    } offScreenPass;

    VkImage depthPyramidImage;
    VkDeviceMemory depthPyramidMem;
    std::vector<VkImageView> depthPyramidViews;
    VkImageView depthPyramidMultiMipView;
    std::vector<VkSampler> depthPyramidSamplers;
    std::vector<mc::DescriptorInfo<VkDescriptorImageInfo>> depthPyramidDescriptorInfo;

    struct DepthFrameBuffer {
        int32_t width, height;
        VkFramebuffer frameBuffer;
        FrameBufferAttachment depth;
        VkSampler sampler;
        VkSampler pcfsampler;
        VkRenderPass renderPass;
    } shadowPass;

    // vector of image views (to access our images)
    std::vector<VkImageView> swapChainImageViews;
    // vector of all frame buffers
    std::vector<VkFramebuffer> swapChainFramebuffers;

    std::vector<VkFramebuffer> imgui_frame_buffers;

    VkFramebuffer geometryFrameBuffer;

    // render pass object
    VkRenderPass renderPass;
    VkRenderPass earlyGeometryPass;
    VkRenderPass lateGeometryPass;
    VkRenderPass imgui_render_pass;
    //VkDescriptorSetLayout descriptorSetLayout;
    //VkDescriptorSetLayout lightingSetLayout;
    //VkDescriptorSetLayout shadowSetLayout;
    std::shared_ptr<mc::ShaderProgram> computeProgram;
    std::shared_ptr<mc::ShaderProgram> depthPyramidComputeProgram;
    std::shared_ptr<mc::ShaderProgram> geometryProgram;
    std::shared_ptr<mc::ShaderProgram> lightingProgram;
    std::shared_ptr<mc::ShaderProgram> shadowProgram;
    VkPipeline computePipeline;
    VkPipeline depthPyramidComputePipeline;
    VkPipeline graphicsPipeline;
    VkPipeline lateGraphicsPipeline;
    VkPipeline lightingPipeline;
    VkPipeline shadowPipeline;

    // create a command pool to manage the memory required for our command buffers
    VkCommandPool commandPool;
    // vector of command buffers. One for each image in swap chain
    std::vector<VkCommandBuffer> commandBuffers;

    VkCommandPool imgui_command_pool;
    std::vector<VkCommandBuffer> imgui_command_buffers;

    // vector of semaphores indicating images have been aquired
    std::vector<VkSemaphore> imageAvailableSemaphores;
    // vector of semaphores indicating images are ready for rendering
    std::vector<VkSemaphore> renderFinishedSemaphores;
    // create a fence for each frame
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    // the current frame we are working on
    size_t currentFrame = 0;

    // bool to store if we have resized
    bool framebufferResized = false;

    Model<5> dragon_model;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    VkBuffer SSBO;
    VkDeviceMemory SSBOMemory;
    VkBuffer scaleSSBO;
    VkDeviceMemory scaleSSBOMemory;
    std::vector<VkBuffer> indirectLodSSBO;
    std::vector<VkDeviceMemory> indirectLodSSBOMemory;
    std::vector<VkBuffer> indirectLodCountSSBO;
    std::vector<VkDeviceMemory> indirectLodCountSSBOMemory;
    std::vector<VkBuffer> lodConfigSSBO;
    std::vector<VkDeviceMemory> lodConfigSSBOMemory;
    std::vector<VkBuffer> drawnLastFrameSSBO;
    std::vector<VkDeviceMemory> drawnLastFrameSSBOMemory;
    std::vector<VkBuffer> sphereProjectionDebugSSBO;
    std::vector<VkDeviceMemory> sphereProjectionDebugSSBOMemory;

    static constexpr size_t chickenCount = 150000;// 50;

    float timestampPeriod = 1.0f;

    struct ModelTransforms {
        std::array<glm::mat4, chickenCount> modelMatricies{};
    };

    std::unique_ptr<ModelTransforms> modelTransforms;
    std::unique_ptr<std::array<float, chickenCount>> modelScales;

    void createSSBOs();

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    std::vector<VkBuffer> shadowUniformBuffers;
    std::vector<VkDeviceMemory> shadowUniformBuffersMemory;

    VkDescriptorPool computeDescriptorPool;
    VkDescriptorPool depthPyramidComputeDescriptorPool;
    VkDescriptorPool descriptorPool;
    VkDescriptorPool lightingDescriptorPool;
    VkDescriptorPool shadowDescriptorPool;
    std::vector<VkDescriptorSet> computeDescriptorSets;
    std::vector<VkDescriptorSet> depthPyramidComputeDescriptorSets;
    std::vector<VkDescriptorSet> descriptorSets;
    std::vector<VkDescriptorSet> lightingDescriptorSets;
    std::vector<VkDescriptorSet> shadowDescriptorSets;
    VkDescriptorPool imgui_descriptor_pool;

    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    VkSampler depthSampler;

    VkImage meshesDrawnDebugViewImage;
    VkDeviceMemory meshesDrawnDebugViewImageMemory;
    VkImageView meshesDrawnDebugViewImageView;
    VkSampler meshesDrawnDebugViewSampler;
    VkDescriptorSet meshesDrawnDebugViewImageViewImGUITexID;

    VkSampler depthNearestSampler;
    VkSampler depthNearestMinSampler;

    std::string MODEL_PATH;
    std::string TEXTURE_PATH;

    std::vector<VkQueryPool> queryPools;
    std::pair<uint32_t, uint32_t> earlyCullQueryIndices;
    std::pair<uint32_t, uint32_t> earlyRenderQueryIndices;
    std::pair<uint32_t, uint32_t> depthPyramidQueryIndices;
    std::pair<uint32_t, uint32_t> lateCullQueryIndices;
    std::pair<uint32_t, uint32_t> lateRenderQueryIndices;

    static constexpr size_t queryHistorySamples = 1000;

    std::array<float, queryHistorySamples> earlyCullTimeHistory = {};
    std::array<float, queryHistorySamples> earlyRenderTimeHistory = {};
    std::array<float, queryHistorySamples> depthPyramidTimeHistory = {};
    std::array<float, queryHistorySamples> lateCullTimeHistory = {};
    std::array<float, queryHistorySamples> lateRenderTimeHistory = {};

    bool updatingImGuiQueryData = true;

    bool model_stage_on = false;
    bool texture_stage_on = false;
    bool lighting_stage_on = false;
    float zoom = 10.0;
    float x_offset = 0;
    float y_offset = 0;
    float z_offset = 0;
    float camera_x_rotation = 0;
    float camera_y_rotation = 0;
    float camera_z_rotation = 0;
    float x_rotation = 0;
    float y_rotation = 0;
    float z_rotation = 0;
    float x_light_rotation = 0;
    float y_light_rotation = 0;
    float z_light_rotation = 0;
    float scale = 1.0f;
    int display_mode = 0;
    float shadow_bias = 0.0;
    bool pcf = false;
    std::string save_path;
    bool updating_pos = true;

    UniformBufferObject ubo{};

    void createImguiPass();
    void createGeometryPass(bool clearAttachmentsOnLoad, VkRenderPass& renderPass);
    void createEarlyGeometryPass();
    void createLateGeometryPass();
    void createShadowPass();

    VkFormat findDepthFormat();

    bool hasStencilComponent(VkFormat format);

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    void createDepthResources();

    void createTextureSampler();

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t baseMipLevel = 0, uint32_t levelCount = 1);

    void loadModel();

    void createTextureImageView();

    void createTextureImage();

    void createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory,
        uint32_t mipLevels = 1);

    void createDescriptorPool();

    void createQueryPools();

    void createUniformBuffers();

    void createIndexBuffer();

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    void createVertexBuffer();

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t baseMipLevel = 0, uint32_t levelCount = VK_REMAINING_MIP_LEVELS);

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    VkCommandBuffer beginSingleTimeCommands();

    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    void createCommandPool(VkCommandPool* commandPool, VkCommandPoolCreateFlags flags);

    void createCommandBuffers(VkCommandBuffer* commandBuffer, uint32_t commandBufferCount, VkCommandPool& commandPool);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // clean up swap chain for a clean recreate
    void cleanupSwapChain();

    void createInstance();

    // populate a given struct with callback info
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    void setupDebugMessenger();

    // create our surface
    void createSurface();

    void pickPhysicalDevice();

    // create a logical device to use based off physical
    void createLogicalDevice();

    // create a swap chain
    void createSwapChain();

    // create our image views
    void createImageViews();

    // create our render pass object
    void createRenderPass();

    // recreate swap chain incase it is invalidated
    void recreateSwapChain();

    void createDescriptorSets();

    void createComputePipeline();

    // create the graphics pipeline.
    void createGraphicsPipeline();

    // function to create all of our framebuffers
    void createFramebuffers();

    // create our command pool
    void createCommandPool();

    // create command buffers
    void createCommandBuffers();

    void createSyncObjects();

    void updateUniformBuffer(uint32_t currentImage);

    void updateSSBO();

    void updateLODSSBO();

    uint32_t getPow2Size(uint32_t width, uint32_t height);

    // create a VkShaderModule to encapsulate our shaders
    VkShaderModule createShaderModule(const std::vector<char>& code);

    // function for choosing the format to use from available formats
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    // function to choose a prefere presentation mode
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    // function to choose a good width and height for images
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    // populate swap chain struct
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    // check a physical device to see if it is suitable
    bool isDeviceSuitable(VkPhysicalDevice device);

    // check that our device has support for the set of extensions we are interested in
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    // search for queue family support
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    // return required list of extensions
    std::vector<const char*> getRequiredExtensions();

    // check whether all requested layers are available. return true iff they are
    bool checkValidationLayerSupport();

    // VKAPI_ATTR and VKAPI_CALL ensure the function is callable by vulkan

    // We pass in the message severity as VK_DEBUG_UTILS_MESSAGE_SEVERITY_{VERBOSE|INFO|WARNING|ERROR}_BIT_EXT
    // messageType can be VK_DEBUG_UTILS_MESSAGE_TYPE_{GENERAL|VALIDATION|PERFORMANCE}_BIT_EXT
    // pCallbackData contains data related to the message. Importantly: pMessage, pObjects, objectCount
    // pUserData can be used to pass custom data in

    // the function is used for taking control of debug output
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
        // output message
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            assert(false);
        }

        // unless testing validation layer itself, return VK_FALSE (0)
        return VK_FALSE;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallbackEXT(
        VkDebugReportFlagsEXT                       flags,
        VkDebugReportObjectTypeEXT                  objectType,
        uint64_t                                    object,
        size_t                                      location,
        int32_t                                     messageCode,
        const char* pLayerPrefix,
        const char* pMessage,
        void* pUserData) {
        // output message
        std::cerr << "validation layer: " << pMessage << std::endl;

        //if (messageCode >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        //{
        //    assert(false);
        //}

        // unless testing validation layer itself, return VK_FALSE (0)
        return VK_FALSE;
    }
};

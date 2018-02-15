#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include<iostream>
#include<fstream>
#include<stdexcept>
#include<functional>
#include<vector>
#include<assert.h>
#include <unordered_map>
#include<algorithm>
#include <chrono>
#include <array>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include"VDeleter.h"
#include"stdArrayUtility.h"


#define NTF_API_DUMP_VALIDATION_LAYER_ON 0
#ifdef NDEBUG
#define NTF_VALIDATION_LAYERS_ON 0
#define NTF_VK_ASSERT_SUCCESS(expr)
#else
#define NTF_VALIDATION_LAYERS_ON 1
#define NTF_VK_ASSERT_SUCCESS(expr) (assert(expr == VK_SUCCESS))
#endif//#ifdef NDEBUG


//these bools are static variables in case I want to make validation layers a runtime-settable property
#if NTF_VALIDATION_LAYERS_ON
const bool s_enableValidationLayers = true;
#else
const bool s_enableValidationLayers = false;
#endif//#if NTF_VALIDATION_LAYERS_ON


#define NTF_VALIDATION_LAYERS_BASE_SIZE 1
#if NTF_API_DUMP_VALIDATION_LAYER_ON
#define NTF_VALIDATION_LAYERS_SIZE (NTF_VALIDATION_LAYERS_BASE_SIZE + 1)
#else
#define NTF_VALIDATION_LAYERS_SIZE (NTF_VALIDATION_LAYERS_BASE_SIZE)
#endif//NTF_API_DUMP_VALIDATION_LAYER_ON


#define NTF_DEVICE_EXTENSIONS_NUM 1

static const uint32_t s_kWidth = 800;
static const uint32_t s_kHeight = 600;

const char*const sk_ModelPath = "models/chalet.obj";
const char*const sk_texturePath = "textures/chalet.jpg";

void CreateTextureImageView(VkImageView*const textureImageViewPtr, const VkImage& textureImage, const VkDevice& device);
void CopyBufferToImage(
    const VkBuffer& buffer,
    const VkImage& image,
    const uint32_t width,
    const uint32_t height,
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device);
void TransitionImageLayout(
    const VkImage& image,
    const VkFormat& format,
    const VkImageLayout& oldLayout,
    const VkImageLayout& newLayout,
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device);
void CreateImage(
    VkImage*const imagePtr,
    VkDeviceMemory*const imageMemoryPtr,
    const uint32_t width,
    const uint32_t height,
    const VkFormat& format,
    const VkImageTiling& tiling,
    const VkImageUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
void CopyBuffer(
    const VkBuffer& srcBuffer,
    const VkBuffer& dstBuffer,
    const VkDeviceSize& size,
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device);
uint32_t FindMemoryType(const uint32_t typeFilter, const VkMemoryPropertyFlags& properties, const VkPhysicalDevice& physicalDevice);
void CreateBuffer(
    VkBuffer*const bufferPtr,
    VkDeviceMemory*const bufferMemoryPtr,
    const VkDeviceSize& size,
    const VkBufferUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice
    );
VkFormat FindDepthFormat(const VkPhysicalDevice& physicalDevice);
void CreateShaderModule(VkShaderModule*const shaderModulePtr, const std::vector<char>& code, const VkDevice& device);
bool CheckValidationLayerSupport(const ArraySafe<const char*, NTF_VALIDATION_LAYERS_SIZE>& validationLayers);
void CreateImageView(VkImageView*const imageViewPtr, const VkDevice& device, const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectFlags);
std::vector<char> ReadFile(const char*const filename);
VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char* layerPrefix,
    const char* msg,
    void* userData);
VkResult CreateDebugReportCallbackEXT(
    VkInstance instance,
    const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugReportCallbackEXT* pCallback);
void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);
void BeginSingleTimeCommands(VkCommandBuffer*const commandBufferPtr, const VkCommandPool& commandPool, const VkDevice& device);

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription GetBindingDescription();

    enum { kGetAttributeDescriptionsSize = 3 };
    static void GetAttributeDescriptions(ArraySafe<VkVertexInputAttributeDescription, kGetAttributeDescriptionsSize>* const attributeDescriptions);

    bool operator==(const Vertex& other) const;
};

namespace std
{
    template<> struct hash<Vertex>
    {
        size_t operator()(Vertex const& vertex) const;
    };
}

struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

template<size_t kRequiredExtensionsMaxNum>
void GetRequiredExtensions(ArraySafe<const char*, kRequiredExtensionsMaxNum>*const requiredExtensions);

VkInstance CreateInstance(const ArraySafe<const char*, NTF_VALIDATION_LAYERS_SIZE>& validationLayers);

VkDebugReportCallbackEXT SetupDebugCallback(const VkInstance& instance);

struct SwapChainSupportDetails
{
    enum { kItemsMax = 32 };
    VkSurfaceCapabilitiesKHR capabilities;
    ArraySafe<VkSurfaceFormatKHR, kItemsMax> formats;
    ArraySafe<VkPresentModeKHR, kItemsMax> presentModes;
};

void QuerySwapChainSupport(SwapChainSupportDetails*const swapChainSupportDetails, const VkSurfaceKHR& surface, const VkPhysicalDevice& device);

struct QueueFamilyIndices
{
    int graphicsFamily = -1;
    int presentFamily = -1;

    bool isComplete()
    {
        return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& device, const VkSurfaceKHR& surface);

template<size_t kItemsMax>
VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const ArraySafe<VkSurfaceFormatKHR, kItemsMax>& availableFormats);

template<size_t kItemsMax>
VkPresentModeKHR ChooseSwapPresentMode(const ArraySafe<VkPresentModeKHR, kItemsMax>& availablePresentModes);

VkExtent2D ChooseSwapExtent(GLFWwindow*const window, const VkSurfaceCapabilitiesKHR& capabilities);

template<size_t kSwapChainImagesNumMax>
void CreateSwapChain(
    GLFWwindow*const window,
    VkSwapchainKHR*const swapChainPtr,
    ArraySafe<VkImage, kSwapChainImagesNumMax>*const swapChainImagesPtr,
    VkFormat*const swapChainImageFormatPtr,
    VkExtent2D*const swapChainExtentPtr,
    const VkPhysicalDevice& physicalDevice,
    const VkSurfaceKHR& surface,
    const VkDevice& device);

template<size_t kSwapChainImagesNumMax>
void CleanupSwapChain(
    ArraySafe<VkCommandBuffer, kSwapChainImagesNumMax>*const commandBuffersPtr,
    const VkDevice& device,
    const VkImageView& depthImageView,
    const VkImage& depthImage,
    const VkDeviceMemory& depthImageMemory,
    const ArraySafe<VkFramebuffer, kSwapChainImagesNumMax>& swapChainFramebuffers,
    const VkCommandPool& commandPool,
    const VkPipeline& graphicsPipeline,
    const VkPipelineLayout& pipelineLayout,
    const VkRenderPass& renderPass,
    const ArraySafe<VkImageView, kSwapChainImagesNumMax>& swapChainImageViews,
    const VkSwapchainKHR& swapChain);

template<size_t kSwapChainImagesNumMax>
void CreateImageViews(
    ArraySafe<VkImageView, kSwapChainImagesNumMax>*const swapChainImageViewsPtr,
    const ArraySafe<VkImage, kSwapChainImagesNumMax>& swapChainImages,
    const VkFormat& swapChainImageFormat,
    const VkDevice& device);

bool CheckDeviceExtensionSupport(const VkPhysicalDevice& physicalDevice, const ArraySafe<const char*, NTF_DEVICE_EXTENSIONS_NUM>& deviceExtensions);

bool IsDeviceSuitable(
    const VkPhysicalDevice& physicalDevice,
    const VkSurfaceKHR& surface,
    const ArraySafe<const char*, NTF_DEVICE_EXTENSIONS_NUM>& deviceExtensions);

bool PickPhysicalDevice(
    VkPhysicalDevice*const physicalDevicePtr,
    const VkSurfaceKHR& surface,
    const ArraySafe<const char*, NTF_DEVICE_EXTENSIONS_NUM>& deviceExtensions,
    const VkInstance& instance);

void CreateLogicalDevice(
    VkDevice*const devicePtr,
    VkQueue*const graphicsQueuePtr,
    VkQueue*const presentQueuePtr,
    const ArraySafe<const char*, NTF_DEVICE_EXTENSIONS_NUM>& deviceExtensions,
    const ArraySafe<const char*, NTF_VALIDATION_LAYERS_SIZE>& validationLayers,
    const VkSurfaceKHR& surface,
    const VkPhysicalDevice& physicalDevice);

void CreateDescriptorSetLayout(VkDescriptorSetLayout*const descriptorSetLayoutPtr, const VkDevice& device);
void CreateGraphicsPipeline(
    VkPipelineLayout*const pipelineLayoutPtr,
    VkPipeline*const graphicsPipelinePtr,
    const VkRenderPass& renderPass,
    const VkDescriptorSetLayout& descriptorSetLayout,
    const VkExtent2D& swapChainExtent,
    const VkDevice& device);
void CreateRenderPass(
    VkRenderPass*const renderPassPtr,
    const VkFormat& swapChainImageFormat,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice
    );

template<size_t kSwapChainImagesNumMax>
void CreateCommandBuffers(
    ArraySafe<VkCommandBuffer, kSwapChainImagesNumMax>*const commandBuffersPtr,
    const VkCommandPool& commandPool,
    const VkDescriptorSet& descriptorSet,
    const ArraySafe<VkFramebuffer, kSwapChainImagesNumMax>& swapChainFramebuffers,
    const VkRenderPass& renderPass,
    const VkExtent2D& swapChainExtent,
    const VkPipelineLayout& pipelineLayout,
    const VkPipeline& graphicsPipeline,
    const VkBuffer& vertexBuffer,
    const VkBuffer& indexBuffer,
    const uint32_t& indicesNum,
    const VkDevice& device);

void CreateUniformBuffer(
    VkBuffer*const uniformBufferPtr,
    VkDeviceMemory*const uniformBufferMemoryPtr,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
void CreateDescriptorPool(VkDescriptorPool*const descriptorPoolPtr, const VkDevice& device);

void CreateDescriptorSet(
    VkDescriptorSet*const descriptorSetPtr,
    const VkDescriptorSetLayout& descriptorSetLayout,
    const VkDescriptorPool& descriptorPool,
    const VkBuffer& uniformBuffer,
    const VkImageView& textureImageView,
    const VkSampler& textureSampler,
    const VkDevice& device);

void LoadModel(std::vector<Vertex>*const verticesPtr, std::vector<uint32_t>*const indicesPtr);

void CreateVertexBuffer(
    VkBuffer*const vertexBufferPtr,
    VkDeviceMemory*const vertexBufferMemoryPtr,
    const std::vector<Vertex>& vertices,
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice
    );
void CreateIndexBuffer(
    VkBuffer*const indexBufferPtr,
    VkDeviceMemory*const indexBufferMemoryPtr,
    const std::vector<uint32_t>& indices,
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice
    );
void EndSingleTimeCommands(const VkCommandBuffer& commandBuffer, const VkCommandPool commandPool, const VkQueue& graphicsQueue, const VkDevice& device);
void CreateCommandPool(VkCommandPool*const commandPoolPtr, const VkSurfaceKHR& surface, const VkDevice& device, const VkPhysicalDevice& physicalDevice);
void CreateDepthResources(
    VkImage*const depthImagePtr,
    VkDeviceMemory*const depthImageMemoryPtr,
    VkImageView*const depthImageViewPtr,
    const VkExtent2D& swapChainExtent,
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);

template<size_t kCandidatesMaxSize>
VkFormat FindSupportedFormat(
    const VkPhysicalDevice& physicalDevice,
    const ArraySafe<VkFormat, kCandidatesMaxSize>& candidates,
    const VkImageTiling& tiling,
    const VkFormatFeatureFlags& features);

bool HasStencilComponent(VkFormat format);
/* @todo:   All of the helper functions that submit commands so far have been set up to execute synchronously by
waiting for the queue to become idle. For practical applications it is recommended to combine these
operations in a single command buffer and execute them asynchronously for higher throughput, especially
the transitions and copy in the CreateTextureImage function. Try to experiment with this by creating a
setupCommandBuffer that the helper functions record commands into, and add a flushSetupCommands to
execute the commands that have been recorded so far.*/
void CreateTextureImage(
    VkImage*const textureImagePtr,
    VkDeviceMemory*const textureImageMemoryPtr,
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
void CreateTextureSampler(VkSampler*const textureSamplerPtr, const VkDevice& device);

template<size_t kSwapChainImagesNumMax>
void CreateFramebuffers(
    ArraySafe<VkFramebuffer, kSwapChainImagesNumMax>*const swapChainFramebuffersPtr,
    const ArraySafe<VkImageView, kSwapChainImagesNumMax>& swapChainImageViews,
    const VkRenderPass& renderPass,
    const VkExtent2D& swapChainExtent,
    const VkImageView& depthImageView,
    const VkDevice& device);
void CreateSurface(VkSurfaceKHR*const surfacePtr, GLFWwindow*const window, const VkInstance& instance);
void CreateSemaphores(VkSemaphore*const imageAvailablePtr, VkSemaphore*const renderFinishedPtr, const VkDevice& device);

void UpdateUniformBuffer(const VkDeviceMemory& uniformBufferMemory, const VkExtent2D& swapChainExtent, const VkDevice& device);

template<size_t kSwapChainImagesNumMax>
void DrawFrame(
    //HelloTriangleApplication*const hackToRecreateSwapChainIfNecessaryPtr,
    const VkSwapchainKHR& swapChain,
    const ArraySafe<VkCommandBuffer, kSwapChainImagesNumMax>& commandBuffers,
    const VkQueue& graphicsQueue,
    const VkQueue& presentQueue,
    const VkSemaphore& imageAvailableSemaphore,
    const VkSemaphore& renderFinishedSemaphore,
    const VkDevice& device);
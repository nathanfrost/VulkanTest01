#pragma once

#include "WinTimer.h"//has to be #included before glfw*.h

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

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

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

typedef uint32_t PushConstantBindIndexType;

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
bool CheckValidationLayerSupport(ConstVectorSafeRef<const char*> validationLayers);
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
    static void GetAttributeDescriptions(VectorSafeRef<VkVertexInputAttributeDescription> attributeDescriptions);

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
    glm::mat4 modelToClip;
};

void GetRequiredExtensions(VectorSafeRef<const char*>*const requiredExtensions);

VkInstance CreateInstance(ConstVectorSafeRef<const char*> validationLayers);

VkDebugReportCallbackEXT SetupDebugCallback(const VkInstance& instance);

struct SwapChainSupportDetails
{
    enum { kItemsMax = 32 };
    VkSurfaceCapabilitiesKHR capabilities;
    VectorSafe<VkSurfaceFormatKHR, kItemsMax> formats;
    VectorSafe<VkPresentModeKHR, kItemsMax> presentModes;
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

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(ConstVectorSafeRef<VkSurfaceFormatKHR> availableFormats);

VkPresentModeKHR ChooseSwapPresentMode(const VectorSafeRef<VkPresentModeKHR>& availablePresentModes);

VkExtent2D ChooseSwapExtent(GLFWwindow*const window, const VkSurfaceCapabilitiesKHR& capabilities);

void CreateSwapChain(
    GLFWwindow*const window,
    VkSwapchainKHR*const swapChainPtr,
    VectorSafeRef<VkImage> swapChainImages,
    VkFormat*const swapChainImageFormatPtr,
    VkExtent2D*const swapChainExtentPtr,
    const VkPhysicalDevice& physicalDevice,
    const uint32_t framesNum,
    const VkSurfaceKHR& surface,
    const VkDevice& device);

void CleanupSwapChain(
    VectorSafeRef<VkCommandBuffer> commandBuffersPrimary,
    VectorSafeRef<VkCommandBuffer> commandBuffersSecondary,
    const VkDevice& device,
    const VkImageView& depthImageView,
    const VkImage& depthImage,
    const VkDeviceMemory& depthImageMemory,
    ConstVectorSafeRef<VkFramebuffer> swapChainFramebuffers,
    const VkCommandPool& commandPool,
    const VkPipeline& graphicsPipeline,
    const VkPipelineLayout& pipelineLayout,
    const VkRenderPass& renderPass,
    ConstVectorSafeRef<VkImageView> swapChainImageViews,
    const VkSwapchainKHR& swapChain);

void CreateImageViews(
    VectorSafeRef<VkImageView> swapChainImageViewsPtr,
    ConstVectorSafeRef<VkImage> swapChainImages,
    const VkFormat& swapChainImageFormat,
    const VkDevice& device);

bool CheckDeviceExtensionSupport(const VkPhysicalDevice& physicalDevice, ConstVectorSafeRef<const char*> deviceExtensions);

bool IsDeviceSuitable(
    const VkPhysicalDevice& physicalDevice,
    const VkSurfaceKHR& surface,
    ConstVectorSafeRef<const char*> deviceExtensions);

bool PickPhysicalDevice(
    VkPhysicalDevice*const physicalDevicePtr,
    const VkSurfaceKHR& surface,
    ConstVectorSafeRef<const char*> deviceExtensions,
    const VkInstance& instance);

void CreateLogicalDevice(
    VkDevice*const devicePtr,
    VkQueue*const graphicsQueuePtr,
    VkQueue*const presentQueuePtr,
    ConstVectorSafeRef<const char*> deviceExtensions,
    ConstVectorSafeRef<const char*> validationLayers,
    const VkSurfaceKHR& surface,
    const VkPhysicalDevice& physicalDevice);

void CreateDescriptorSetLayout(VkDescriptorSetLayout*const descriptorSetLayoutPtr, const VkDescriptorType descriptorType, const VkDevice& device);
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

void AllocateCommandBuffers(
    VectorSafeRef<VkCommandBuffer> commandBuffers,
    const VkCommandPool& commandPool,
    const VkCommandBufferLevel& commandBufferLevel,
    const int commandBuffersNum,
    const VkDevice& device);

void AcquireNextImage(
    uint32_t*const acquiredImageIndexPtr,
    const VkSwapchainKHR& swapChain,
    const VkSemaphore& imageAvailableSemaphore,
    const VkDevice& device);

void FillCommandBuffer(
    const VkCommandBuffer& commandBufferPrimary,
    const VkCommandBuffer& commandBufferSecondary,
    const VkDescriptorSet& descriptorSet,
    const VkDeviceSize& uniformBufferElementSize,
    const size_t objectNum,
    const VkFramebuffer& swapChainFramebuffers,
    const VkRenderPass& renderPass,
    const VkExtent2D& swapChainExtent,
    const VkPipelineLayout& pipelineLayout,
    const VkPipeline& graphicsPipeline,
    const VkBuffer& vertexBuffer,
    const VkBuffer& indexBuffer,
    const uint32_t& indicesNum,
    const VkDevice& device);

VkDeviceSize UniformBufferCpuAlignmentCalculate(const VkDeviceSize bufferSize, const VkPhysicalDevice& physicalDevice);
void CreateUniformBuffer(
    ArraySafeRef<uint8_t>*const uniformBufferCpuMemoryPtr,
    VkDeviceMemory*const uniformBufferGpuMemoryPtr,
    VkBuffer*const uniformBufferPtr,
    const VkDeviceSize bufferSize,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
void DestroyUniformBuffer(
    ArraySafeRef<uint8_t> uniformBufferCpuMemory,
    const VkDeviceMemory uniformBufferGpuMemory,
    const VkBuffer uniformBuffer,
    const VkDevice& device);
void CreateDescriptorPool(VkDescriptorPool*const descriptorPoolPtr, const VkDescriptorType descriptorType, const VkDevice& device);

void CreateDescriptorSet(
    VkDescriptorSet*const descriptorSetPtr,
    const VkDescriptorType descriptorType,
    const VkDescriptorSetLayout& descriptorSetLayout,
    const VkDescriptorPool& descriptorPool,
    const VkBuffer& uniformBuffer,
    const size_t uniformBufferSize,
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

VkFormat FindSupportedFormat(
    const VkPhysicalDevice& physicalDevice,
    ConstVectorSafeRef<VkFormat> candidates,
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

void CreateFramebuffers(
    VectorSafeRef<VkFramebuffer> swapChainFramebuffersPtr,
    ConstVectorSafeRef<VkImageView> swapChainImageViews,
    const VkRenderPass& renderPass,
    const VkExtent2D& swapChainExtent,
    const VkImageView& depthImageView,
    const VkDevice& device);
void CreateSurface(VkSurfaceKHR*const surfacePtr, GLFWwindow*const window, const VkInstance& instance);
void CreateFrameSyncPrimitives(
    VectorSafeRef<VkSemaphore> imageAvailable,
    VectorSafeRef<VkSemaphore> renderFinished,
    VectorSafeRef<VkFence> fence,
    const size_t framesNum,
    const VkDevice& device);

void UpdateUniformBuffer(
    ArraySafeRef<uint8_t> uniformBufferCpuMemory,
    const VkDeviceMemory& uniformBufferGpuMemory, 
    const size_t objectNum,
    const VkDeviceSize uniformBufferSize,
    const VkExtent2D& swapChainExtent, 
    const VkDevice& device);

void DrawFrame(
    //VulkanRendererNTF*const hackToRecreateSwapChainIfNecessaryPtr,///#TODO_CALLBACK: clean this up with a proper callback
    const VkSwapchainKHR& swapChain,
    ConstVectorSafeRef<VkCommandBuffer> commandBuffers,
    const uint32_t acquiredImageIndex,
    const VkQueue& graphicsQueue,
    const VkQueue& presentQueue,
    const VkFence& fence,
    const VkSemaphore& imageAvailableSemaphore,
    const VkSemaphore& renderFinishedSemaphore,
    const VkDevice& device);
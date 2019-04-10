#pragma once

#include "WinTimer.h"//has to be #included before glfw*.h

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

///@todo: alphabetize and place immediately below WinTimer.h
#include<algorithm>
#include<assert.h>
#include<chrono>
#include<fstream>
#include<functional>
#include<iostream>
#include<stdexcept>
#include<thread>
#include<unordered_map>
#include<vector>

#include"StackNTF.h"
#include"StreamingUnit.h"

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
#define NTF_VK_SUCCESS(expr) (expr == VK_SUCCESS ? true : false)


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

extern StackCpu* g_stbAllocator;
class VulkanPagedStackAllocator;

static const uint32_t s_kWidth = 1600;
static const uint32_t s_kHeight = 1200;

typedef uint32_t PushConstantBindIndexType;


VkAllocationCallbacks* GetVulkanAllocationCallbacks();
#if NTF_DEBUG
size_t GetVulkanApiCpuBytesAllocatedMax();
#endif//#if NTF_DEBUG
HANDLE MutexCreate();
void MutexRelease(const HANDLE mutex);
HANDLE ThreadSignalingEventCreate();
BOOL HandleCloseWindows(HANDLE*const h);
void SignalSemaphoreWindows(const HANDLE wakeEventHandle);
void WaitForSignalWindows(const HANDLE doneEventHandle);
void TransferImageFromCpuToGpu(
    const VkImage& image,
    const uint32_t width,
    const uint32_t height,
    const VkFormat& format,
    const VkBuffer& stagingBuffer,
    const VkCommandBuffer commandBufferTransfer,
    const VkQueue& transferQueue,
    const uint32_t transferQueueFamilyIndex,
    const VkCommandBuffer commandBufferGraphics,
    const VkQueue& graphicsQueue,
    const uint32_t graphicsQueueFamilyIndex,
    const VkDevice& device);
void CreateTextureImageView(VkImageView*const textureImageViewPtr, const VkImage& textureImage, const VkDevice& device);
bool CreateAllocateBindImageIfAllocatorHasSpace(
    VkImage*const imagePtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    VkDeviceSize*const alignmentPtr,
    const uint32_t width,
    const uint32_t height,
    const VkFormat& format,
    const VkImageTiling& tiling,
    const VkImageUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const bool residentForever,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
void CreateBuffer(
    VkBuffer*const vkBufferPtr,
    const VkDeviceMemory& vkBufferMemory,
    const VkDeviceSize& offsetToAllocatedBlock,
    const VkDeviceSize& vkBufferSizeBytes,
    const VkMemoryPropertyFlags& flags,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
void CopyBufferToImage(
    const VkBuffer& buffer,
    const VkImage& image,
    const uint32_t width,
    const uint32_t height,
    const VkCommandBuffer& commandBuffer,
    const VkDevice& device);
VkResult SubmitCommandBuffer(
    ConstVectorSafeRef<VkSemaphore> signalSemaphores,
    ConstVectorSafeRef<VkSemaphore> waitSemaphores,
    ArraySafeRef<VkPipelineStageFlags> waitStages,///<@todo: ConstArraySafeRef
    const VkCommandBuffer& commandBuffer,
    const VkQueue& queue,
    const VkFence& fence);
uint32_t FindMemoryType(const uint32_t typeFilter, const VkMemoryPropertyFlags& properties, const VkPhysicalDevice& physicalDevice);
uint32_t FindMemoryHeapIndex(const VkMemoryPropertyFlags& properties, const VkPhysicalDevice& physicalDevice);
void CreateBuffer(
    VkBuffer*const bufferPtr,
    VkDeviceMemory*const bufferMemoryPtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    VkDeviceSize*const offsetToAllocatedBlockPtr,
    const VkDeviceSize& size,
    const VkBufferUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const bool residentForever,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
VkFormat FindDepthFormat(const VkPhysicalDevice& physicalDevice);
void CreateShaderModule(VkShaderModule*const shaderModulePtr, char*const code, const size_t codeSizeBytes, const VkDevice& device);
bool CheckValidationLayerSupport(ConstVectorSafeRef<const char*> validationLayers);
void CreateImageView(VkImageView*const imageViewPtr, const VkDevice& device, const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectFlags);
void ReadFile(char**const fileData, StackCpu*const allocatorPtr, size_t*const fileSizeBytesPtr, const char*const filename);
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
void BeginCommands(const VkCommandBuffer& commandBuffer, const VkDevice& device);

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription GetBindingDescription();
    static void GetAttributeDescriptions(VectorSafeRef<VkVertexInputAttributeDescription> attributeDescriptions);

    bool operator==(const Vertex& other) const;
};

struct TexturedGeometry
{
    std::vector<Vertex> vertices;///<@todo: #StreamingMemory: eliminate std::vector
    std::vector<uint32_t> indices;///<@todo: #StreamingMemory: eliminate std::vector
    uint32_t indicesSize;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkImage textureImage;
    VkDeviceMemory textureBufferMemory;

    bool Valid() const
    {
        return  indices.size() == indicesSize &&
                indicesSize > 0 &&
                vertices.size() > 0;
    }
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

struct ThreadHandles
{
    HANDLE threadHandle;
    HANDLE wakeEventHandle;
    HANDLE doneEventHandle;
};

class StreamingUnitRuntime
{
public:
    VkSampler m_textureSampler;
#define TODO_REFACTOR_NUM 2//is NTF_OBJECTS_NUM -- todo: generalize #StreamingMemory
    ArraySafe<TexturedGeometry, TODO_REFACTOR_NUM> m_texturedGeometries;
    ArraySafe<VkImageView, TODO_REFACTOR_NUM> m_textureImageViews;

    /*@todo NTF: 
        1. Make StreamingUnitRuntime be entirely allocated from a StackNTF by methods like AddTexturePaths(const char*const* texturePaths, const size_t texturePathsNum) so that it can contain a variable amount of everything up to the stack limit.  Use ArraySafe<>'s to index each container subset within the bytestream
        2. Pull texture and model loading code into cooking module that accepts StreamingUnitOld ("StreamingUnitTemplate") and spits out StreamingUnitNew, which is still allocated from a StackNTF and uses ArraySafe<>'s but contains the ready-to-pass-to-Vulkan model and texture data as well as the other variables
    */
    const char*const m_modelPaths[TODO_REFACTOR_NUM] = { "models/skull.obj", "models/Banana.obj" /*"models/Orange.obj"*/, /*"models/Container_OBJ.obj",*/ /*"models/apple textured obj.obj"*//*"models/cat.obj"*//*,"models/chalet.obj"*/ };//#StreamingMemory
    const float m_uniformScales[TODO_REFACTOR_NUM] = { .05f, .005f, /*0.5f,*//*,.0025f*//*.01f,*/ /*1.f*/ };//#StreamingMemory

    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorSet m_descriptorSet;//automatically freed when the VkDescriptorPool is destroyed   

    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformBufferGpuMemory;
    VkDeviceSize m_uniformBufferOffsetToGpuMemory;
    ArraySafeRef<uint8_t> m_uniformBufferCpuMemory;

    VkDeviceSize m_uniformBufferSizeAligned;//single uniform buffer that contains all uniform information for this streaming unit
    size_t m_uniformBufferSizeUnaligned;///<@todo: only exists to generate the same value but aligned
    
    VkPipelineLayout m_pipelineLayout;

    void Free(const VkDevice device);
};

struct QueueFamilyIndices;
class AssetLoadingArguments
{
public:
    VulkanPagedStackAllocator* m_deviceLocalMemory;
    VkPipeline* m_graphicsPipeline;
    StreamingUnitRuntime* m_streamingUnit;
    enum ThreadCommand {kFirstValidArgument, kLoadStreamingUnit=kFirstValidArgument, 
                        kLastValidArgument, kCleanupAndTerminate=kLastValidArgument} *m_threadCommand;

    const VkCommandBuffer* m_commandBufferTransfer;
    const VkCommandBuffer* m_commandBufferTransitionImage;
    const VkDevice* m_device;
    const VkQueue* m_graphicsQueue;
    const VkPhysicalDevice* m_physicalDevice;
    const QueueFamilyIndices* m_queueFamilyIndices;
    const VkRenderPass* m_renderPass;
    const char* m_streamingUnitFilenameNoExtension;
    const VkExtent2D* m_swapChainExtent;
    const HANDLE* m_threadDone;
    const HANDLE* m_threadWake;
    const VkQueue* m_transferQueue;

    inline void AssertValid()
    {
        assert(m_deviceLocalMemory);
        assert(m_graphicsPipeline);
        assert(m_streamingUnit);
        assert(m_threadCommand && *m_threadCommand >= kFirstValidArgument && *m_threadCommand <= kLastValidArgument);

        assert(m_commandBufferTransfer);
        assert(m_commandBufferTransitionImage);
        assert(m_device);
        assert(m_graphicsQueue);
        assert(m_physicalDevice);
        assert(m_queueFamilyIndices);
        assert(m_renderPass);
        assert(m_streamingUnitFilenameNoExtension);
        assert(strlen(m_streamingUnitFilenameNoExtension) > 0);
        assert(m_swapChainExtent);
        assert(m_threadDone);
        assert(m_threadWake);
        assert(m_transferQueue);
    }
};
struct AssetLoadingThreadData
{
    ThreadHandles m_handles;
    AssetLoadingArguments::ThreadCommand m_threadCommand;
};

struct CommandBufferThreadArgumentsTest
{
    VkCommandBuffer* commandBuffer;
    VkDescriptorSet* descriptorSet;
    VkRenderPass* renderPass;
    VkExtent2D* swapChainExtent;
    VkPipelineLayout* pipelineLayout;
    VkBuffer* vertexBuffer;
    VkBuffer* indexBuffer;
    VkFramebuffer* swapChainFramebuffer;
    VkPipeline* graphicsPipeline;
    uint32_t* objectIndex;
    uint32_t* indicesNum;
    HANDLE* commandBufferThreadDone;
    HANDLE* commandBufferThreadWake;
};

DWORD WINAPI CommandBufferThreadTest(void* arg);
DWORD WINAPI AssetLoadingThread(void* arg);

HANDLE CreateThreadWindows(LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter);

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
    int transferFamily = -1;

    bool IsComplete()
    {
        return graphicsFamily >= 0 && presentFamily >= 0 && transferFamily >= 0;
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
    const VkDevice& device,
    const VkImageView& depthImageView,
    const VkImage& depthImage,
    ConstVectorSafeRef<VkFramebuffer> swapChainFramebuffers,
    const VkCommandPool& commandPool,
    ConstVectorSafeRef<ArraySafe<VkCommandPool, 2>> commandPoolsSecondary,///<@todo NTF: refactor out magic number 2 (meant to be NTF_OBJECTS_NUM) and either support VectorSafeRef<ArraySafeRef<T>> or repeatedly call FreeCommandBuffers on each VectorSafe outside of this function
    const VkPipeline& graphicsPipeline,
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
    VkQueue*const transferQueuePtr,
    ConstVectorSafeRef<const char*> deviceExtensions,
    ConstVectorSafeRef<const char*> validationLayers,
    const VkSurfaceKHR& surface,
    const VkPhysicalDevice& physicalDevice);

void CreateDescriptorSetLayout(
    VkDescriptorSetLayout*const descriptorSetLayoutPtr,
    const VkDescriptorType descriptorType,
    const VkDevice& device,
    const uint32_t objectsNum);
void CreateGraphicsPipeline(
    VkPipelineLayout*const pipelineLayoutPtr,
    VkPipeline*const graphicsPipelinePtr,
    StackCpu*const allocatorPtr,
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
    ArraySafeRef<VkCommandBuffer> commandBuffers,
    const VkCommandPool& commandPool,
    const VkCommandBufferLevel& commandBufferLevel,
    const uint32_t commandBuffersNum,
    const VkDevice& device);

void AcquireNextImage(
    uint32_t*const acquiredImageIndexPtr,
    const VkSwapchainKHR& swapChain,
    const VkSemaphore& imageAvailableSemaphore,
    const VkDevice& device);

void FillSecondaryCommandBuffersTest(
    ArraySafeRef<VkCommandBuffer> commandBuffersSecondary,
    ArraySafeRef<ThreadHandles> commandBuffersSecondaryThreads,
    ArraySafeRef<HANDLE> commandBufferThreadDoneEvents,
    ArraySafeRef<CommandBufferThreadArgumentsTest> commandBufferThreadArgumentsArray,
    VkDescriptorSet*const descriptorSet,
    VkFramebuffer*const swapChainFramebuffer,
    VkRenderPass*const renderPass,
    VkExtent2D*const swapChainExtent,
    VkPipelineLayout*const pipelineLayout,
    VkPipeline*const graphicsPipeline,
    VkBuffer*const vertexBuffer,
    VkBuffer*const indexBuffer,
    uint32_t*const indicesSize,
    ArraySafeRef<uint32_t> objectIndex,
    const size_t objectsNum);

void FillCommandBufferPrimary(
    const VkCommandBuffer& commandBufferPrimary,
    const ArraySafeRef<TexturedGeometry> texturedGeometries,
    const VkDescriptorSet descriptorSet,
    const size_t objectNum,
    const size_t drawCallsPerObjectNum,
    const VkFramebuffer& swapChainFramebuffer,
    const VkRenderPass& renderPass,
    const VkExtent2D& swapChainExtent,
    const VkPipelineLayout& pipelineLayout,
    const VkPipeline& graphicsPipeline,
    const VkDevice& device);

VkDeviceSize UniformBufferCpuAlignmentCalculate(const VkDeviceSize bufferSize, const VkPhysicalDevice& physicalDevice);
void CreateUniformBuffer(
    ArraySafeRef<uint8_t>*const uniformBufferCpuMemoryPtr,
    VkDeviceMemory*const uniformBufferGpuMemoryPtr,
    VkBuffer*const uniformBufferPtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    VkDeviceSize*const offsetToGpuMemoryPtr,
    const VkDeviceSize bufferSize,
    const bool residentForever,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
void DestroyUniformBuffer(
    ArraySafeRef<uint8_t> uniformBufferCpuMemory,
    const VkDeviceMemory uniformBufferGpuMemory,
    const VkBuffer uniformBuffer,
    const VkDevice& device);
void CreateDescriptorPool(
    VkDescriptorPool*const descriptorPoolPtr,
    const VkDescriptorType descriptorType,
    const VkDevice& device,
    const uint32_t texturesNum);

void CreateDescriptorSet(
    VkDescriptorSet*const descriptorSetPtr,
    const VkDescriptorType descriptorType,
    const VkDescriptorSetLayout& descriptorSetLayout,
    const VkDescriptorPool& descriptorPool,
    const VkBuffer& uniformBuffer,
    const VkDeviceSize uniformBufferSize,
    const ArraySafeRef<VkImageView> textureImageViews,
    const size_t texturesNum,
    const VkSampler textureSampler,
    const VkDevice& device);

void LoadModel(std::vector<Vertex>*const verticesPtr, std::vector<uint32_t>*const indicesPtr, const char*const modelPath, const float uniformScale);

void CopyBufferToGpuPrepare(
    VulkanPagedStackAllocator*const deviceLocalMemoryPtr,
    VkBuffer*const gpuBufferPtr,
    VkDeviceMemory*const gpuBufferMemoryPtr,
    ArraySafeRef<VkBuffer> stagingBuffersGpu,
    size_t*const stagingBuffersGpuIndexPtr,
    StackCpu*const stagingBufferMemoryMapCpuToGpuPtr,
    StackNTF<VkDeviceSize>*const stagingBufferGpuStackPtr,
    const VkDeviceMemory stagingBufferGpuMemory,
    const VkDeviceSize stagingBufferGpuAlignmentStandard,
    const VkDeviceSize offsetToFirstByteOfStagingBuffer,
    const void*const cpuBufferSource,
    const VkDeviceSize bufferSize,
    const VkMemoryPropertyFlags& memoryPropertyFlags,
    const bool residentForever,
    const VkCommandBuffer& commandBuffer,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
void CreateAndCopyToGpuBuffer(
    VulkanPagedStackAllocator*const allocatorPtr,
    VkBuffer*const gpuBufferPtr,
    VkDeviceMemory*const gpuBufferMemoryPtr,
    ArraySafeRef<uint8_t> stagingBufferMemoryMapCpuToGpu,
    const void*const cpuBufferSource,
    const VkBuffer& stagingBufferGpu,
    const VkDeviceSize bufferSize,
    const VkMemoryPropertyFlags& flags,
    const bool residentForever,
    const VkCommandBuffer& commandBuffer,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);

void EndSingleTimeCommandsHackDeleteSoon(const VkCommandBuffer& commandBuffer, const VkQueue& queue, const VkDevice& device);
void CreateCommandPool(VkCommandPool*const commandPoolPtr, const uint32_t& queueFamilyIndex, const VkDevice& device, const VkPhysicalDevice& physicalDevice);
void CreateDepthResources(
    VkImage*const depthImagePtr,
    VkImageView*const depthImageViewPtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    const VkExtent2D& swapChainExtent,
    const VkCommandBuffer& commandBuffer,
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
bool CreateImageAndCopyPixelsIfStagingBufferHasSpace(
    VkImage*const imagePtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    VkDeviceSize*const alignmentPtr,
    StreamingUnitTextureDimension*const textureWidthPtr,
    StreamingUnitTextureDimension*const textureHeightPtr,
    StackCpu*const stagingBufferMemoryMapCpuToGpuStackPtr,
    size_t*const imageSizeBytesPtr,
    FILE*const streamingUnitFile,
    VectorSafeRef<uint8_t> pixelBufferScratch,
    const VkFormat& format,
    const VkImageTiling& tiling,
    const VkImageUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const bool residentForever,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
void CreateTextureImage(
    VkImage*const textureImagePtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    const uint32_t widthPixels,
    const uint32_t heightPixels,
    const VkBuffer& stagingBufferGpu,
    const bool residentForever,
    const VkQueue& transferQueue,
    const VkCommandBuffer& commandBufferTransfer,
    const uint32_t transferQueueFamilyIndex,
    const VkSemaphore transferFinishedSemaphore,
    const VkQueue& graphicsQueue,
    const VkCommandBuffer& commandBufferGraphics,
    const uint32_t graphicsQueueFamilyIndex,
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
void CreateVulkanSemaphore(VkSemaphore*const semaphorePtr, const VkDevice& device);
void CreateFrameSyncPrimitives(
    VectorSafeRef<VkSemaphore> imageAvailable,
    VectorSafeRef<VkSemaphore> renderFinished,
    VectorSafeRef<VkFence> fence,
    const size_t framesNum,
    const VkDevice& device);

void FenceCreate(VkFence*const fencePtr, const VkFenceCreateFlagBits flags, const VkDevice& device);
void FenceWaitUntilSignalled(const VkFence& fence, const VkDevice& device);
void FenceReset(const VkFence& fence, const VkDevice& device);

void UpdateUniformBuffer(
    ArraySafeRef<uint8_t> uniformBufferCpuMemory,
    const glm::vec3 cameraTranslation,
    const VkDeviceMemory& uniformBufferGpuMemory,
    const VkDeviceSize& offsetToGpuMemory,
    const size_t drawCallsNum,
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

void CommandBufferSecondaryThreadsCreateTest(
    ArraySafeRef<ThreadHandles> threadData,
    ArraySafeRef<HANDLE> threadDoneEvents,
    ArraySafeRef<CommandBufferThreadArgumentsTest> threadArguments,
    const size_t threadsNum);

class VulkanMemoryHeapPage
{
public:
    ///@todo: all explicit default C++ functions except default constructor

    bool Allocate(const VkDeviceSize memoryMaxBytes, const uint32_t memoryTypeIndex, const VkDevice& device);
    inline void Free(const VkDevice& device)
    {
        m_stack.Free();
        vkFreeMemory(device, m_memoryHandle, GetVulkanAllocationCallbacks());
    }

    inline bool SufficientMemory(const VkMemoryRequirements& memRequirements) const
    {
        assert(m_stack.Allocated());
        VkDeviceSize dummy0, dummy1;
        return PushAlloc(&dummy0, &dummy1, memRequirements);
    }
    bool PushAlloc(VkDeviceSize* memoryOffsetPtr, const VkMemoryRequirements& memRequirements);
    inline VkDeviceMemory GetMemoryHandle() const 
    { 
        assert(m_stack.Allocated()); 
        return m_memoryHandle; 
    }
    inline bool Allocated() const { return m_stack.Allocated(); }

    VulkanMemoryHeapPage* m_next;
private:
    StackNTF<VkDeviceSize> m_stack;
    VkDeviceMemory m_memoryHandle;  ///<to its Vulkan allocation

    bool PushAlloc(
        VkDeviceSize*const firstByteFreePtr,
        VkDeviceSize*const firstByteReturnedPtr,
        const VkMemoryRequirements& memRequirements) const;
};

///@todo: unit test
class VulkanMemoryHeap
{
public:
    VulkanMemoryHeap()
    {
#if NTF_DEBUG
        m_initialized = false;
#endif//#if NTF_DEBUG
    };
    void Initialize(const uint32_t memoryTypeIndex, const VkDeviceSize memoryHeapPageSizeBytes);
    ///@todo: all explicit default C++ functions

    void Destroy(const VkDevice device);

    bool PushAlloc(
        VkDeviceSize* memoryOffsetPtr,
        VkDeviceMemory* memoryHandlePtr,
        const VkMemoryRequirements& memRequirements,
        const VkMemoryPropertyFlags& properties,
        const bool residentForever,
        const bool linearResource,
        const VkDevice& device,
        const VkPhysicalDevice& physicalDevice);

    inline uint32_t GetMemoryTypeIndex() const { return m_memoryTypeIndex; }

private:
#if NTF_DEBUG
    bool m_initialized;
#endif//#if NTF_DEBUG
    uint32_t m_memoryTypeIndex;
    VkDeviceSize m_pageSizeBytes;

    /** we are not concerned with VkPhysicalDeviceLimits::bufferImageGranularity, because linear and optimally accessed resources are suballocated 
        from different VkDeviceMemory objects, so there's no need to worry about bufferImageGranularity's often-egregious alignment requirements */
    VulkanMemoryHeapPage* m_pageAllocatedLinearFirst;
    VulkanMemoryHeapPage* m_pageAllocatedNonlinearFirst;
    VulkanMemoryHeapPage* m_pageFreeFirst;
    ArraySafe<VulkanMemoryHeapPage, 32> m_pagePool;

    VulkanMemoryHeapPage m_pageResidentForeverNonlinear, m_pageResidentForeverLinear;
};

///@todo: unit test
class VulkanPagedStackAllocator
{
public:
    VulkanPagedStackAllocator()
    {
        m_mutex = 0;
#if NTF_DEBUG
        m_initialized = false;
#endif//#if NTF_DEBUG
    }
    ///@todo: all explicit default C++ functions

    void Initialize(const VkDevice& device, const VkPhysicalDevice& physicalDevice);
    void Destroy(const VkDevice& device);

    bool PushAlloc(
        VkDeviceSize* memoryOffsetPtr,
        VkDeviceMemory* memoryHandlePtr,
        const VkMemoryRequirements& memRequirements,
        const VkMemoryPropertyFlags& properties,
        const bool residentForever,
        const bool linearResource,
        const VkDevice& device,
        const VkPhysicalDevice& physicalDevice);

    ///@todo: memreport function

private:
#if NTF_DEBUG
    bool m_initialized;
#endif//NTF_DEBUG
    VectorSafe<VulkanMemoryHeap, 32> m_vulkanMemoryHeaps;
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;
    HANDLE m_mutex;
};

void STBAllocatorCreate(StackCpu**const stbAllocatorPtrPtr);
void STBAllocatorDestroy(StackCpu**const stbAllocatorPtrPtr);
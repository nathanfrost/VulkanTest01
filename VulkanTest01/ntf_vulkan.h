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
#include"glmNTF.h"
#include<iostream>
#include<stdexcept>
#include<thread>
#include<unordered_map>
#include<vector>

#include"StackNTF.h"
#include"StreamingUnit.h"

#include"stdArrayUtility.h"

extern bool g_deviceDiagnosticCheckpointsSupported;

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

#define NTF_DEVICE_EXTENSIONS_NUM 2

class VulkanPagedStackAllocator;

static const uint32_t s_kWidth = 1600;
static const uint32_t s_kHeight = 1200;

typedef uint32_t PushConstantBindIndexType;

struct DrawFrameFinishedFence
{
    DrawFrameFinishedFence()
    {
        m_frameNumberCpuRecordedCompleted = true;//don't record garbage frame numbers -- wait until they're filled out on submission to Gpu
    }

    VkFence m_fence;
    StreamingUnitRuntime::FrameNumber m_frameNumberCpuSubmitted;///<to track when the Gpu is finished with a frame
    bool m_frameNumberCpuRecordedCompleted;
};

void NTFVulkanInitialize(const VkPhysicalDevice& physicalDevice);
void GetPhysicalDeviceMemoryPropertiesCached(VkPhysicalDeviceMemoryProperties**const memPropertiesPtr);
void GetPhysicalDevicePropertiesCached(VkPhysicalDeviceProperties**const physicalDevicePropertiesPtr);

VkAllocationCallbacks* GetVulkanAllocationCallbacks();
#if NTF_DEBUG
size_t GetVulkanApiCpuBytesAllocatedMax();
#endif//#if NTF_DEBUG
HANDLE MutexCreate();
void MutexRelease(const HANDLE mutex);
HANDLE ThreadSignalingEventCreate();
BOOL HandleCloseWindows(HANDLE*const h);
void UnsignalSemaphoreWindows(const HANDLE semaphoreHandle);
void SignalSemaphoreWindows(const HANDLE semaphoreHandle);
void WaitForSignalWindows(const HANDLE semaphoreOrMutexHandle);
void TransferImageFromCpuToGpu(
    const VkImage& image,
    const uint32_t width,
    const uint32_t height,
    const VkFormat& format,
    const VkBuffer& stagingBuffer,
    const VkCommandBuffer commandBufferTransfer,
    const uint32_t transferQueueFamilyIndex,
    const VkCommandBuffer commandBufferGraphics,
    const uint32_t graphicsQueueFamilyIndex,
    const VkDevice& device, 
    const VkInstance instance);
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
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
void CreateBuffer(
    VkBuffer*const vkBufferPtr,
    VkDeviceSize*const stagingBufferGpuOffsetToAllocatedBlockPtr,
    const VkDeviceMemory& vkBufferMemory,
    const VkDeviceSize& offsetToAllocatedBlock,
    const VkDeviceSize& vkBufferSizeBytes,
    const VkMemoryPropertyFlags& flags,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
void BindBufferMemory(const VkBuffer& buffer, const VkDeviceMemory& bufferMemory, const VkDeviceSize& offsetToAllocatedBlock, const VkDevice& device);
void CopyBufferToImage(
    const VkBuffer& buffer,
    const VkImage& image,
    const uint32_t width,
    const uint32_t height,
    const VkCommandBuffer& commandBuffer,
    const VkDevice& device,
    const VkInstance instance);
VkResult SubmitCommandBuffer(
    ConstVectorSafeRef<VkSemaphore> signalSemaphores,
    ConstVectorSafeRef<VkSemaphore> waitSemaphores,
    ArraySafeRef<VkPipelineStageFlags> stagesWhereEachWaitSemaphoreWaits,///<@todo: ConstArraySafeRef
    const VkCommandBuffer& commandBuffer,
    const VkQueue& queue,
    const HANDLE*const queueMutexPtr,
    const VkFence& fenceToSignalWhenCommandBufferDone,
    const VkInstance& instance);
uint32_t FindMemoryType(const uint32_t typeFilter, const VkMemoryPropertyFlags& properties, const VkPhysicalDevice& physicalDevice);
uint32_t FindMemoryHeapIndex(const VkMemoryPropertyFlags& properties, const VkPhysicalDevice& physicalDevice);
void CreateBuffer(
    VkBuffer*const bufferPtr,
    VkDeviceMemory*const bufferMemoryPtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    VkDeviceSize*const offsetToAllocatedBlockPtr,
    VkDeviceSize size,
    const VkBufferUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const bool respectNonCoherentAtomSize,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
VkFormat FindDepthFormat(const VkPhysicalDevice& physicalDevice);
void CreateShaderModule(VkShaderModule*const shaderModulePtr, char*const code, const size_t codeSizeBytes, const VkDevice& device);
bool CheckValidationLayerSupport(ConstVectorSafeRef<const char*> validationLayers);
void CreateImageView(VkImageView*const imageViewPtr, const VkDevice& device, const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectFlags);
void ReadFile(char**const fileData, StackCpu<VkDeviceSize>*const allocatorPtr, size_t*const fileSizeBytesPtr, const char*const filename);
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
void BeginCommandBuffer(const VkCommandBuffer& commandBuffer, const VkDevice& device);

struct UniformBufferObject
{
    glm::mat4 modelToClip;
};

///standard handles all threads need
struct ThreadHandles
{
    HANDLE threadHandle;
    HANDLE wakeEventHandle;
    HANDLE doneEventHandle;
};

//struct CommandBufferThreadArgumentsTest
//{
//    VkCommandBuffer* commandBuffer;
//    VkDescriptorSet* descriptorSet;
//    VkRenderPass* renderPass;
//    VkExtent2D* swapChainExtent;
//    VkPipelineLayout* pipelineLayout;
//    VkBuffer* vertexBuffer;
//    VkBuffer* indexBuffer;
//    VkFramebuffer* swapChainFramebuffer;
//    VkPipeline* graphicsPipeline;
//    uint32_t* objectIndex;
//    uint32_t* indicesNum;
//    HANDLE* commandBufferThreadDone;
//    HANDLE* commandBufferThreadWake;
//};

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
    const QueueFamilyIndices& indices,
    const VkPhysicalDevice& physicalDevice);

void CreateDescriptorSetLayout(
    VkDescriptorSetLayout*const descriptorSetLayoutPtr,
    const VkDescriptorType descriptorType,
    const VkDevice& device,
    const uint32_t objectsNum);
void CreateGraphicsPipeline(
    VkPipelineLayout*const pipelineLayoutPtr,
    VkPipeline*const graphicsPipelinePtr,
    StackCpu<size_t>*const allocatorPtr,
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

void FillCommandBufferPrimary(
    StreamingUnitRuntime::FrameNumber*const streamingUnitLastFrameSubmittedPtr,
    bool*const renderedOnceSinceLastLoadPtr,
    const StreamingUnitRuntime::FrameNumber currentFrameNumber,
    const VkCommandBuffer& commandBufferPrimary,
    const ArraySafeRef<TexturedGeometry> texturedGeometries,
    const VkDescriptorSet descriptorSet,
    const size_t objectNum,
    const size_t drawCallsPerObjectNum,
    const VkPipelineLayout& pipelineLayout,
    const VkPipeline& graphicsPipeline,
    const VkInstance& instance);

VkDeviceSize AlignToNonCoherentAtomSize(VkDeviceSize i);
void MapMemory(
    void** uniformBufferCpuMemoryCPtrPtr,
    const VkDeviceMemory& uniformBufferGpuMemory,
    const VkDeviceSize& offsetToGpuMemory,
    const VkDeviceSize bufferSize,
    const VkDevice& device);
void CreateUniformBuffer(
    ArraySafeRef<uint8_t>*const uniformBufferCpuMemoryPtr,
    VkDeviceMemory*const uniformBufferGpuMemoryPtr,
    VkBuffer*const uniformBufferPtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    VkDeviceSize*const offsetToGpuMemoryPtr,
    const VkDeviceSize bufferSize,
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

void CopyBufferToGpuPrepare(
    VulkanPagedStackAllocator*const deviceLocalMemoryPtr,
    VkBuffer*const gpuBufferPtr,
    VkDeviceMemory*const gpuBufferMemoryPtr,
    VectorSafeRef<VkBuffer> stagingBuffersGpu,
    VkDeviceSize*const stagingBufferGpuOffsetToAllocatedBlockPtr,
    const VkDeviceMemory stagingBufferGpuMemory,
    const VkDeviceSize stagingBufferGpuAlignmentStandard,
    const VkDeviceSize offsetToFirstByteOfStagingBuffer,
    const VkDeviceSize bufferSize,
    const VkMemoryPropertyFlags& memoryPropertyFlags,
    const VkCommandBuffer& commandBuffer,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice,
    const VkInstance instance);
void CreateAndCopyToGpuBuffer(
    VulkanPagedStackAllocator*const allocatorPtr,
    VkBuffer*const gpuBufferPtr,
    VkDeviceMemory*const gpuBufferMemoryPtr,
    const VkBuffer& stagingBufferGpu,
    const VkDeviceSize bufferSize,
    const VkMemoryPropertyFlags& flags,
    const VkCommandBuffer& commandBuffer,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice,
    const VkInstance instance);

void EndCommandBuffer(const VkCommandBuffer& commandBuffer);
void EndSingleTimeCommandsStall(const VkCommandBuffer& commandBuffer, const VkQueue& queue, const VkDevice& device);
void CreateCommandPool(VkCommandPool*const commandPoolPtr, const uint32_t& queueFamilyIndex, const VkDevice& device, const VkPhysicalDevice& physicalDevice);
void CreateDepthResources(
    VkImage*const depthImagePtr,
    VkImageView*const depthImageViewPtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    const VkExtent2D& swapChainExtent,
    const VkCommandBuffer& commandBuffer,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice,
    const VkInstance instance);

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
void ReadTextureAndCreateImageAndCopyPixelsIfStagingBufferHasSpace(
    VkImage*const imagePtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    StreamingUnitTextureDimension*const textureWidthPtr,
    StreamingUnitTextureDimension*const textureHeightPtr,
    StackCpu<VkDeviceSize>*const stagingBufferMemoryMapCpuToGpuStackPtr,
    size_t*const imageSizeBytesPtr,
    VkDeviceSize*const stagingBufferGpuOffsetToAllocatedBlockPtr,
    FILE*const streamingUnitFile,
    const VkFormat& format,
    const VkImageTiling& tiling,
    const VkImageUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
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
    ArraySafeRef<DrawFrameFinishedFence> fence,
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

///@todo: find better translation unit
template<class T>
inline T MaxNtf(const T a, const T b)
{
    assert(a >= 0);
    assert(b >= 0);
    return a > b ? a : b;
}

enum class CmdSetCheckpointValues :size_t 
{
    vkCmdBeginRenderPass_kBefore, 
    vkCmdBeginRenderPass_kAfter,
    vkCmdEndRenderPass_kBefore,
    vkCmdEndRenderPass_kAfter,
    vkCmdCopyBufferToImage_kBefore, 
    vkCmdCopyBufferToImage_kAfter, 
    vkCmdPipelineBarrier_kBefore, 
    vkCmdPipelineBarrier_kAfter, 
    vkCmdCopyBuffer_kBefore, 
    vkCmdCopyBuffer_kAfter, 
    vkCmdBindPipeline_kBefore, 
    vkCmdBindPipeline_kAfter, 
    vkCmdBindDescriptorSets_kBefore,
    vkCmdBindDescriptorSets_kAfter, 
    vkCmdBindVertexBuffers_kBefore, 
    vkCmdBindVertexBuffers_kAfter, 
    vkCmdBindIndexBuffer_kBefore, 
    vkCmdBindIndexBuffer_kAfter, 
    vkCmdPushConstants_kBefore, 
    vkCmdPushConstants_kAfter, 
    vkCmdDrawIndexed_kBefore, 
    vkCmdDrawIndexed_kAfter,
    Num};
static ArraySafe<CmdSetCheckpointValues, static_cast<size_t>(CmdSetCheckpointValues::Num)> s_cmdSetCheckpointData;
void CmdSetCheckpointNV(const VkCommandBuffer& commandBuffer, const CmdSetCheckpointValues*const pCheckpointMarker, const VkInstance& instance);

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
    inline void ClearSuballocations()
    {
        m_stack.ClearSuballocations();
    }

    inline bool SufficientMemory(const VkDeviceSize alignment, const VkDeviceSize size, const bool respectNonCoherentAtomSize) const
    {
        assert(m_stack.Allocated());
        VkDeviceSize dummy0, dummy1;
        return PushAlloc(&dummy0, &dummy1, alignment, size, respectNonCoherentAtomSize);
    }
    bool PushAlloc(VkDeviceSize* memoryOffsetPtr, VkDeviceSize alignment, const VkDeviceSize size, const bool respectNonCoherentAtomSize);
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
        VkDeviceSize alignment,
        const VkDeviceSize size,
        const bool respectNonCoherentAtomSize) const;
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

    void FreeAllPages(const bool deallocateBackToGpu, const VkDevice device);

    bool PushAlloc(
        VkDeviceSize* memoryOffsetPtr,
        VkDeviceMemory* memoryHandlePtr,
        const VkDeviceSize alignment,
        const VkDeviceSize size,
        const VkMemoryPropertyFlags& properties,
        const bool linearResource,
        const bool respectNonCoherentAtomSize,
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

    void FreeAllPages(const bool deallocateBackToGpu, const VkDevice& device);

    bool PushAlloc(
        VkDeviceSize* memoryOffsetPtr,
        VkDeviceMemory* memoryHandlePtr,
        const uint32_t memRequirementsMemoryTypeBits,
        const VkDeviceSize alignment,
        const VkDeviceSize size,
        const VkMemoryPropertyFlags& properties,
        const bool linearResource,
        const bool respectNonCoherentAtomSize,
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

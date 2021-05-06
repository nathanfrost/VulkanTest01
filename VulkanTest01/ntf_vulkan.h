#pragma once

#include "WinTimer.h"//has to be #included before glfw*.h

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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

#include"volk.h"
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


#define NTF_VALIDATION_LAYERS_BASE_SIZE 1
#if NTF_API_DUMP_VALIDATION_LAYER_ON
#define NTF_VALIDATION_LAYERS_SIZE (NTF_VALIDATION_LAYERS_BASE_SIZE + 1)
#else
#define NTF_VALIDATION_LAYERS_SIZE (NTF_VALIDATION_LAYERS_BASE_SIZE)
#endif//NTF_API_DUMP_VALIDATION_LAYER_ON

#define NTF_DEVICE_EXTENSIONS_NUM 2

static const uint32_t s_kWidth = 1600;
static const uint32_t s_kHeight = 900;

typedef uint32_t PushConstantBindIndexType;

VkAllocationCallbacks* GetVulkanAllocationCallbacks();
class VulkanMemoryHeapPage
{
public:
    VulkanMemoryHeapPage() = default;
    VulkanMemoryHeapPage(const VulkanMemoryHeapPage& other) = default;
    VulkanMemoryHeapPage& operator=(const VulkanMemoryHeapPage& other) = default;
    VulkanMemoryHeapPage(VulkanMemoryHeapPage&& other) = default;
    VulkanMemoryHeapPage& operator=(VulkanMemoryHeapPage&& other) = default;
    ~VulkanMemoryHeapPage() = default;

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
    explicit VulkanMemoryHeap()
    {
#if NTF_DEBUG
        m_initialized = false;
#endif//#if NTF_DEBUG
    };
    void Initialize(const uint32_t memoryTypeIndex, const VkDeviceSize memoryHeapPageSizeBytes);
    VulkanMemoryHeap(const VulkanMemoryHeap& other) = default;
    VulkanMemoryHeap& operator=(const VulkanMemoryHeap& other) = default;
    VulkanMemoryHeap(VulkanMemoryHeap&& other) = default;
    VulkanMemoryHeap& operator=(VulkanMemoryHeap&& other) = default;
    ~VulkanMemoryHeap() = default;


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
    uint32_t m_memoryTypeIndex;///<note that this means we maintain one VulkanMemoryHeap per VkMemoryAllocateInfo::memoryTypeIndex, even though typically some of these VkMemoryAllocateInfo::memoryTypeIndex's map to the same heap, and thus can (probably?) be efficiently allocated from the same range of Gpu memory (and thus probably with less fragmentation)
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
    enum class HeapSize :size_t { SMALL = 0, MEDIUM, LARGE, NUM };
    static size_t I(const HeapSize hs) { return static_cast<size_t>(hs); }

    explicit VulkanPagedStackAllocator()
    {
#if NTF_DEBUG
        m_initialized = false;
#endif//#if NTF_DEBUG
    }
    VulkanPagedStackAllocator(const VulkanPagedStackAllocator& other) = default;
    VulkanPagedStackAllocator& operator=(const VulkanPagedStackAllocator& other) = default;
    VulkanPagedStackAllocator(VulkanPagedStackAllocator&& other) = default;
    VulkanPagedStackAllocator& operator=(VulkanPagedStackAllocator&& other) = default;
    ~VulkanPagedStackAllocator() = default;

    void Initialize(const VkDevice& device, const VkPhysicalDevice& physicalDevice);
    void Destroy(const VkDevice& device);

    void FreeAllPages(const bool deallocateBackToGpu, const VkDevice& device);

    bool PushAlloc(
        VkDeviceSize* memoryOffsetPtr,
        VkDeviceMemory* memoryHandlePtr,
        const uint32_t memRequirementsMemoryTypeBits,
        const VkDeviceSize alignment,
        const VkDeviceSize size,
        const HeapSize heapSize,
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

    ArraySafe<VectorSafe<VulkanMemoryHeap, 16>, static_cast<size_t>(HeapSize::NUM)> m_vulkanMemoryHeaps;
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;
    RTL_CRITICAL_SECTION m_criticalSection;
};

struct DrawFrameFinishedFence
{
    DrawFrameFinishedFence()
    {
        m_frameNumberCpuCompletedByGpu = true;//don't record garbage frame numbers -- wait until they're filled out on submission to Gpu
    }

    VkFence m_fence;
    StreamingUnitRuntime::FrameNumber m_frameNumberCpuSubmitted;///<to track when the Gpu is finished with a frame
    bool m_frameNumberCpuCompletedByGpu;
};

void NTFVulkanInitialize(const VkPhysicalDevice& physicalDevice);
void GetPhysicalDeviceMemoryPropertiesCached(VkPhysicalDeviceMemoryProperties**const memPropertiesPtr);
void GetPhysicalDevicePropertiesCached(VkPhysicalDeviceProperties**const physicalDevicePropertiesPtr);

#if NTF_DEBUG
size_t GetVulkanApiCpuBytesAllocatedMax();
#endif//#if NTF_DEBUG
void TransferImageFromCpuToGpu(
    const VkImage& image,
    const uint32_t widthMip0,
    const uint32_t heightMip0,
    const uint32_t mipLevels,
    const ConstVectorSafeRef<VkBuffer>& stagingBuffers,
    const VkCommandBuffer commandBufferTransfer,
    const uint32_t transferQueueFamilyIndex,
    const VkCommandBuffer commandBufferGraphics,
    const uint32_t graphicsQueueFamilyIndex,
    const VkDevice& device,
    const VkInstance& instance);
void CreateTextureImageView(VkImageView*const textureImageViewPtr, const VkImage& textureImage, const uint32_t mipLevels, const VkDevice& device);
bool CreateAllocateBindImageIfAllocatorHasSpace(
    VkImage*const imagePtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    VkMemoryRequirements*const memRequirementsPtr,
    VkDeviceSize*const memoryOffsetPtr,
    VkDeviceMemory*const memoryHandlePtr,
    const uint32_t width,
    const uint32_t height,
    const uint32_t mipLevels,
    const VkFormat& format,
    const VkSampleCountFlagBits& sampleCountFlagBits,
    const VkImageTiling& tiling,
    const VkImageUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const bool respectNonCoherentAtomAlignment,
    const VkPhysicalDevice& physicalDevice,
    const VkDevice& device);
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
    const uint32_t mipLevel,
    const VkCommandBuffer& commandBuffer,
    const VkDevice& device,
    const VkInstance& instance);
void CmdPipelineImageBarrier(
    const VkImageMemoryBarrier*const barrierPtr,
    const VkCommandBuffer& commandBuffer,
    const VkPipelineStageFlags& srcStageMask,
    const VkPipelineStageFlags& dstStageMask);
void ImageMemoryBarrier(
    const VkImageLayout& oldLayout,
    const VkImageLayout& newLayout,
    const uint32_t srcQueueFamilyIndex,
    const uint32_t dstQueueFamilyIndex,
    const VkImageAspectFlagBits& aspectMask,
    const VkImage& image,
    const VkAccessFlags& srcAccessMask,
    const VkAccessFlags& dstAccessMask,
    const VkPipelineStageFlags& srcStageMask,
    const VkPipelineStageFlags& dstStageMask,
    const uint32_t mipLevels,
    const VkCommandBuffer& commandBuffer,
    const VkInstance instance);
void CmdPipelineBufferBarrier(
    const VkBufferMemoryBarrier*const barrierPtr,
    const VkCommandBuffer& commandBuffer,
    const VkPipelineStageFlags& srcStageMask,
    const VkPipelineStageFlags& dstStageMask);

VkResult SubmitCommandBuffer(
    RTL_CRITICAL_SECTION*const criticalSectionPtr,
    const ConstVectorSafeRef<VkSemaphore>& waitSemaphores,
    const ConstVectorSafeRef<VkSemaphore>& signalSemaphores,
    const ConstArraySafeRef<VkPipelineStageFlags>& stagesWhereEachWaitSemaphoreWaits,
    const VkCommandBuffer& commandBuffer,
    const VkQueue& queue,
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
    VulkanPagedStackAllocator::HeapSize heapSize,
    const VkBufferUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const bool respectNonCoherentAtomSize,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);
VkFormat FindDepthFormat(const VkPhysicalDevice& physicalDevice);
void CreateShaderModule(VkShaderModule*const shaderModulePtr, char*const code, const size_t codeSizeBytes, const VkDevice& device);
bool CheckValidationLayerSupport(const ConstVectorSafeRef<const char*>& validationLayers);
void CreateImageView(
    VkImageView*const imageViewPtr,
    const VkImage& image,
    const VkFormat& format,
    const VkImageAspectFlags& aspectFlags,
    const uint32_t mipLevels,
    const VkDevice& device);
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
void CommandBufferBegin(const VkCommandBuffer& commandBuffer, const VkDevice& device);

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

void GetRequiredExtensions(VectorSafeRef<const char*>*const requiredExtensions, const bool enableValidationLayers);

void ValidationLayersInitialize(VectorSafeRef<const char *> validationLayers);
VkInstance InstanceCreate(const ConstVectorSafeRef<const char*>& validationLayers);

VkDebugReportCallbackEXT SetupDebugCallback(const VkInstance& instance, const bool enableValidationLayers);

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
    typedef int IndexDataType;
    enum Type:IndexDataType {kGraphicsQueue, kComputeQueue, kTransferQueue, kPresentQueue, kTypeSize, kUninitialized = -1};
    ArraySafe<IndexDataType, Type::kTypeSize> index;
#define NTF_QUEUE_NUM_EXCLUDING_PRESENT Type::kTypeSize - 1
    const ArraySafe<VkQueueFlagBits, NTF_QUEUE_NUM_EXCLUDING_PRESENT> kFamilyIndicesQueueFlags = ArraySafe<VkQueueFlagBits, NTF_QUEUE_NUM_EXCLUDING_PRESENT>(
        {VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT});//mirrors QueueFamily, except present queue, which is unfortunately a special case in Vulkan and has no VK_QUEUE_PRESENT_BIT
    QueueFamilyIndices()
    {
        for (auto& familyIndex : index)
        {
            familyIndex = kUninitialized;
        }
    }

    bool IsComplete()
    {
        for (const auto& familyIndex : index)
        {
            if (familyIndex == kUninitialized)
            {
                return false;
            }
        }
        return true;
    }

    inline IndexDataType GraphicsQueueIndex() const { return index[QueueFamilyIndices::Type::kGraphicsQueue]; }
    inline IndexDataType TransferQueueIndex() const { return index[QueueFamilyIndices::Type::kTransferQueue]; }
    inline IndexDataType PresentQueueIndex() const { return index[QueueFamilyIndices::Type::kPresentQueue]; }
};

void PhysicalDeviceQueueFamilyPropertiesGet(VectorSafeRef<VkQueueFamilyProperties> queueFamilyProperties, const VkPhysicalDevice& device);
void FindQueueFamilies(QueueFamilyIndices*const queueFamilyIndicesPtr, const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface);

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const ConstVectorSafeRef<VkSurfaceFormatKHR>& availableFormats);

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

void DestroyImageView(const VkImageView& framebufferColorImageView, const VkDevice& device);
void DestroyImage(const VkImage& framebufferColorImage, const VkDevice& device);
void CleanupSwapChain(
    const ConstVectorSafeRef<VkCommandBuffer>& commandBuffersPrimary,
    const VkDevice& device,
    const VkImageView& framebufferColorImageView,
    const VkImage& framebufferColorImage,
    const VkImageView& depthImageView,
    const VkImage& depthImage,
    const ConstVectorSafeRef<VkFramebuffer>& swapChainFramebuffers,
    const VkCommandPool& commandPool,
    const ConstVectorSafeRef<ArraySafe<VkCommandPool, 2>>& commandPoolsSecondary,///<@todo NTF: refactor out magic number 2 (meant to be NTF_OBJECTS_NUM) and either support VectorSafeRef<ArraySafeRef<T>> or repeatedly call FreeCommandBuffers on each VectorSafe outside of this function
    const VkRenderPass& renderPass,
    const ConstVectorSafeRef<VkImageView>& swapChainImageViews,
    const VkSwapchainKHR& swapChain);

void CreateImageViews(
    VectorSafeRef<VkImageView> swapChainImageViewsPtr,
    const ConstVectorSafeRef<VkImage>& swapChainImages,
    const VkFormat& swapChainImageFormat,
    const VkDevice& device);

bool CheckDeviceExtensionSupport(const VkPhysicalDevice& physicalDevice, const ConstVectorSafeRef<const char*>& deviceExtensions);

bool IsDeviceSuitable(
    const VkPhysicalDevice& physicalDevice,
    const VkSurfaceKHR& surface,
    const ConstVectorSafeRef<const char*>& deviceExtensions);

bool PhysicalDevicesGet(VectorSafeRef<VkPhysicalDevice> physicalDevices, const VkInstance& instance);
bool PickPhysicalDevice(
    VkPhysicalDevice*const physicalDevicePtr,
    const VkSurfaceKHR& surface,
    const ConstVectorSafeRef<const char*>& deviceExtensions,
    const VkInstance& instance);

void CreateLogicalDevice(
    VkDevice*const devicePtr,
    VkQueue*const graphicsQueuePtr,
    VkQueue*const presentQueuePtr,
    VkQueue*const transferQueuePtr,
    const ConstVectorSafeRef<const char*>& deviceExtensions,
    const ConstVectorSafeRef<const char*>& validationLayers,
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
    const VkSampleCountFlagBits& sampleCountFlagBitMsaa,
    const VkDevice& device);
void CreateRenderPass(
    VkRenderPass*const renderPassPtr,
    const VkSampleCountFlagBits& msaaSamples,
    const VkFormat& swapChainImageFormat,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice);

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
    const StreamingUnitRuntime::FrameNumber currentFrameNumber,
    const VkCommandBuffer& commandBufferPrimary,
    const ConstArraySafeRef<TexturedGeometry>& texturedGeometries,
    const VkDescriptorSet descriptorSet,
    const size_t objectNum,
    const size_t drawCallsPerObjectNum,
    const VkPipelineLayout& pipelineLayout,
    const VkPipeline& graphicsPipeline,
    const VkInstance& instance);

VkDeviceSize AlignToNonCoherentAtomSize(VkDeviceSize i);
void MapMemory(
    ArraySafeRef<uint8_t>*const cpuMemoryArraySafePtr,
    const VkDeviceMemory& gpuMemory,
    const VkDeviceSize& offsetToGpuMemory,
    const VkDeviceSize bufferSize,
    const VkDevice& device);
ConstArraySafeRef<uint8_t> MapMemory(
    const VkDeviceMemory& gpuMemory,
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
    const ConstArraySafeRef<VkImageView>& textureImageViews,
    const size_t texturesNum,
    const VkSampler textureSampler,
    const VkDevice& device);

void CopyVertexOrIndexBufferToGpu(
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
    const VkCommandBuffer commandBufferTransfer,
    const uint32_t transferQueueFamilyIndex,
    const VkCommandBuffer commandBufferGraphics,
    const uint32_t graphicsQueueFamilyIndex,
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

void CommandBufferEnd(const VkCommandBuffer& commandBuffer);
void CreateCommandPool(VkCommandPool*const commandPoolPtr, const uint32_t& queueFamilyIndex, const VkDevice& device, const VkPhysicalDevice& physicalDevice);
void CreateImageViewResources(
    VkImage*const depthImagePtr,
    VkImageView*const depthImageViewPtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    const VkFormat& format,
    const VkSampleCountFlagBits& sampleCountFlagBits,
    const VkImageUsageFlags& imageUsageFlags,
    const VkImageAspectFlags& imageAspectFlags,
    const VkMemoryPropertyFlags& memoryPropertyFlags,
    const VkExtent2D& swapChainExtent,
    const VkCommandBuffer& commandBuffer,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice,
    const VkInstance instance);

VkFormat FindSupportedFormat(
    const VkPhysicalDevice& physicalDevice,
    const ConstVectorSafeRef<VkFormat>& candidates,
    const VkImageTiling& tiling,
    const VkFormatFeatureFlags& features);

bool HasStencilComponent(VkFormat format);

void ReadTextureAndCreateImageAndCopyPixelsIfStagingBufferHasSpace(
    VkImage*const imagePtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    StreamingUnitTextureDimension*const textureWidthPtr,
    StreamingUnitTextureDimension*const textureHeightPtr,
    uint32_t*const mipLevelsPtr,
    StackCpu<VkDeviceSize>*const stagingBufferMemoryMapCpuToGpuStackPtr,
    size_t*const imageSizeBytesPtr,
    VkDeviceSize*const stagingBufferGpuOffsetToAllocatedBlockPtr,
    FILE*const streamingUnitFile,
    VkMemoryRequirements*const memoryRequirementsPtr,
    VectorSafeRef<VkBuffer> stagingBuffersGpu,
    const VkDeviceMemory stagingBufferGpuMemory,
    const VkDeviceSize offsetToFirstByteOfStagingBuffer,
    const VkFormat& format,
    const VkImageTiling& tiling,
    const VkImageUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const VkPhysicalDevice& physicalDevice,
    const VkDevice& device);
void CreateTextureSampler(VkSampler*const textureSamplerPtr, const VkDevice& device);

void CreateFramebuffers(
    VectorSafeRef<VkFramebuffer> swapChainFramebuffersPtr,
    const ConstVectorSafeRef<VkImageView>& swapChainImageViews,
    const VkRenderPass& renderPass,
    const VkExtent2D& swapChainExtent,
    const VkImageView& framebufferColorImageView,
    const VkImageView& depthImageView,
    const VkDevice& device);
void CreateSurface(VkSurfaceKHR*const surfacePtr, GLFWwindow*const window, const VkInstance& instance);
void CreateVulkanSemaphore(VkSemaphore*const semaphorePtr, const VkDevice& device);
void CreateFrameSyncPrimitives(
    ArraySafeRef<VkSemaphore> imageAvailable,
    ArraySafeRef<VkSemaphore> renderFinished,
    ArraySafeRef<DrawFrameFinishedFence> fence,
    const size_t framesNum,
    const VkDevice& device);

void FenceCreate(VkFence*const fencePtr, const VkFenceCreateFlagBits flags, const VkDevice& device);
void FenceWaitUntilSignalled(const VkFence& fence, const VkDevice& device);
void FenceReset(const VkFence& fence, const VkDevice& device);

void FlushMemoryMappedRange(
    const VkDeviceMemory& gpuMemory,
    const VkDeviceSize offsetIntoGpuMemoryToFlush,
    const VkDeviceSize sizeBytesToFlush,
    const VkDevice& device);
void UpdateUniformBuffer(
    ArraySafeRef<uint8_t> uniformBufferCpuMemory,
    const glm::vec3 cameraTranslation,
    const VkDeviceMemory& uniformBufferGpuMemory,
    const VkDeviceSize& offsetToGpuMemory,
    const size_t drawCallsNum,
    const VkDeviceSize uniformBufferSize,
    const VkExtent2D& swapChainExtent,
    const VkDevice& device);

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

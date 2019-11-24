#pragma once
#include"ntf_vulkan.h"
#include"QueueCircular.h"

///@todo: update to latest code; this is currently broken
#define NTF_ASSET_LOADING_MULTITHREADED 1

enum { kStreamingUnitsNum = 6, kStreamingUnitsRenderableNum = 3, kStreamingUnitCommandsNum = kStreamingUnitsRenderableNum * 4 };

DWORD WINAPI AssetLoadingThread(void* arg);

struct QueueFamilyIndices;
class StreamingCommandQueueManager;

enum class AssetLoadingArgumentsThreadCommand
{
    kFirstValidArgument, kProcessStreamingUnits = kFirstValidArgument,
    kLastValidArgument, kCleanupAndTerminate = kLastValidArgument
};
class AssetLoadingArguments
{
public:
    VulkanPagedStackAllocator* m_deviceLocalMemoryPersistent;
    VectorSafeRef<VulkanPagedStackAllocator> m_deviceLocalMemoryStreamingUnits;
    ArraySafeRef<bool> m_deviceLocalMemoryStreamingUnitsAllocated;
	VectorSafeRef<StreamingUnitRuntime*> m_streamingUnitsToAddToLoad;
	VectorSafeRef<StreamingUnitRuntime*> m_streamingUnitsToAddToRenderable;
    AssetLoadingArgumentsThreadCommand *m_threadCommand;

    const VkCommandBuffer* m_commandBufferTransfer;
    const VkCommandBuffer* m_commandBufferTransitionImage;
    const VkDevice* m_device;
    const HANDLE* m_deviceLocalMemoryMutex;
    const VkQueue* m_graphicsQueue;
    const HANDLE* m_graphicsQueueMutex;
    const VkInstance* m_instance;
    const VkPhysicalDevice* m_physicalDevice;
    const QueueFamilyIndices* m_queueFamilyIndices;
    const VkRenderPass* m_renderPass;
	const HANDLE* m_streamingUnitsAddToLoadListMutex;
	const HANDLE* m_streamingUnitsAddToRenderableMutex;
    const VkExtent2D* m_swapChainExtent;
    const HANDLE* m_threadDone;
    const HANDLE* m_threadWake;
    const VkQueue* m_transferQueue;

    inline void AssertValid()
    {
        assert(m_deviceLocalMemoryPersistent);
		//assert(m_deviceLocalMemoryStreamingUnits);//alphabetical placeholder since class already auto-asserts
		//assert(m_deviceLocalMemoryStreamingUnitsAllocated);//alphabetical placeholder since class already auto-asserts
		//assert(m_streamingUnitsToAddToLoad);//alphabetical placeholder since class already auto-asserts
		//assert(m_streamingUnitsToAddToRenderable);//alphabetical placeholder since class already auto-asserts

        assert(m_commandBufferTransfer);
        assert(m_commandBufferTransitionImage);
        assert(m_device);
        assert(m_deviceLocalMemoryMutex);
        assert(m_graphicsQueue);
        assert(m_graphicsQueueMutex);
        assert(m_instance);
        assert(m_physicalDevice);
        assert(m_queueFamilyIndices);
        assert(m_renderPass);
		assert(m_streamingUnitsAddToLoadListMutex);
		assert(m_streamingUnitsAddToRenderableMutex);
        assert(m_swapChainExtent);
		assert(m_threadDone);
		assert(m_threadWake);
		assert(m_transferQueue);
    }
};
struct AssetLoadingPersistentResources
{
	VkDeviceSize offsetToFirstByteOfStagingBuffer;
	StackCpu<size_t> shaderLoadingScratchSpace;
    VkBuffer stagingBufferGpu;
	VkDeviceSize stagingBufferGpuAlignmentStandard;
    VkDeviceMemory stagingBufferGpuMemory;
    StackCpu<VkDeviceSize> stagingBufferMemoryMapCpuToGpu;
    VkSemaphore transferFinishedSemaphore;
};
void AssetLoadingThreadPersistentResourcesCreate(
    AssetLoadingPersistentResources*const assetLoadingPersistentResourcesPtr,
    VulkanPagedStackAllocator*const deviceLocalMemoryPersistentPtr,
    const VkPhysicalDevice& physicalDevice,
    const VkDevice& device);
void AssetLoadingPersistentResourcesDestroy(
    AssetLoadingPersistentResources*const assetLoadingPersistentResourcesPtr,
    const HANDLE& threadDone,
    const VkDevice& device);

struct AssetLoadingThreadData
{
	ThreadHandles m_handles;
	AssetLoadingArgumentsThreadCommand m_threadCommand;
};
void StreamingCommandsProcess(
    AssetLoadingArguments*const threadArgumentsPtr,
    AssetLoadingPersistentResources*const assetLoadingPersistentResourcesPtr);

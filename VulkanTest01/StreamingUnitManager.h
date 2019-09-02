#pragma once
#include"ntf_vulkan.h"
#include"QueueCircular.h"

#define NTF_ASSET_LOADING_MULTITHREADED 0

DWORD WINAPI AssetLoadingThread(void* arg);

struct QueueFamilyIndices;
class StreamingUnitLoadQueueManager;
class AssetLoadingArguments
{
public:
    enum ThreadCommand {
        kFirstValidArgument, kLoadStreamingUnit = kFirstValidArgument,
        kLastValidArgument, kCleanupAndTerminate = kLastValidArgument
    } *m_threadCommand;

    VulkanPagedStackAllocator* m_deviceLocalMemoryPersistent;
    VectorSafeRef<VulkanPagedStackAllocator> m_deviceLocalMemoryStreamingUnits;
    ArraySafeRef<bool> m_deviceLocalMemoryStreamingUnitsAllocated;
    StreamingUnitLoadQueueManager* m_streamingUnitLoadQueueManager;

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
    const VkExtent2D* m_swapChainExtent;
    const HANDLE* m_threadDone;
    const HANDLE* m_threadWake;
    const VkQueue* m_transferQueue;

    inline void AssertValid()
    {
        assert(m_threadCommand && *m_threadCommand >= kFirstValidArgument && *m_threadCommand <= kLastValidArgument);
        assert(m_deviceLocalMemoryPersistent);

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
        assert(m_streamingUnitLoadQueueManager);
        assert(m_swapChainExtent);
        assert(m_threadDone);
        assert(m_threadWake);
        assert(m_transferQueue);
    }
};
struct AssetLoadingPersistentResources
{
    StackCpu<size_t> shaderLoadingScratchSpace;
    VkBuffer stagingBufferGpu;
    VkDeviceMemory stagingBufferGpuMemory;
    VkDeviceSize offsetToFirstByteOfStagingBuffer;
    VkDeviceSize stagingBufferGpuAlignmentStandard;
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
class StreamingUnitQueue
{
public:
    inline StreamingUnitQueue()
    {
        m_modifyMutex = MutexCreate();
        m_streamingUnitsDoneLoadingHandle = ThreadSignalingEventCreate();
    }
    inline void Destroy()
    {
        HandleCloseWindows(&m_modifyMutex);
        HandleCloseWindows(&m_streamingUnitsDoneLoadingHandle);
    }

    HANDLE m_streamingUnitsDoneLoadingHandle;
    QueueCircular<StreamingUnitRuntime*, 7> m_queue;

private:
    friend StreamingUnitLoadQueueManager;
    HANDLE m_modifyMutex;
};
class StreamingUnitLoadQueueManager
{
public:
    inline StreamingUnitLoadQueueManager()
    {
        m_mainThreadStreamingUnitQueue0 = true;
        m_mainThread_StreamingUnitQueue_IsIndex0_Mutex = MutexCreate();
    }
    void Destroy();
    inline StreamingUnitQueue* GetMainThreadStreamingUnitLoadQueue_after_WaitForSignalWindows() ///<should only be called by main thread
    {  
        WaitForSignalWindows(m_mainThread_StreamingUnitQueue_IsIndex0_Mutex);
        StreamingUnitQueue*const streamingUnitQueuePtr = GetMainThreadStreamingUnitLoadQueue();
        MutexRelease(m_mainThread_StreamingUnitQueue_IsIndex0_Mutex);

        WaitForSignalWindows(streamingUnitQueuePtr->m_modifyMutex);
        return streamingUnitQueuePtr;
    }
    inline void Release(StreamingUnitQueue*const streamingUnitLoadQueuePtr)
    {
        NTF_REF(streamingUnitLoadQueuePtr, streamingUnitLoadQueue);
        MutexRelease(streamingUnitLoadQueue.m_modifyMutex);
    }
    inline StreamingUnitQueue* GetAssetLoadingStreamingUnitLoadQueue_after_WaitForSignalWindows()
    {
        WaitForSignalWindows(m_mainThread_StreamingUnitQueue_IsIndex0_Mutex);
        StreamingUnitQueue*const streamingUnitQueuePtr = GetAssetLoadingStreamingUnitLoadQueue();
        MutexRelease(m_mainThread_StreamingUnitQueue_IsIndex0_Mutex);

        WaitForSignalWindows(streamingUnitQueuePtr->m_modifyMutex);
        return streamingUnitQueuePtr;
    }
    void SwitchStreamingUnitLoadQueues_and_AcquireBothQueueMutexes();///<should only be called by asset loading thread

    inline StreamingUnitQueue* GetMainThreadStreamingUnitLoadQueue() { return   &m_streamingUnitQueues[m_mainThreadStreamingUnitQueue0 ? 0 : 1]; }///<should only be directly called by asset loading thread or by other methods in this class
    inline StreamingUnitQueue* GetAssetLoadingStreamingUnitLoadQueue() { return &m_streamingUnitQueues[m_mainThreadStreamingUnitQueue0 ? 1 : 0]; }///<should only be directly called by asset loading thread or by other methods in this class

private:
    ArraySafe<StreamingUnitQueue, 2> m_streamingUnitQueues;
    HANDLE m_mainThread_StreamingUnitQueue_IsIndex0_Mutex;
    bool m_mainThreadStreamingUnitQueue0;
};
struct AssetLoadingThreadData
{
    ThreadHandles m_handles;
    StreamingUnitLoadQueueManager m_streamingUnitQueue;
    AssetLoadingArguments::ThreadCommand m_threadCommand;
};

void StreamingUnitsLoadAllQueued(
    AssetLoadingArguments*const threadArgumentsPtr,
    AssetLoadingPersistentResources*const assetLoadingPersistentResourcesPtr);

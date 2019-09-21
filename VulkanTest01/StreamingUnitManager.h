#pragma once
#include"ntf_vulkan.h"
#include"QueueCircular.h"

#define NTF_ASSET_LOADING_MULTITHREADED 1


DWORD WINAPI AssetLoadingThread(void* arg);

struct QueueFamilyIndices;
class StreamingCommandQueueManager;
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
    StreamingCommandQueueManager* m_streamingCommandQueueManager;

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
        assert(m_streamingCommandQueueManager);
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
class StreamingCommand
{
public:
    enum Command
    {
        kLoad, kFirstValidValue = kLoad,
        kUnload, kLastValidValue = kUnload
    } m_command;
    StreamingUnitRuntime* m_streamingUnit;

	StreamingCommand() {};

    StreamingCommand(const Command command, StreamingUnitRuntime*const streamingUnit):
    m_command(command), m_streamingUnit(streamingUnit)
    {
		AssertValid();
    }
	void AssertValid()
	{
		assert(m_streamingUnit);
		assert(m_command >= kFirstValidValue);
		assert(m_command <= kLastValidValue);
	}
};
class StreamingCommandQueue
{
public:
    inline StreamingCommandQueue()
    {
        m_modifyMutex = MutexCreate();
        m_streamingCommandsDoneHandle = ThreadSignalingEventCreate();
        SignalSemaphoreWindows(m_streamingCommandsDoneHandle);//the streaming unit queue starts idle
        NTF_LOG_STREAMING("%i:StreamingCommandQueue:SignalSemaphoreWindows():streamingCommandQueue=%p->m_streamingUnitsDoneLoadingHandle=%zu\n",
            GetCurrentThreadId(), this, (size_t)m_streamingCommandsDoneHandle);
    }
    inline void Destroy()
    {
        HandleCloseWindows(&m_modifyMutex);
        HandleCloseWindows(&m_streamingCommandsDoneHandle);
    }

    HANDLE m_streamingCommandsDoneHandle;
    QueueCircular<StreamingCommand, 7> m_queue;

private:
    friend StreamingCommandQueueManager;
    HANDLE m_modifyMutex;
};
class StreamingCommandQueueManager
{
public:
    inline StreamingCommandQueueManager()
    {
        m_mainThreadStreamingCommandQueue0 = true;
        m_mainThread_StreamingCommandQueue_IsIndex0_Mutex = MutexCreate();
    }
    void Destroy();
    inline StreamingCommandQueue* GetMainThreadStreamingCommandQueue_after_WaitForSignalWindows() ///<should only be called by main thread
    {  
        WaitForSignalWindows(m_mainThread_StreamingCommandQueue_IsIndex0_Mutex);
        NTF_LOG_STREAMING("%i:GetMainThreadStreamingCommandQueue_after_WaitForSignalWindows:WaitForSignalWindows(m_mainThread_StreamingUnitQueue_IsIndex0_Mutex=%zu)\n", GetCurrentThreadId(), (size_t)m_mainThread_StreamingCommandQueue_IsIndex0_Mutex);
        
        StreamingCommandQueue*const streamingUnitQueuePtr = GetMainThreadStreamingCommandQueue();
        MutexRelease(m_mainThread_StreamingCommandQueue_IsIndex0_Mutex);
        NTF_LOG_STREAMING("%i:GetMainThreadStreamingCommandQueue_after_WaitForSignalWindows:MutexRelease(m_mainThread_StreamingUnitQueue_IsIndex0_Mutex=%zu)\n", GetCurrentThreadId(), (size_t)m_mainThread_StreamingCommandQueue_IsIndex0_Mutex);

        WaitForSignalWindows(streamingUnitQueuePtr->m_modifyMutex);
        NTF_LOG_STREAMING("%i:GetMainThreadStreamingCommandQueue_after_WaitForSignalWindows:WaitForSignalWindows(streamingUnitQueuePtr->m_modifyMutex=%zu)\n", GetCurrentThreadId(), (size_t)streamingUnitQueuePtr->m_modifyMutex);
        NTF_LOG_STREAMING("%i:GetMainThreadStreamingCommandQueue_after_WaitForSignalWindows:return streamingUnitQueuePtr=%p)\n", GetCurrentThreadId(), streamingUnitQueuePtr);
        return streamingUnitQueuePtr;
    }
    inline void Release(StreamingCommandQueue*const streamingCommandQueuePtr)
    {
        NTF_REF(streamingCommandQueuePtr, streamingCommandQueue);
        MutexRelease(streamingCommandQueue.m_modifyMutex);
        NTF_LOG_STREAMING("%i:GetMainThreadStreamingCommandQueue_after_WaitForSignalWindows:MutexRelease(streamingCommandQueue.m_modifyMutex=%zu)\n", GetCurrentThreadId(), (size_t)streamingCommandQueue.m_modifyMutex);
    }
    inline StreamingCommandQueue* GetAssetLoadingStreamingCommandQueue_after_WaitForSignalWindows()
    {
        WaitForSignalWindows(m_mainThread_StreamingCommandQueue_IsIndex0_Mutex);
        NTF_LOG_STREAMING("%i:GetAssetLoadingStreamingCommandQueue_after_WaitForSignalWindows:WaitForSignalWindows(m_mainThread_StreamingUnitQueue_IsIndex0_Mutex=%zu)\n", GetCurrentThreadId(), (size_t)m_mainThread_StreamingCommandQueue_IsIndex0_Mutex);

        StreamingCommandQueue*const streamingCommandQueuePtr = GetAssetLoadingStreamingCommandQueue();
        MutexRelease(m_mainThread_StreamingCommandQueue_IsIndex0_Mutex);
        NTF_LOG_STREAMING("%i:GetAssetLoadingStreamingCommandQueue_after_WaitForSignalWindows:MutexRelease(m_mainThread_StreamingUnitQueue_IsIndex0_Mutex=%zu)\n", GetCurrentThreadId(), (size_t)m_mainThread_StreamingCommandQueue_IsIndex0_Mutex);

        WaitForSignalWindows(streamingCommandQueuePtr->m_modifyMutex);
        NTF_LOG_STREAMING("%i:GetAssetLoadingStreamingCommandQueue_after_WaitForSignalWindows:WaitForSignalWindows(streamingUnitQueuePtr->m_modifyMutex=%zu)\n", GetCurrentThreadId(), (size_t)streamingCommandQueuePtr->m_modifyMutex);
        NTF_LOG_STREAMING("%i:GetMainThreadStreamingCommandQueue_after_WaitForSignalWindows:return streamingUnitQueuePtr=%p)\n", GetCurrentThreadId(), streamingCommandQueuePtr);
        return streamingCommandQueuePtr;
    }
    void SwitchStreamingCommandQueues_and_AcquireBothQueueMutexes();///<should only be called by asset loading thread

    inline StreamingCommandQueue* GetMainThreadStreamingCommandQueue() { return   &m_streamingCommandQueues[m_mainThreadStreamingCommandQueue0 ? 0 : 1]; }///<should only be directly called by asset loading thread or by other methods in this class
    inline StreamingCommandQueue* GetAssetLoadingStreamingCommandQueue() { return &m_streamingCommandQueues[m_mainThreadStreamingCommandQueue0 ? 1 : 0]; }///<should only be directly called by asset loading thread or by other methods in this class

private:
    ArraySafe<StreamingCommandQueue, 2> m_streamingCommandQueues;
    HANDLE m_mainThread_StreamingCommandQueue_IsIndex0_Mutex;
    bool m_mainThreadStreamingCommandQueue0;
};
struct AssetLoadingThreadData
{
    ThreadHandles m_handles;
    StreamingCommandQueueManager m_streamingCommandQueue;
    AssetLoadingArguments::ThreadCommand m_threadCommand;
};

void StreamingCommandsProcess(
    AssetLoadingArguments*const threadArgumentsPtr,
    AssetLoadingPersistentResources*const assetLoadingPersistentResourcesPtr);

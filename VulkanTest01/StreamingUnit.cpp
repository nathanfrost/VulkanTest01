#include"StreamingUnit.h"
#include"ntf_vulkan.h"
#include"StreamingUnitManager.h"

extern FILE*s_streamingDebug;
extern HANDLE s_streamingDebugMutex;
#if NTF_DEBUG
extern bool s_allowedToIssueStreamingCommands;
#endif//#if NTF_DEBUG

void StreamingUnitAddToLoadMutexed(
	StreamingUnitRuntime*const streamingUnitToLoadPtr, 
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
    const HANDLE streamingUnitsAddToLoadMutex)
{
    assert(s_allowedToIssueStreamingCommands);
	NTF_REF(streamingUnitToLoadPtr, streamingUnitToLoad);
	VectorSafe<StreamingUnitRuntime*, 1> temp;
	temp.Push(&streamingUnitToLoad);
	StreamingUnitsAddToLoadMutexed(&temp, streamingUnitsAddToLoad, streamingUnitsAddToLoadMutex);
}
void StreamingUnitsAddToLoadMutexed(
	VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToLoad,///<not ConstVectorSafeRef because I want to force the passer to use '&', since semantically want to emphasize that the streaming units contained in the vector will be modified, even if the vector itself will not
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
	const HANDLE streamingUnitsAddToLoadMutex)
{
    assert(s_allowedToIssueStreamingCommands);
	for (auto& streamingUnitToLoadPtr : streamingUnitsToLoad)
	{
		NTF_REF(streamingUnitToLoadPtr, streamingUnitToLoad);

        WaitForSignalWindows(streamingUnitToLoad.m_stateMutex);
        const bool wasUnloadedState = streamingUnitToLoad.m_state == StreamingUnitRuntime::State::kUnloaded;
#if !NTF_UNIT_TEST_STREAMING
        assert(wasUnloadedState);
#endif//#if !NTF_UNIT_TEST_STREAMING
        if (wasUnloadedState)
        {
            streamingUnitToLoad.m_state = StreamingUnitRuntime::State::kLoading;
            MutexRelease(streamingUnitToLoad.m_stateMutex);
            //printf("MAIN THREAD: StreamingUnitsAddToLoadMutexed(%s)\n", streamingUnitToLoad.m_filenameNoExtension.data());

            WaitForSignalWindows(streamingUnitsAddToLoadMutex);
            streamingUnitsAddToLoad.PushIfUnique(&streamingUnitToLoad);
            MutexRelease(streamingUnitsAddToLoadMutex);
        }
        else
        {
            MutexRelease(streamingUnitToLoad.m_stateMutex);
        }
	}
}
void AssetLoadingThreadExecuteLoad(AssetLoadingArgumentsThreadCommand*const threadCommandPtr, const HANDLE assetLoadingThreadWakeHandle)
{
    assert(s_allowedToIssueStreamingCommands);
    *threadCommandPtr = AssetLoadingArgumentsThreadCommand::kProcessStreamingUnits;

#if NTF_UNIT_TEST_STREAMING_LOG
    WaitForSignalWindows(s_streamingDebugMutex);
    FwriteSnprintf(s_streamingDebug, "%s:%i:about to call AssetLoadingThreadExecuteLoad()::SignalSemaphoreWindows(assetLoadingThreadWakeHandle)\n", __FILE__, __LINE__);
    ReleaseMutex(s_streamingDebugMutex);
#endif//#if NTF_UNIT_TEST_STREAMING_LOG

    SignalSemaphoreWindows(assetLoadingThreadWakeHandle);

#if NTF_UNIT_TEST_STREAMING_LOG
    WaitForSignalWindows(s_streamingDebugMutex);
    FwriteSnprintf(s_streamingDebug, "%s:%i:returned from AssetLoadingThreadExecuteLoad()::SignalSemaphoreWindows(assetLoadingThreadWakeHandle)\n", __FILE__, __LINE__);
    ReleaseMutex(s_streamingDebugMutex);
#endif//#if NTF_UNIT_TEST_STREAMING_LOG
}
void StreamingUnitAddToUnload(
	StreamingUnitRuntime*const streamingUnitToUnloadPtr,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsRenderable,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToUnload)
{
    assert(s_allowedToIssueStreamingCommands);
	VectorSafe<StreamingUnitRuntime*, 1> temp;
	
	NTF_REF(streamingUnitToUnloadPtr, streamingUnitToUnload);
#if !NTF_UNIT_TEST_STREAMING
    StreamingCommand lastQueuedStreamingCommand;
    assert( !streamingUnitToUnload.m_streamingCommandQueue.PeekLastQueuedItem(&lastQueuedStreamingCommand) ||
            lastQueuedStreamingCommand == StreamingCommand::kLoad);
#endif//#if !NTF_UNIT_TEST_STREAMING
	temp.Push(&streamingUnitToUnload);
	StreamingUnitsAddToUnload(&temp, streamingUnitsRenderable, streamingUnitsToUnload);
}
void StreamingUnitsAddToUnload(
	VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToAddToUnload,///<not ConstVectorSafeRef because I want to force the passer to use '&', since semantically want to emphasize that the streaming units contained in the vector will be modified, even if the vector itself will not
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsRenderable,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToUnload)
{
    assert(s_allowedToIssueStreamingCommands);
	for(auto& streamingUnitToAddToUnloadListPtr: streamingUnitsToAddToUnload)
	{
		NTF_REF(streamingUnitToAddToUnloadListPtr, streamingUnitToUnload);

        WaitForSignalWindows(streamingUnitToUnload.m_stateMutex);
        const bool streamingUnitCurrentlyLoaded = streamingUnitToUnload.m_state == StreamingUnitRuntime::State::kLoaded;
#if !NTF_UNIT_TEST_STREAMING
        assert(streamingUnitCurrentlyLoaded);
#endif//#if !NTF_UNIT_TEST_STREAMING
        if (streamingUnitCurrentlyLoaded)
        {
            streamingUnitsRenderable.Remove(&streamingUnitToUnload);
            streamingUnitsToUnload.PushIfUnique(&streamingUnitToUnload);
        }
        MutexRelease(streamingUnitToUnload.m_stateMutex);
        //printf("MAIN THREAD: StreamingUnitsAddToUnload(%s)\n", streamingUnitToUnload.m_filenameNoExtension.data());
	}
#if NTF_UNIT_TEST_STREAMING_LOG
    WaitForSignalWindows(s_streamingDebugMutex);
    FwriteSnprintf(s_streamingDebug, "%s:%i:StreamingUnitsAddToUnload():streamingUnitsAddToUnload.size()=%zu\n", __FILE__, __LINE__, streamingUnitsToUnload.size());
    ReleaseMutex(s_streamingDebugMutex);
#endif//#if NTF_UNIT_TEST_STREAMING_LOG
}

StreamingUnitRuntime::State StreamingUnitRuntime::StateMutexed() const
{
    WaitForSignalWindows(m_stateMutex);
    State ret = m_state;
    ReleaseMutex(m_stateMutex);

    return ret;
};

/*static*/ void Vertex::GetAttributeDescriptions(VectorSafeRef<VkVertexInputAttributeDescription> attributeDescriptions)
{
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;///<mirrored in the vertex shader
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;//equivalent to vec3 layout
    attributeDescriptions[0].offset = offsetof(Vertex, pos);//defines address of first byte of the relevant datafield

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
}

StreamingUnitRuntime::StreamingUnitRuntime()
{
}
///Initialize()/Destroy() concern themselves solely with constructs which we don't want to risk fragmenting by creating and destroying
void StreamingUnitRuntime::Initialize(const VkDevice& device)
{
	m_stateMutex = MutexCreate();
    const VkFenceCreateFlagBits fenceCreateFlagBits = static_cast<VkFenceCreateFlagBits>(0);

    FenceCreate(&m_transferQueueFinishedFence, fenceCreateFlagBits, device);
    FenceCreate(&m_graphicsQueueFinishedFence, fenceCreateFlagBits, device);
}

///concerns itself solely with Vulkan constructs that we memory-manage, not OS constructs like mutexes
void StreamingUnitRuntime::Free(
    ArraySafeRef<bool> deviceLocalMemoryStreamingUnitsAllocated,
    ConstVectorSafeRef<VulkanPagedStackAllocator> deviceLocalMemoryStreamingUnits,
    const HANDLE deviceLocalMemoryMutex,
    const bool deallocateBackToGpu,
    const VkDevice& device)
{
    //printf("StreamingUnitRuntime::Free() enter\n");//#LogStreaming
    vkDestroySampler(device, m_textureSampler, GetVulkanAllocationCallbacks());

    for (auto& texturedGeometry : m_texturedGeometries)
    {
        vkDestroyImage(device, texturedGeometry.textureImage, GetVulkanAllocationCallbacks());
        vkDestroyBuffer(device, texturedGeometry.indexBuffer, GetVulkanAllocationCallbacks());
        vkDestroyBuffer(device, texturedGeometry.vertexBuffer, GetVulkanAllocationCallbacks());
    }

    DestroyUniformBuffer(m_uniformBufferCpuMemory, m_uniformBufferGpuMemory, m_uniformBuffer, device);
    for (auto& imageView : m_textureImageViews)
    {
        vkDestroyImageView(device, imageView, GetVulkanAllocationCallbacks()); 
    }

    vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, GetVulkanAllocationCallbacks());
    vkDestroyDescriptorPool(device, m_descriptorPool, GetVulkanAllocationCallbacks());
    vkDestroyPipelineLayout(device, m_pipelineLayout, GetVulkanAllocationCallbacks());
    vkDestroyPipeline(device, m_graphicsPipeline, GetVulkanAllocationCallbacks());

    FenceReset(m_transferQueueFinishedFence, device);
    FenceReset(m_graphicsQueueFinishedFence, device);

    //release allocator back to pool
    WaitForSignalWindows(deviceLocalMemoryMutex);
    assert(m_deviceLocalMemory);
    NTF_LOG_STREAMING("%i:StreamingUnitRuntime::Free:WaitForSignalWindows(deviceLocalMemoryMutex=%zu)\n", GetCurrentThreadId(), (size_t)deviceLocalMemoryMutex);
    size_t deviceLocalMemoryStreamingUnitsIndex = 0;
    for (auto& deviceLocalMemoryStreamingUnit : deviceLocalMemoryStreamingUnits)
    {
        if (m_deviceLocalMemory == &deviceLocalMemoryStreamingUnit)
        {
            auto& deviceLocalMemoryStreamingUnitAllocated = deviceLocalMemoryStreamingUnitsAllocated[deviceLocalMemoryStreamingUnitsIndex];
            assert(deviceLocalMemoryStreamingUnitAllocated);
            deviceLocalMemoryStreamingUnitAllocated = false;
            m_deviceLocalMemory->FreeAllPages(deallocateBackToGpu, device);
            m_deviceLocalMemory = nullptr;
        }
        ++deviceLocalMemoryStreamingUnitsIndex;
    }
    assert(!m_deviceLocalMemory);
    MutexRelease(deviceLocalMemoryMutex);
    NTF_LOG_STREAMING("%i:StreamingUnitRuntime::Free:MutexRelease(deviceLocalMemoryMutex=%zu)\n", GetCurrentThreadId(), (size_t)deviceLocalMemoryMutex);

    //printf("StreamingUnitRuntime::Free() exit\n");//#LogStreaming
}

///Initialize()/Destroy() concern themselves solely with constructs we don't want to risk fragmenting by creating and destroying)
void StreamingUnitRuntime::Destroy(const VkDevice& device)
{
	WaitForSignalWindows(m_stateMutex);
	HandleCloseWindows(&m_stateMutex);//assume no ReleaseMutex() needed before closing; not sure this is actually true

    vkDestroyFence(device, m_transferQueueFinishedFence, GetVulkanAllocationCallbacks());
    vkDestroyFence(device, m_graphicsQueueFinishedFence, GetVulkanAllocationCallbacks());
}

void StreamingUnitRuntime::AssertValid() const
{
    assert(m_filenameNoExtension.Strnlen() > 0);
}

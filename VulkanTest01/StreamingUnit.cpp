#include"StreamingUnit.h"
#include"ntf_vulkan.h"
#include"StreamingUnitManager.h"

///expected StreamingUnitRuntime::m_streamingCommandQueueMutex to be acquired when called
static void StreamingCommandQueueRationalize(
    StreamingUnitRuntime*const streamingUnitPtr, 
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToUnload,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
    const HANDLE streamingUnitsAddToLoadListMutex)
{
    NTF_REF(streamingUnitPtr, streamingUnit);
    NTF_REF(&streamingUnit.m_streamingCommandQueue, streamingCommandQueue);

    size_t queueSize;
    while ((queueSize = streamingCommandQueue.Size()) >= 2)
    {
        const StreamingCommand nextCommandToDequeue = streamingCommandQueue[queueSize - 1];
        const StreamingCommand subsequentCommandToDequeue = streamingCommandQueue[queueSize - 2];
        if (nextCommandToDequeue == subsequentCommandToDequeue)
        {
#if !NTF_UNIT_TEST_STREAMING
            assert(false);
#endif//#if !NTF_UNIT_TEST_STREAMING
            streamingCommandQueue.Dequeue();//remove duplicate command
        }
        else if (   nextCommandToDequeue == StreamingCommand::kLoad && subsequentCommandToDequeue == StreamingCommand::kUnload ||
                    nextCommandToDequeue == StreamingCommand::kUnload && subsequentCommandToDequeue == StreamingCommand::kLoad)
        {
            //cancel out opposing commands
            streamingCommandQueue.Dequeue();
            streamingCommandQueue.Dequeue();
        }
    }

    
    WaitForSignalWindows(streamingUnitsAddToLoadListMutex);
    if (NextItemToDequeueIs(StreamingCommand::kLoad, streamingCommandQueue))
    {
        streamingUnitsAddToLoad.PushIfUnique(&streamingUnit);
    }
    else
    {
        streamingUnitsAddToLoad.Remove(&streamingUnit);//loading is not called for
    }
    MutexRelease(streamingUnitsAddToLoadListMutex);

    if (NextItemToDequeueIs(StreamingCommand::kUnload, streamingCommandQueue))
    {
        streamingUnitsAddToUnload.PushIfUnique(&streamingUnit);
    }
    else
    {
        streamingUnitsAddToUnload.Remove(&streamingUnit);//unloading is not called for
    }
}

void StreamingUnitAddToLoadMutexed(
	StreamingUnitRuntime*const streamingUnitToLoadPtr, 
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsRenderable,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToUnload,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
    const HANDLE streamingUnitsAddToLoadListMutex)
{
	NTF_REF(streamingUnitToLoadPtr, streamingUnitToLoad);
	VectorSafe<StreamingUnitRuntime*, 1> temp;
	temp.Push(&streamingUnitToLoad);
	StreamingUnitsAddToLoadMutexed(&temp, streamingUnitsRenderable, streamingUnitsAddToUnload, streamingUnitsAddToLoad, streamingUnitsAddToLoadListMutex);
}
void StreamingUnitsAddToLoadMutexed(
	VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToLoad,///<not ConstVectorSafeRef because I want to force the passer to use '&', since semantically want to emphasize that the streaming units contained in the vector will be modified, even if the vector itself will not
	VectorSafeRef<StreamingUnitRuntime*> streamingUnitsRenderable,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToUnload,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
	const HANDLE streamingUnitsAddToLoadListMutex)
{
	for (auto& streamingUnitToLoadPtr : streamingUnitsToLoad)
	{
		NTF_REF(streamingUnitToLoadPtr, streamingUnitToLoad);

		WaitForSignalWindows(streamingUnitToLoad.m_streamingCommandQueueMutex);
#if !NTF_UNIT_TEST_STREAMING
#if NTF_DEBUG                     
        StreamingCommand lastQueuedStreamingCommand;
        assert( !streamingUnitToLoad.m_streamingCommandQueue.PeekLastQueuedItem(&lastQueuedStreamingCommand) ||
                lastQueuedStreamingCommand == StreamingCommand::kUnload);
#endif//#if NTF_DEBUG
#endif//#if !NTF_UNIT_TEST_STREAMING
        streamingUnitToLoad.m_streamingCommandQueue.Enqueue(StreamingCommand::kLoad);
        StreamingCommandQueueRationalize(
            &streamingUnitToLoad, 
            streamingUnitsAddToUnload, 
            streamingUnitsAddToLoad, 
            streamingUnitsAddToLoadListMutex);

		MutexRelease(streamingUnitToLoad.m_streamingCommandQueueMutex);
        //printf("MAIN THREAD: StreamingUnitsAddToLoadMutexed(%s)\n", streamingUnitToLoad.m_filenameNoExtension.data());
		
		assert(streamingUnitsRenderable.Find(&streamingUnitToLoad) < 0);//loading a unit means it should not be on the render list
	}
}
void AssetLoadingThreadExecuteLoad(AssetLoadingArgumentsThreadCommand*const threadCommandPtr, const HANDLE assetLoadingThreadWakeHandle)
{
    *threadCommandPtr = AssetLoadingArgumentsThreadCommand::kProcessStreamingUnits;
    SignalSemaphoreWindows(assetLoadingThreadWakeHandle);
}
void StreamingUnitAddToUnload(
	StreamingUnitRuntime*const streamingUnitToUnloadPtr,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsRenderable,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToUnload,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
    const HANDLE streamingUnitsAddToLoadListMutex)
{
	VectorSafe<StreamingUnitRuntime*, 1> temp;
	
	NTF_REF(streamingUnitToUnloadPtr, streamingUnitToUnload);
#if !NTF_UNIT_TEST_STREAMING
    StreamingCommand lastQueuedStreamingCommand;
    assert( !streamingUnitToUnload.m_streamingCommandQueue.PeekLastQueuedItem(&lastQueuedStreamingCommand) ||
            lastQueuedStreamingCommand == StreamingCommand::kLoad);
#endif//#if !NTF_UNIT_TEST_STREAMING
	temp.Push(&streamingUnitToUnload);
	StreamingUnitsAddToUnload(&temp, streamingUnitsRenderable, streamingUnitsAddToUnload, streamingUnitsAddToLoad, streamingUnitsAddToLoadListMutex);
}
void StreamingUnitsAddToUnload(
	VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToAddToUnload,///<not ConstVectorSafeRef because I want to force the passer to use '&', since semantically want to emphasize that the streaming units contained in the vector will be modified, even if the vector itself will not
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsRenderable,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToUnloadList,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
    const HANDLE streamingUnitsAddToLoadListMutex)
{
	for(auto& streamingUnitToAddToUnloadListPtr: streamingUnitsToAddToUnload)
	{
		NTF_REF(streamingUnitToAddToUnloadListPtr, streamingUnitToUnload);
		streamingUnitsRenderable.Remove(&streamingUnitToUnload);

        WaitForSignalWindows(streamingUnitToUnload.m_streamingCommandQueueMutex);
        const StreamingCommand unload = StreamingCommand::kUnload;
        streamingUnitToUnload.m_streamingCommandQueue.Enqueue(unload);
        StreamingCommandQueueRationalize(
            &streamingUnitToUnload, 
            streamingUnitsAddToUnloadList, 
            streamingUnitsAddToLoad, 
            streamingUnitsAddToLoadListMutex);
        MutexRelease(streamingUnitToUnload.m_streamingCommandQueueMutex);
        //printf("MAIN THREAD: StreamingUnitsAddToUnload(%s)\n", streamingUnitToUnload.m_filenameNoExtension.data());
	}
}

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
	m_streamingCommandQueueMutex = MutexCreate();
    const VkFenceCreateFlagBits fenceCreateFlagBits = static_cast<VkFenceCreateFlagBits>(0);

    FenceCreate(&m_transferQueueFinishedFence, fenceCreateFlagBits, device);
    FenceCreate(&m_graphicsQueueFinishedFence, fenceCreateFlagBits, device);
    m_loaded = false;
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
	WaitForSignalWindows(m_streamingCommandQueueMutex);
	HandleCloseWindows(&m_streamingCommandQueueMutex);//assume no ReleaseMutex() needed before closing; not sure this is actually true

    vkDestroyFence(device, m_transferQueueFinishedFence, GetVulkanAllocationCallbacks());
    vkDestroyFence(device, m_graphicsQueueFinishedFence, GetVulkanAllocationCallbacks());
}

void StreamingUnitRuntime::AssertValid() const
{
    assert(m_filenameNoExtension.Strnlen() > 0);
}

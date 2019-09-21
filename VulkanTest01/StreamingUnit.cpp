#include"StreamingUnit.h"
#include"ntf_vulkan.h"
#include"StreamingUnitManager.h"

void StreamingCommandsStart(
    VectorSafeRef<StreamingCommand> streamingCommands, 
    StreamingCommandQueueManager*const streamingCommandQueueManagerPtr,
    const HANDLE assetLoadingThreadWakeHandle)
{
    NTF_REF(streamingCommandQueueManagerPtr, streamingCommandQueueManager);
    assert(assetLoadingThreadWakeHandle);

    auto& streamingCommandQueue = *streamingCommandQueueManager.GetMainThreadStreamingCommandQueue_after_WaitForSignalWindows();
    for (auto streamingCommand : streamingCommands)///<@todo: could make a memcpy, but performance difference should be negligible with a small number of streaming commands
    {
        NTF_REF(streamingCommand.m_streamingUnit, streamingUnit);
        if (streamingCommand.m_command == StreamingCommand::kLoad && 
            streamingUnit.m_unloadAsSoonAsGpuIsDoneRendering && 
            streamingUnit.StateMutexed() == StreamingUnitRuntime::kReady)
        {
            /*  streaming unit is still in memory, and the main thread is waiting for the opportunity to free that streaming unit's memory -- just 
                start rendering the streaming unit again, and stop waiting to free its memory */
            streamingUnit.m_unloadAsSoonAsGpuIsDoneRendering = false;
        }
        else
        {
            if (streamingUnit.StateMutexed() == StreamingUnitRuntime::kNotLoaded && streamingCommand.m_command == StreamingCommand::kLoad)
            {
                streamingCommand.m_streamingUnit->m_renderedOnceSinceLastLoad = false;
            }
            streamingCommandQueue.m_queue.Enqueue(streamingCommand);
            UnsignalSemaphoreWindows(streamingCommandQueue.m_streamingCommandsDoneHandle);
            NTF_LOG_STREAMING("%i:StreamingCommandsStart:UnsignalSemaphoreWindows():streamingCommandQueue=%p->m_streamingCommandsDoneLoadingHandle=%zu\n",
                GetCurrentThreadId(), &streamingCommandQueue, (size_t)streamingCommandQueue.m_streamingCommandsDoneHandle);
        }
    }
    streamingCommandQueueManager.Release(&streamingCommandQueue);
    SignalSemaphoreWindows(assetLoadingThreadWakeHandle);
    NTF_LOG_STREAMING("%i:SignalSemaphoreWindows():assetLoadingThreadWakeHandle=%zu\n", GetCurrentThreadId(), (size_t)assetLoadingThreadWakeHandle);
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
    m_stateMutexed.Initialize();
    const VkFenceCreateFlagBits fenceCreateFlagBits = static_cast<VkFenceCreateFlagBits>(0);

    FenceCreate(&m_transferQueueFinishedFence, fenceCreateFlagBits, device);
    FenceCreate(&m_graphicsQueueFinishedFence, fenceCreateFlagBits, device);
    m_unloadAsSoonAsGpuIsDoneRendering = false;
}

StreamingUnitRuntime::StateMutexed::StateMutexed()
{
#if NTF_DEBUG
    m_initialized = false;
#endif//#if NTF_DEBUG
}
void StreamingUnitRuntime::StateMutexed::Initialize()
{
#if NTF_DEBUG
    assert(!m_initialized);
#endif//#if NTF_DEBUG
    m_mutex = MutexCreate();

#if NTF_DEBUG
    m_initialized = true;
#endif//#if NTF_DEBUG
    Set(kNotLoaded);
}

void StreamingUnitRuntime::StateMutexed::Destroy()
{
#if NTF_DEBUG
    assert(m_initialized);
    m_initialized = false;
#endif//#if NTF_DEBUG
    WaitForSignalWindows(m_mutex);
    HandleCloseWindows(&m_mutex);//assume no ReleaseMutex() needed before closing; not sure this is actually true
}

StreamingUnitRuntime::State StreamingUnitRuntime::StateMutexed::Get() const
{
#if NTF_DEBUG
    assert(m_initialized);
#endif//#if NTF_DEBUG
    WaitForSignalWindows(m_mutex);
    AssertValid();
    const State ret = m_state;
    //printf("Get: m_state=%i\n", ret);
    MutexRelease(m_mutex);
    return ret;
}
void StreamingUnitRuntime::StateMutexed::Set(const StreamingUnitRuntime::State state)
{
#if NTF_DEBUG
    assert(m_initialized);
#endif//#if NTF_DEBUG
    WaitForSignalWindows(m_mutex);
    m_state = state;
    AssertValid();
    //printf("Set: m_state=%i\n", m_state);
    MutexRelease(m_mutex);
}

///not threadsafe
void StreamingUnitRuntime::StateMutexed::AssertValid() const
{
#if NTF_DEBUG
    assert(m_state >= kFirstValidValue);
    assert(m_state <= kLastValidValue);
#endif//#if NTF_DEBUG
}

StreamingUnitRuntime::State StreamingUnitRuntime::StateMutexed() const
{
    return m_stateMutexed.Get();
}
void StreamingUnitRuntime::StateMutexed(const StreamingUnitRuntime::State state)
{
	m_stateMutexed.Set(state);
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

    assert(StateMutexed() == State::kUnloading);
    assert(m_unloadAsSoonAsGpuIsDoneRendering == false);//this should have been set to true before issuing the command

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
    assert(m_deviceLocalMemory);
    WaitForSignalWindows(deviceLocalMemoryMutex);
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
    StateMutexed(State::kNotLoaded);
}

///Initialize()/Destroy() concern themselves solely with constructs we don't want to risk fragmenting by creating and destroying)
void StreamingUnitRuntime::Destroy(const VkDevice& device)
{
    m_stateMutexed.Destroy();
    vkDestroyFence(device, m_transferQueueFinishedFence, GetVulkanAllocationCallbacks());
    vkDestroyFence(device, m_graphicsQueueFinishedFence, GetVulkanAllocationCallbacks());
}

void StreamingUnitRuntime::AssertValid() const
{
    assert(m_filenameNoExtension.Strnlen() > 0);
    assert(StateMutexed() >= kFirstValidValue);
    assert(StateMutexed() <= kLastValidValue);
}

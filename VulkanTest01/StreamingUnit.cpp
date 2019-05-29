#include"StreamingUnit.h"
#include"ntf_vulkan.h"

void StreamingUnitLoadStart(
    StreamingUnitRuntime*const streamingUnitPtr, 
    StreamingUnitRuntime**const assetLoadingThreadStreamingUnitPtrPtr,
    VectorSafeRef<VulkanPagedStackAllocator> deviceLocalMemoryStreamingUnits,
    ArraySafeRef<bool> deviceLocalMemoryStreamingUnitsAllocated,
    const HANDLE assetLoadingThreadWakeHandle, 
    const HANDLE assetLoadingThreadStreamingUnitsDoneLoading)
{
    NTF_REF(assetLoadingThreadStreamingUnitPtrPtr, assetLoadingThreadStreamingUnitPtr);
    NTF_REF(streamingUnitPtr, streamingUnit);
    assert(assetLoadingThreadWakeHandle);

    assetLoadingThreadStreamingUnitPtr = &streamingUnit;
    const size_t deviceLocalMemoryStreamingUnitsSize = deviceLocalMemoryStreamingUnits.size();
    size_t deviceLocalMemoryStreamingUnitIndex = 0;
    for (; deviceLocalMemoryStreamingUnitIndex < deviceLocalMemoryStreamingUnitsSize; ++deviceLocalMemoryStreamingUnitIndex)
    {
        auto& deviceLocalMemoryStreamingUnitAllocated = deviceLocalMemoryStreamingUnitsAllocated[deviceLocalMemoryStreamingUnitIndex];
        if (!deviceLocalMemoryStreamingUnitAllocated)
        {
            deviceLocalMemoryStreamingUnitAllocated = true;
            streamingUnit.m_deviceLocalMemory = &deviceLocalMemoryStreamingUnits[deviceLocalMemoryStreamingUnitIndex];
            break;
        }
    }
    assert(deviceLocalMemoryStreamingUnitIndex < deviceLocalMemoryStreamingUnitsSize);
    streamingUnit.StateMutexed(StreamingUnitRuntime::kLoading);

    UnsignalSemaphoreWindows(assetLoadingThreadStreamingUnitsDoneLoading);///@todo: double-buffered streaming unit queue for 100% robustness: when you do that; get rid of this handle and instead check the double-buffered queue for being empty
    SignalSemaphoreWindows(assetLoadingThreadWakeHandle);
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
/** Initialize()/Destroy() concern themselves solely with OS constructs like mutexes (which I don't want to risk fragmenting by creating and
    destroying), but not Vulkan constructs that we memory-manage */
void StreamingUnitRuntime::Initialize()
{
    m_stateMutexed.Initialize();
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
    ReleaseMutex(m_mutex);
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
    ReleaseMutex(m_mutex);
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
    const bool deallocateBackToGpu,
    const VkDevice& device)
{
    //printf("StreamingUnitRuntime::Free() enter\n");

    assert(m_stateMutexed.Get() == State::kUnloading);
    m_stateMutexed.Set(State::kNotLoaded);

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

    vkDestroyFence(device, m_transferQueueFinishedFence, GetVulkanAllocationCallbacks());
    vkDestroyFence(device, m_graphicsQueueFinishedFence, GetVulkanAllocationCallbacks());

    //release allocator back to pool
    assert(m_deviceLocalMemory);
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

    //printf("StreamingUnitRuntime::Free() exit\n");
}

/** Initialize()/Destroy() concern themselves solely with OS constructs like mutexes (which I don't want to risk fragmenting by creating and 
    destroying), but not Vulkan constructs that we memory-manage */
void StreamingUnitRuntime::Destroy()
{
    m_stateMutexed.Destroy();
}

void StreamingUnitRuntime::AssertValid() const
{
    m_stateMutexed.AssertValid();
}

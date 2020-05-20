#include"StreamingUnit.h"

#include"ntf_vulkan.h"
#include"StreamingUnitManager.h"
#include"WindowsUtil.h"

#if NTF_DEBUG
extern bool s_allowedToIssueStreamingCommands;
#endif//#if NTF_DEBUG

void StreamingUnitAddToLoadCriticalSection(
	StreamingUnitRuntime*const streamingUnitToLoadPtr, 
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
    RTL_CRITICAL_SECTION*const streamingUnitsAddToLoadCriticalSectionPtr)
{
    assert(s_allowedToIssueStreamingCommands);
	NTF_REF(streamingUnitToLoadPtr, streamingUnitToLoad);
    NTF_REF(streamingUnitsAddToLoadCriticalSectionPtr, streamingUnitsAddToLoadCriticalSection);

	VectorSafe<StreamingUnitRuntime*, 1> temp;
	temp.Push(&streamingUnitToLoad);
	StreamingUnitsAddToLoadCriticalSection(&temp, &streamingUnitsAddToLoad, &streamingUnitsAddToLoadCriticalSection);
}
void StreamingUnitsAddToLoadCriticalSection(
	VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToLoad,///<not ConstVectorSafeRef because I want to force the passer to use '&', since semantically want to emphasize that the streaming units contained in the vector will be modified, even if the vector itself will not
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
    RTL_CRITICAL_SECTION*const streamingUnitsAddToLoadCriticalSectionPtr)
{
    assert(s_allowedToIssueStreamingCommands);
    NTF_REF(streamingUnitsAddToLoadCriticalSectionPtr, streamingUnitsAddToLoadCriticalSection);

	for (auto& streamingUnitToLoadPtr : streamingUnitsToLoad)
	{
		NTF_REF(streamingUnitToLoadPtr, streamingUnitToLoad);

        CriticalSectionEnter(&streamingUnitToLoad.m_stateCriticalSection);

        const bool wasUnloadedState = streamingUnitToLoad.m_state == StreamingUnitRuntime::State::kUnloaded;
        NTF_LOG_STREAMING("%s:%i:StreamingUnitsAddToLoadCriticalSection():%s.m_state=%zu\n",__FILE__, __LINE__, streamingUnitToLoad.m_filenameNoExtension.data(), streamingUnitToLoad.m_state);
#if !NTF_UNIT_TEST_STREAMING
        assert(wasUnloadedState);
#endif//#if !NTF_UNIT_TEST_STREAMING
        if (wasUnloadedState)
        {
            streamingUnitToLoad.m_state = StreamingUnitRuntime::State::kLoading;
            NTF_LOG_STREAMING("%s:%i:StreamingUnitsAddToLoadCriticalSection():%s.m_state=%zu\n", __FILE__, __LINE__, streamingUnitToLoad.m_filenameNoExtension.data(), streamingUnitToLoad.m_state);
            CriticalSectionLeave(&streamingUnitToLoad.m_stateCriticalSection);

            CriticalSectionEnter(&streamingUnitsAddToLoadCriticalSection);
            streamingUnitsAddToLoad.PushIfUnique(&streamingUnitToLoad);
            CriticalSectionLeave(&streamingUnitsAddToLoadCriticalSection);
        }
        else
        {
            CriticalSectionLeave(&streamingUnitToLoad.m_stateCriticalSection);
        }
	}
}
void AssetLoadingThreadExecuteLoad(AssetLoadingArgumentsThreadCommand*const threadCommandPtr, const HANDLE assetLoadingThreadWakeHandle)
{
    assert(s_allowedToIssueStreamingCommands);
    *threadCommandPtr = AssetLoadingArgumentsThreadCommand::kProcessStreamingUnits;

    NTF_LOG_STREAMING(  "%s:%i:about to call AssetLoadingThreadExecuteLoad()::SignalSemaphoreWindows(assetLoadingThreadWakeHandle)\n", 
                        __FILE__, __LINE__);

    SignalSemaphoreWindows(assetLoadingThreadWakeHandle);

    NTF_LOG_STREAMING(  "%s:%i:returned from AssetLoadingThreadExecuteLoad()::SignalSemaphoreWindows(assetLoadingThreadWakeHandle)\n",
                        __FILE__, __LINE__);
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

        CriticalSectionEnter(&streamingUnitToUnload.m_stateCriticalSection);
        const bool streamingUnitCurrentlyLoaded = streamingUnitToUnload.m_state == StreamingUnitRuntime::State::kLoaded;
        NTF_LOG_STREAMING("%s:%i:StreamingUnitsAddToUnload():%s.m_state=%zu\n", __FILE__, __LINE__, streamingUnitToUnload.m_filenameNoExtension.data(), streamingUnitToUnload.m_state);
#if !NTF_UNIT_TEST_STREAMING
        assert(streamingUnitCurrentlyLoaded);
#endif//#if !NTF_UNIT_TEST_STREAMING
        if (streamingUnitCurrentlyLoaded)
        {
            streamingUnitsRenderable.Remove(&streamingUnitToUnload);
            streamingUnitsToUnload.PushIfUnique(&streamingUnitToUnload);
        }
        CriticalSectionLeave(&streamingUnitToUnload.m_stateCriticalSection);
	}
    NTF_LOG_STREAMING(  "%s:%i:StreamingUnitsAddToUnload():streamingUnitsAddToUnload.size()=%zu\n", 
                        __FILE__, __LINE__, streamingUnitsToUnload.size());
}

StreamingUnitRuntime::State StreamingUnitRuntime::StateCriticalSection()
{
    CriticalSectionEnter(&m_stateCriticalSection);
    const State ret = m_state;
    NTF_LOG_STREAMING("%s:%i:StreamingUnitRuntime::StateCriticalSection():%s.m_state=%zu\n", __FILE__, __LINE__, m_filenameNoExtension.data(), m_state);
    CriticalSectionLeave(&m_stateCriticalSection);

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
	CriticalSectionCreate(&m_stateCriticalSection);
    const VkFenceCreateFlagBits fenceCreateFlagBits = static_cast<VkFenceCreateFlagBits>(0);

    FenceCreate(&m_transferQueueFinishedFence, fenceCreateFlagBits, device);
    FenceCreate(&m_graphicsQueueFinishedFence, fenceCreateFlagBits, device);
}

///concerns itself solely with Vulkan constructs that we memory-manage, not OS constructs like critical sections
void StreamingUnitRuntime::Free(
    ArraySafeRef<bool> deviceLocalMemoryStreamingUnitsAllocated,
    RTL_CRITICAL_SECTION*const deviceLocalMemoryCriticalSectionPtr,
    const ConstVectorSafeRef<VulkanPagedStackAllocator>& deviceLocalMemoryStreamingUnits,
    const bool deallocateBackToGpu,
    const VkDevice& device)
{
    NTF_REF(deviceLocalMemoryCriticalSectionPtr, deviceLocalMemoryCriticalSection);

    NTF_LOG_STREAMING("%s:%i:StreamingUnitRuntime::Free():streaming unit %s\n",
        __FILE__, __LINE__, m_filenameNoExtension.data());

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
    CriticalSectionEnter(&deviceLocalMemoryCriticalSection);
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
    CriticalSectionLeave(&deviceLocalMemoryCriticalSection);

    CriticalSectionEnter(&m_stateCriticalSection);
    m_state = StreamingUnitRuntime::State::kUnloaded;
    NTF_LOG_STREAMING("%s:%i:StreamingUnitRuntime::Free():%s.m_state=%zu\n", __FILE__, __LINE__, m_filenameNoExtension.data(), m_state);
    CriticalSectionLeave(&m_stateCriticalSection);

    NTF_LOG_STREAMING(  "%s:%i:StreamingUnitRuntime::Free():streaming unit %s\n",
                        __FILE__, __LINE__, m_filenameNoExtension.data());
}

///Initialize()/Destroy() concern themselves solely with constructs we don't want to risk fragmenting by creating and destroying)
void StreamingUnitRuntime::Destroy(const VkDevice& device)
{
    CriticalSectionDelete(&m_stateCriticalSection);//assume no CriticalSectionLeave() is needed; undefined behavior otherwise!

    vkDestroyFence(device, m_transferQueueFinishedFence, GetVulkanAllocationCallbacks());
    vkDestroyFence(device, m_graphicsQueueFinishedFence, GetVulkanAllocationCallbacks());
}

void StreamingUnitRuntime::AssertValid() const
{
    assert(strlen(m_filenameNoExtension.data()) > 0);
}

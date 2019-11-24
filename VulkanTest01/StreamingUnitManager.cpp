#include"StreamingUnitManager.h"
#include"StreamingUnit.h"

//extern LARGE_INTEGER g_queryPerformanceFrequency;

#define NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE (128 * 1024 * 1024)

void AssetLoadingThreadPersistentResourcesCreate(
    AssetLoadingPersistentResources*const assetLoadingPersistentResourcesPtr, 
    VulkanPagedStackAllocator*const deviceLocalMemoryPersistentPtr,
    const VkPhysicalDevice& physicalDevice, 
    const VkDevice& device)
{
    NTF_REF(assetLoadingPersistentResourcesPtr, assetLoadingPersistentResources);
    NTF_REF(deviceLocalMemoryPersistentPtr, deviceLocalMemoryPersistent);

    NTF_REF(&assetLoadingPersistentResources.shaderLoadingScratchSpace, shaderLoadingScratchSpace);
    NTF_REF(&assetLoadingPersistentResources.stagingBufferGpu, stagingBufferGpu);
    NTF_REF(&assetLoadingPersistentResources.stagingBufferGpuMemory, stagingBufferGpuMemory);
    NTF_REF(&assetLoadingPersistentResources.offsetToFirstByteOfStagingBuffer, offsetToFirstByteOfStagingBuffer);
    NTF_REF(&assetLoadingPersistentResources.stagingBufferGpuAlignmentStandard, stagingBufferGpuAlignmentStandard);
    NTF_REF(&assetLoadingPersistentResources.stagingBufferMemoryMapCpuToGpu, stagingBufferMemoryMapCpuToGpu);
    NTF_REF(&assetLoadingPersistentResources.transferFinishedSemaphore, transferFinishedSemaphore);

    const size_t stackAllocatorHackMemorySizeBytes = 64 * 1024 * 1024;
    static StreamingUnitByte stackAllocatorHackMemory[stackAllocatorHackMemorySizeBytes];
    shaderLoadingScratchSpace.Initialize(&stackAllocatorHackMemory[0], stackAllocatorHackMemorySizeBytes);

    const VkDeviceSize stagingBufferCpuToGpuSizeAligned = AlignToNonCoherentAtomSize(NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE);
    CreateBuffer(
        &stagingBufferGpu,
        &stagingBufferGpuMemory,
        &deviceLocalMemoryPersistent,
        &offsetToFirstByteOfStagingBuffer,
        stagingBufferCpuToGpuSizeAligned,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        true,///<this buffer will be memory mapped, so respect alignment
        device,
        physicalDevice);
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, stagingBufferGpu, &memRequirements);
    stagingBufferGpuAlignmentStandard = memRequirements.alignment;

    void* stagingBufferMemoryMapCpuToGpuPtr;
    MapMemory(&stagingBufferMemoryMapCpuToGpuPtr, stagingBufferGpuMemory, offsetToFirstByteOfStagingBuffer, stagingBufferCpuToGpuSizeAligned, device);
    stagingBufferMemoryMapCpuToGpu.Initialize(reinterpret_cast<uint8_t*>(stagingBufferMemoryMapCpuToGpuPtr), NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE);

    CreateVulkanSemaphore(&transferFinishedSemaphore, device);
}

void StreamingCommandsProcess(
    AssetLoadingArguments*const threadArgumentsPtr, 
    AssetLoadingPersistentResources*const assetLoadingPersistentResourcesPtr)
{
    NTF_REF(threadArgumentsPtr, threadArguments);

	auto& deviceLocalMemoryStreamingUnits = threadArguments.m_deviceLocalMemoryStreamingUnits;
	auto& deviceLocalMemoryStreamingUnitsAllocated = threadArguments.m_deviceLocalMemoryStreamingUnitsAllocated;
	NTF_REF(&threadArguments.m_streamingUnitsToAddToLoad, streamingUnitsToAddToLoad);
	NTF_REF(&threadArguments.m_streamingUnitsToAddToRenderable, streamingUnitsToAddToRenderable);

    NTF_REF(threadArguments.m_commandBufferTransfer, commandBufferTransfer);
    NTF_REF(threadArguments.m_commandBufferTransitionImage, commandBufferTransitionImage);
    NTF_REF(threadArguments.m_device, device);
    NTF_REF(threadArguments.m_deviceLocalMemoryMutex, deviceLocalMemoryMutex);
    NTF_REF(threadArguments.m_graphicsQueue, graphicsQueue);
    NTF_REF(threadArguments.m_graphicsQueueMutex, graphicsQueueMutex);
    NTF_REF(threadArguments.m_instance, instance);
    NTF_REF(threadArguments.m_physicalDevice, physicalDevice);
    NTF_REF(threadArguments.m_queueFamilyIndices, queueFamilyIndices);
	NTF_REF(threadArguments.m_renderPass, renderPass);
	NTF_REF(threadArguments.m_streamingUnitsAddToLoadListMutex, streamingUnitsAddToLoadListMutex);
	NTF_REF(threadArguments.m_streamingUnitsAddToRenderableMutex, streamingUnitsAddToRenderableMutex);
	NTF_REF(threadArguments.m_swapChainExtent, swapChainExtent);
    NTF_REF(threadArguments.m_transferQueue, transferQueue);


    NTF_REF(assetLoadingPersistentResourcesPtr, assetLoadingPersistentResources);

	NTF_REF(&assetLoadingPersistentResources.offsetToFirstByteOfStagingBuffer, offsetToFirstByteOfStagingBuffer);
    NTF_REF(&assetLoadingPersistentResources.shaderLoadingScratchSpace, shaderLoadingScratchSpace);
    NTF_REF(&assetLoadingPersistentResources.stagingBufferGpu, stagingBufferGpu);
	NTF_REF(&assetLoadingPersistentResources.stagingBufferGpuAlignmentStandard, stagingBufferGpuAlignmentStandard);
    NTF_REF(&assetLoadingPersistentResources.stagingBufferGpuMemory, stagingBufferGpuMemory);
    NTF_REF(&assetLoadingPersistentResources.stagingBufferMemoryMapCpuToGpu, stagingBufferMemoryMapCpuToGpu);
    NTF_REF(&assetLoadingPersistentResources.transferFinishedSemaphore, transferFinishedSemaphore);


    VkDeviceSize stagingBufferGpuOffsetToAllocatedBlock;

    const bool unifiedGraphicsAndTransferQueue = graphicsQueue == transferQueue;
    assert(unifiedGraphicsAndTransferQueue == (queueFamilyIndices.transferFamily == queueFamilyIndices.graphicsFamily));
    const HANDLE*const transferQueueMutex = unifiedGraphicsAndTransferQueue ? &graphicsQueueMutex : nullptr;//if we have a single queue for graphics and transfer rather than two separate queues, then we must be mutex that one queue

    VkPipelineStageFlags transferFinishedPipelineStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VectorSafe<VkBuffer, 32> stagingBuffersGpu;

	VectorSafe<StreamingUnitRuntime*, kStreamingUnitCommandsNum> streamingUnitsToLoad, streamingUnitsToLoadCurrent;

	WaitForSignalWindows(streamingUnitsAddToLoadListMutex);
	streamingUnitsToLoad.Copy(streamingUnitsToAddToLoad);
	streamingUnitsToAddToLoad.size(0);
	ReleaseMutex(streamingUnitsAddToLoadListMutex);
    
    while (streamingUnitsToLoad.size())
    {
        streamingUnitsToLoadCurrent.Copy(streamingUnitsToLoad);
        streamingUnitsToLoad.size(0);//assume all processed, until shown otherwise
        for (auto& streamingUnitToLoadPtr: streamingUnitsToLoadCurrent)
        {
            NTF_REF(streamingUnitToLoadPtr, streamingUnit);
            //printf("streamingUnit:%s\n", streamingUnit.m_filenameNoExtension.data());//#LogStreaming

            WaitForSignalWindows(streamingUnit.m_streamingCommandQueueMutex);
            ///TODO_NEXT: "Peek and maybe process" and "assert and ignore if already loaded" like main thread on unload
            if (NextItemToDequeueIs(StreamingCommand::kLoad, streamingUnit.m_streamingCommandQueue))
            {
                const bool streamingUnitNotLoaded = !streamingUnit.m_loaded;//if the streaming unit wasn't loaded, we can trust it won't become loaded until this thread loads it, so this boolean can be relied upon outside of this mutexed section of code
//#if !NTF_UNIT_TEST_STREAMING
                assert(streamingUnitNotLoaded);//if this assert is ever struck, then comment back in the NTF_UNIT_TEST_STREAMING #ifdef's enclosing this line, and mutex the streaming unit's use of its assigned deviceLocalMemory
//#endif//#if !NTF_UNIT_TEST_STREAMING
                if (streamingUnitNotLoaded)
                {
#if NTF_DEBUG
                    WaitForSignalWindows(streamingUnitsAddToRenderableMutex);
                    assert(streamingUnitsToAddToRenderable.Find(&streamingUnit) < 0);
                    ReleaseMutex(streamingUnitsAddToRenderableMutex);
#endif//NTF_DEBUG
                    streamingUnit.m_submittedToGpuOnceSinceLastLoad = false;//must happen before removing the streaming command below so that if an Unload command is pending after this Load command, the streaming unit is not unloaded before its Gpu resources have been fully loaded
                }
                streamingUnit.m_streamingCommandQueue.Dequeue();//if the streaming unit is loaded we're done with the load command; otherwise we will load it, meaning we're still done with this load command
                ReleaseMutex(streamingUnit.m_streamingCommandQueueMutex);

                if (streamingUnitNotLoaded)
                {
                    assert(stagingBuffersGpu.size() == 0);
                    assert(stagingBufferMemoryMapCpuToGpu.IsEmptyAndAllocated());
                    {
                        //LARGE_INTEGER perfCount;
                        //QueryPerformanceCounter(&perfCount);
                        //printf("ASSET THREAD: AssetLoadingThread loading streaming unit; time=%f\n", static_cast<double>(perfCount.QuadPart)/ static_cast<double>(g_queryPerformanceFrequency.QuadPart));
                    }

#if !NTF_UNIT_TEST_STREAMING
                    assert(streamingUnitNotLoaded);//it is an error to unload a streaming unit that isn't loaded, albeit an error we can probably recover from
#endif//#if !NTF_UNIT_TEST_STREAMING
                    //allocate a memory allocator to the streaming unit
                    WaitForSignalWindows(deviceLocalMemoryMutex);
                    NTF_LOG_STREAMING("%i:StreamingUnitsLoadAllQueued:WaitForSignalWindows(deviceLocalMemoryMutex=%zu)\n", GetCurrentThreadId(), (size_t)deviceLocalMemoryMutex);
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
                    MutexRelease(deviceLocalMemoryMutex);

                    const VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    CreateDescriptorPool(&streamingUnit.m_descriptorPool, descriptorType, device, TODO_REFACTOR_NUM);
                    CreateDescriptorSetLayout(&streamingUnit.m_descriptorSetLayout, descriptorType, device, TODO_REFACTOR_NUM);
                    CreateGraphicsPipeline(
                        &streamingUnit.m_pipelineLayout,
                        &streamingUnit.m_graphicsPipeline,
                        &shaderLoadingScratchSpace,
                        renderPass,
                        streamingUnit.m_descriptorSetLayout,
                        swapChainExtent,
                        device);

                    streamingUnit.m_uniformBufferSizeAligned = AlignToNonCoherentAtomSize(streamingUnit.m_uniformBufferSizeUnaligned);
                    BeginCommandBuffer(commandBufferTransfer, device);
                    if (!unifiedGraphicsAndTransferQueue)
                    {
                        BeginCommandBuffer(commandBufferTransitionImage, device);
                    }
                    CreateTextureSampler(&streamingUnit.m_textureSampler, device);

                    VectorSafe<VkSemaphore, 1> transferFinishedSemaphores;

                    ArraySafe<char, 512> streamingUnitFilePathRelative;
                    streamingUnitFilePathRelative.Snprintf("%s\\%s.%s",
                        CookedFileDirectoryGet(), streamingUnit.m_filenameNoExtension.data(), StreamingUnitFilenameExtensionGet());
                    FILE* streamingUnitFile;

                    Fopen(&streamingUnitFile, streamingUnitFilePathRelative.begin(), "rb");

                    //BEG_GENERALIZE_READER_WRITER
                    StreamingUnitVersion version;
                    StreamingUnitTexturedGeometryNum texturedGeometryNum;
                    Fread(streamingUnitFile, &version, sizeof(version), 1);
                    Fread(streamingUnitFile, &texturedGeometryNum, sizeof(texturedGeometryNum), 1);
                    //END_GENERALIZE_READER_WRITER
                    stagingBufferGpuOffsetToAllocatedBlock = 0;
                    NTF_REF(streamingUnit.m_deviceLocalMemory, deviceLocalMemory);//does not need to be mutexed, because allocation and freeing of GPU deviceLocalMemory is mutexed, and a streaming unit cannot get unloaded if it isn't loaded first, like here
                    for (size_t texturedGeometryIndex = 0; texturedGeometryIndex < texturedGeometryNum; ++texturedGeometryIndex)
                    {
                        //load texture
                        StreamingUnitTextureDimension textureWidth, textureHeight;
                        auto& texturedGeometry = streamingUnit.m_texturedGeometries[texturedGeometryIndex];
                        size_t imageSizeBytes;
                        const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
                        ReadTextureAndCreateImageAndCopyPixelsIfStagingBufferHasSpace(
                            &texturedGeometry.textureImage,
                            &deviceLocalMemory,
                            &textureWidth,
                            &textureHeight,
                            &stagingBufferMemoryMapCpuToGpu,
                            &imageSizeBytes,
                            &stagingBufferGpuOffsetToAllocatedBlock,
                            streamingUnitFile,
                            imageFormat,
                            VK_IMAGE_TILING_OPTIMAL/*could also pass VK_IMAGE_TILING_LINEAR so texels are laid out in row-major order for debugging (less performant)*/,
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT/*accessible by shader*/,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            device,
                            physicalDevice);

                        stagingBuffersGpu.sizeIncrement();
                        CreateBuffer(
                            &stagingBuffersGpu.back(),
                            &stagingBufferGpuOffsetToAllocatedBlock,
                            stagingBufferGpuMemory,
                            offsetToFirstByteOfStagingBuffer,
                            imageSizeBytes,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            device,
                            physicalDevice);

                        TransferImageFromCpuToGpu(
                            texturedGeometry.textureImage,
                            textureWidth,
                            textureHeight,
                            imageFormat,
                            stagingBuffersGpu.back(),
                            commandBufferTransfer,
                            queueFamilyIndices.transferFamily,
                            commandBufferTransitionImage,
                            queueFamilyIndices.graphicsFamily,
                            device,
                            instance);

                        CreateTextureImageView(&streamingUnit.m_textureImageViews[texturedGeometryIndex], texturedGeometry.textureImage, device);
                        {
                            //LARGE_INTEGER perfCount;
                            //QueryPerformanceCounter(&perfCount);
                            //printf("ASSET THREAD: CreateBuffer()=%llu at time %f\n", (uint64_t)stagingBuffersGpu[stagingBufferGpuAllocateIndex - 1], static_cast<double>(perfCount.QuadPart)/ static_cast<double>(g_queryPerformanceFrequency.QuadPart));
                        }

                        //load vertex and index buffer
                        ArraySafeRef<StreamingUnitByte> stagingBufferCpuToGpuVertices;
                        StreamingUnitVerticesNum verticesNum;
                        size_t vertexBufferSizeBytes;
                        VertexBufferSerialize<SerializerRuntimeIn>(
                            streamingUnitFile,
                            &stagingBufferMemoryMapCpuToGpu,
                            &stagingBufferGpuOffsetToAllocatedBlock,
                            &verticesNum,
                            ArraySafeRef<Vertex>(),
                            stagingBufferCpuToGpuVertices,
                            &vertexBufferSizeBytes,
                            stagingBufferGpuAlignmentStandard);
                        CopyBufferToGpuPrepare(
                            &deviceLocalMemory,
                            &texturedGeometry.vertexBuffer,
                            &texturedGeometry.vertexBufferMemory,
                            &stagingBuffersGpu,
                            &stagingBufferGpuOffsetToAllocatedBlock,
                            stagingBufferGpuMemory,
                            stagingBufferGpuAlignmentStandard,
                            offsetToFirstByteOfStagingBuffer,
                            vertexBufferSizeBytes,
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,/*specifies that the buffer is suitable for passing as an element of the pBuffers array to vkCmdBindVertexBuffers*/
                            commandBufferTransfer,
                            device,
                            physicalDevice,
                            instance);

                        ArraySafeRef<StreamingUnitByte> stagingBufferCpuToGpuIndices;
                        StreamingUnitIndicesNum indicesNum;
                        size_t indexBufferSizeBytes;
                        IndexBufferSerialize<SerializerRuntimeIn>(
                            streamingUnitFile,
                            &stagingBufferMemoryMapCpuToGpu,
                            &stagingBufferGpuOffsetToAllocatedBlock,
                            &indicesNum,
                            ArraySafeRef<IndexBufferValue>(),
                            stagingBufferCpuToGpuIndices,
                            &indexBufferSizeBytes,
                            stagingBufferGpuAlignmentStandard);
                        texturedGeometry.indicesSize = CastWithAssert<size_t, uint32_t>(indicesNum);
                        CopyBufferToGpuPrepare(
                            &deviceLocalMemory,
                            &texturedGeometry.indexBuffer,
                            &texturedGeometry.indexBufferMemory,
                            &stagingBuffersGpu,
                            &stagingBufferGpuOffsetToAllocatedBlock,
                            stagingBufferGpuMemory,
                            stagingBufferGpuAlignmentStandard,
                            offsetToFirstByteOfStagingBuffer,
                            indexBufferSizeBytes,
                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                            commandBufferTransfer,
                            device,
                            physicalDevice,
                            instance);
                        {
                            //LARGE_INTEGER perfCount;
                            //QueryPerformanceCounter(&perfCount);
                            //printf("ASSET THREAD: CreateBuffer()=%llu at time %f\n", (uint64_t)stagingBuffersGpu[stagingBufferGpuAllocateIndex-1], static_cast<double>(perfCount.QuadPart)/ static_cast<double>(g_queryPerformanceFrequency.QuadPart));
                        }
                    }
                    Fclose(streamingUnitFile);
                    if (!unifiedGraphicsAndTransferQueue)
                    {
                        transferFinishedSemaphores.Push(transferFinishedSemaphore);
                    }

                    EndCommandBuffer(commandBufferTransfer);
                    SubmitCommandBuffer(
                        transferFinishedSemaphores,
                        ConstVectorSafeRef<VkSemaphore>(),
                        ArraySafeRef<VkPipelineStageFlags>(),
                        commandBufferTransfer,
                        transferQueue,
                        transferQueueMutex,
                        streamingUnit.m_transferQueueFinishedFence,
                        instance);

                    if (!unifiedGraphicsAndTransferQueue)
                    {
                        EndCommandBuffer(commandBufferTransitionImage);
                        SubmitCommandBuffer(
                            ConstVectorSafeRef<VkSemaphore>(),
                            transferFinishedSemaphores,
                            ArraySafeRef<VkPipelineStageFlags>(&transferFinishedPipelineStageFlags, 1),///<@todo: ArraySafeRefConst
                            commandBufferTransitionImage,
                            graphicsQueue,
                            &graphicsQueueMutex,
                            streamingUnit.m_graphicsQueueFinishedFence,
                            instance);
                    }

                    const VkDeviceSize uniformBufferSize = streamingUnit.m_uniformBufferSizeAligned;
                    CreateUniformBuffer(
                        &streamingUnit.m_uniformBufferCpuMemory,
                        &streamingUnit.m_uniformBufferGpuMemory,
                        &streamingUnit.m_uniformBuffer,
                        &deviceLocalMemory,
                        &streamingUnit.m_uniformBufferOffsetToGpuMemory,
                        uniformBufferSize,
                        device,
                        physicalDevice);

                    CreateDescriptorSet(
                        &streamingUnit.m_descriptorSet,
                        descriptorType,
                        streamingUnit.m_descriptorSetLayout,
                        streamingUnit.m_descriptorPool,
                        streamingUnit.m_uniformBuffer,
                        uniformBufferSize,
                        &streamingUnit.m_textureImageViews,///<@todo NTF: @todo: ConstArraySafeRef that does not need ambersand here
                        TODO_REFACTOR_NUM,
                        streamingUnit.m_textureSampler,
                        device);

                    //clean up staging buffers if they were in use but have completed their transfers
                    {
                        FenceWaitUntilSignalled(streamingUnit.m_transferQueueFinishedFence, device);
                        NTF_LOG_STREAMING("%i:FenceWaitUntilSignalled(streamingUnit.m_transferQueueFinishedFence=%zu)\n", GetCurrentThreadId(), (size_t)streamingUnit.m_transferQueueFinishedFence);
                        if (!unifiedGraphicsAndTransferQueue)
                        {
                            FenceWaitUntilSignalled(streamingUnit.m_graphicsQueueFinishedFence, device);
                            NTF_LOG_STREAMING("%i:FenceWaitUntilSignalled(streamingUnit.m_graphicsQueueFinishedFence=%zu)\n", GetCurrentThreadId(), (size_t)streamingUnit.m_graphicsQueueFinishedFence);
                        }

                        //clean up staging memory
                        stagingBufferMemoryMapCpuToGpu.Clear();

                        for (auto& stagingBufferGpu : stagingBuffersGpu)
                        {
                            vkDestroyBuffer(device, stagingBufferGpu, GetVulkanAllocationCallbacks());

                            //LARGE_INTEGER perfCount;
                            //QueryPerformanceCounter(&perfCount);
                            //printf("ASSET THREAD: vkDestroyBuffer(%llu) at time %f\n", (uint64_t)stagingBuffersGpu[stagingBufferGpuAllocateIndexFree], static_cast<double>(perfCount.QuadPart)/ static_cast<double>(g_queryPerformanceFrequency.QuadPart));
                        }
                        stagingBuffersGpu.size(0);
                        //printf("Staging buffers cleaned up\n");
                    }

                    //streaming unit is now loaded so tag it renderable
                    streamingUnit.m_loaded = true;
                    WaitForSignalWindows(streamingUnitsAddToRenderableMutex);
                    streamingUnitsToAddToRenderable.Push(&streamingUnit);
                    ReleaseMutex(streamingUnitsAddToRenderableMutex);
                }//if (streamingUnitNotLoaded)
                //printf("ASSET THREAD: streamingUnit.Load('%s') completed\n", streamingUnit.m_filenameNoExtension.data());
            }//(NextItemToDequeueIs(StreamingCommand::kLoad, streamingUnit.m_streamingCommandQueue))
            else
            {
                ReleaseMutex(streamingUnit.m_streamingCommandQueueMutex);
                if (streamingUnit.m_streamingCommandQueue.Find(StreamingCommand::kLoad) >= 0)
                {
                    streamingUnitsToLoad.Push(&streamingUnit);//can't be processed yet; try again next loop
                }
                //else//streaming unit no longer wants to be loaded, so don't
            }
        }//for (auto& streamingUnitToLoadPtr: streamingUnitsToLoadCurrent)
    }
}

void AssetLoadingPersistentResourcesDestroy(
    AssetLoadingPersistentResources*const assetLoadingPersistentResourcesPtr, 
    const HANDLE& threadDone, 
    const VkDevice& device)
{
    NTF_REF(assetLoadingPersistentResourcesPtr, assetLoadingPersistentResources);

    assetLoadingPersistentResources.shaderLoadingScratchSpace.Destroy();

    vkUnmapMemory(device, assetLoadingPersistentResources.stagingBufferGpuMemory);
    vkDestroyBuffer(device, assetLoadingPersistentResources.stagingBufferGpu, GetVulkanAllocationCallbacks());

    assetLoadingPersistentResources.stagingBufferMemoryMapCpuToGpu.Destroy();
    vkDestroySemaphore(device, assetLoadingPersistentResources.transferFinishedSemaphore, GetVulkanAllocationCallbacks());

    SignalSemaphoreWindows(threadDone);
}
DWORD WINAPI AssetLoadingThread(void* arg)
{
    auto& threadArguments = *reinterpret_cast<AssetLoadingArguments*>(arg);
    threadArguments.AssertValid();

    NTF_REF(threadArguments.m_deviceLocalMemoryPersistent, deviceLocalMemoryPersistent);
    NTF_REF(threadArguments.m_device, device);
    NTF_REF(threadArguments.m_physicalDevice, physicalDevice);
    NTF_REF(threadArguments.m_threadCommand, threadCommand);
    NTF_REF(threadArguments.m_threadDone, threadDone);
    NTF_REF(threadArguments.m_threadWake, threadWake);

    AssetLoadingPersistentResources assetLoadingPersistentResources;
    AssetLoadingThreadPersistentResourcesCreate(&assetLoadingPersistentResources, &deviceLocalMemoryPersistent, physicalDevice, device);

    for (;;)
    {
        //#Wait
        //WaitOnAddress(&signalMemory, &undesiredValue, sizeof(AssetLoadingArguments::SignalMemoryType), INFINITE);//#SynchronizationWindows8+Only
        WaitForSignalWindows(threadWake);
        NTF_LOG_STREAMING("%i:AssetLoadingThread():threadWake\n", GetCurrentThreadId());

        if (threadCommand == AssetLoadingArgumentsThreadCommand::kCleanupAndTerminate)
        {
            break;
        }

        assert(threadCommand == AssetLoadingArgumentsThreadCommand::kProcessStreamingUnits);
        StreamingCommandsProcess(&threadArguments, &assetLoadingPersistentResources);
    }

    AssetLoadingPersistentResourcesDestroy(&assetLoadingPersistentResources, threadDone, device);
    return 0;
}

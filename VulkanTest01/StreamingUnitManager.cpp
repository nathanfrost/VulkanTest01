#include"StreamingUnitManager.h"

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

void StreamingUnitsLoadAllQueued(
    AssetLoadingArguments*const threadArgumentsPtr, 
    AssetLoadingPersistentResources*const assetLoadingPersistentResourcesPtr)
{
    NTF_REF(threadArgumentsPtr, threadArguments);

    NTF_REF(threadArguments.m_commandBufferTransfer, commandBufferTransfer);
    NTF_REF(threadArguments.m_commandBufferTransitionImage, commandBufferTransitionImage);
    NTF_REF(threadArguments.m_device, device);
    NTF_REF(threadArguments.m_deviceLocalMemoryMutex, deviceLocalMemoryMutex);
    NTF_REF(threadArguments.m_deviceLocalMemoryPersistent, deviceLocalMemoryPersistent);
    auto& deviceLocalMemoryStreamingUnits = threadArguments.m_deviceLocalMemoryStreamingUnits;
    auto& deviceLocalMemoryStreamingUnitsAllocated = threadArguments.m_deviceLocalMemoryStreamingUnitsAllocated;
    NTF_REF(threadArguments.m_graphicsQueue, graphicsQueue);
    NTF_REF(threadArguments.m_graphicsQueueMutex, graphicsQueueMutex);
    NTF_REF(threadArguments.m_instance, instance);
    NTF_REF(threadArguments.m_physicalDevice, physicalDevice);
    NTF_REF(threadArguments.m_queueFamilyIndices, queueFamilyIndices);
    NTF_REF(threadArguments.m_streamingUnitLoadQueueManager, streamingUnitLoadQueueManager);
    auto& threadCommand = *threadArguments.m_threadCommand;
    NTF_REF(threadArguments.m_threadDone, threadDone);
    NTF_REF(threadArguments.m_threadWake, threadWake);
    NTF_REF(threadArguments.m_transferQueue, transferQueue);

    NTF_REF(threadArguments.m_renderPass, renderPass);
    NTF_REF(threadArguments.m_swapChainExtent, swapChainExtent);


    NTF_REF(assetLoadingPersistentResourcesPtr, assetLoadingPersistentResources);

    NTF_REF(&assetLoadingPersistentResources.shaderLoadingScratchSpace, shaderLoadingScratchSpace);
    NTF_REF(&assetLoadingPersistentResources.stagingBufferGpu, stagingBufferGpu);
    NTF_REF(&assetLoadingPersistentResources.stagingBufferGpuMemory, stagingBufferGpuMemory);
    NTF_REF(&assetLoadingPersistentResources.offsetToFirstByteOfStagingBuffer, offsetToFirstByteOfStagingBuffer);
    NTF_REF(&assetLoadingPersistentResources.stagingBufferGpuAlignmentStandard, stagingBufferGpuAlignmentStandard);
    NTF_REF(&assetLoadingPersistentResources.stagingBufferMemoryMapCpuToGpu, stagingBufferMemoryMapCpuToGpu);
    NTF_REF(&assetLoadingPersistentResources.transferFinishedSemaphore, transferFinishedSemaphore);


    VkDeviceSize stagingBufferGpuOffsetToAllocatedBlock;

    const bool unifiedGraphicsAndTransferQueue = graphicsQueue == transferQueue;
    assert(unifiedGraphicsAndTransferQueue == (queueFamilyIndices.transferFamily == queueFamilyIndices.graphicsFamily));
    const HANDLE*const transferQueueMutex = unifiedGraphicsAndTransferQueue ? &graphicsQueueMutex : nullptr;//if we have a single queue for graphics and transfer rather than two separate queues, then we must be mutex that one queue

    assert(threadCommand == AssetLoadingArguments::ThreadCommand::kLoadStreamingUnit);

    VkPipelineStageFlags transferFinishedPipelineStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VectorSafe<VkBuffer, 32> stagingBuffersGpu;

    //main thread should have enqueued one or more streaming units on its queue, so get access to that queue
    streamingUnitLoadQueueManager.SwitchStreamingUnitLoadQueues_and_AcquireBothQueueMutexes();
    streamingUnitLoadQueueManager.Release(streamingUnitLoadQueueManager.GetMainThreadStreamingUnitLoadQueue());//give main thread the new main thread queue that we just acquired above

    auto& streamingUnitQueue = *streamingUnitLoadQueueManager.GetAssetLoadingStreamingUnitLoadQueue();//no need to acquire the asset thread queue mutex, since we acquired it above
    size_t streamingUnitQueueSize = streamingUnitQueue.m_queue.Size();
    for (size_t streamingUnitIndex = 0; streamingUnitIndex < streamingUnitQueueSize; ++streamingUnitIndex)
    {
        /*  advance past any streaming unit that isn't loaded; once no streaming units in the queue are not loaded clear the queue.  This
        ensures duplicate and already-loaded streaming units are only loaded once, and that unloading streaming units sit in the queue
        until they are unloaded*/
        NTF_REF(streamingUnitQueue.m_queue[streamingUnitIndex], streamingUnit);
        //printf("streamingUnit:%s\n", streamingUnit.m_filenameNoExtension.data());//#LogStreaming
        if (streamingUnit.StateMutexed() == StreamingUnitRuntime::kNotLoaded)
        {
            assert(stagingBuffersGpu.size() == 0);
            assert(stagingBufferMemoryMapCpuToGpu.IsEmptyAndAllocated());
            {
                //LARGE_INTEGER perfCount;
                //QueryPerformanceCounter(&perfCount);
                //printf("ASSET THREAD: AssetLoadingThread loading streaming unit; time=%f\n", static_cast<double>(perfCount.QuadPart)/ static_cast<double>(g_queryPerformanceFrequency.QuadPart));
            }

            streamingUnit.StateMutexed(StreamingUnitRuntime::kLoading);

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

            assert(streamingUnit.StateMutexed() == StreamingUnitRuntime::kLoading);

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
            NTF_REF(streamingUnit.m_deviceLocalMemory, deviceLocalMemory);
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

                streamingUnit.StateMutexed(StreamingUnitRuntime::kReady);//streaming unit is ready to render on the main thread

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
        }
    }
    streamingUnitQueue.m_queue.Clear();//loaded all requested staging buffers
    SignalSemaphoreWindows(streamingUnitQueue.m_streamingUnitsDoneLoadingHandle);
    streamingUnitLoadQueueManager.Release(&streamingUnitQueue);
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

        if (threadCommand == AssetLoadingArguments::ThreadCommand::kCleanupAndTerminate)
        {
            break;
        }

        StreamingUnitsLoadAllQueued(&threadArguments, &assetLoadingPersistentResources);
    }

    AssetLoadingPersistentResourcesDestroy(&assetLoadingPersistentResources, threadDone, device);
    return 0;
}

void StreamingUnitLoadQueueManager::SwitchStreamingUnitLoadQueues_and_AcquireBothQueueMutexes()
{
    for (auto& streamingUnitQueue : m_streamingUnitQueues)
    {
        WaitForSignalWindows(streamingUnitQueue.m_modifyMutex);
        NTF_LOG_STREAMING("%i:StreamingUnitLoadQueueManager::SwitchStreamingUnitLoadQueues_and_AcquireBothQueueMutexes:WaitForSignalWindows(&streamingUnitQueue=%p->streamingUnitQueue.m_modifyMutex=%zu)\n", 
            GetCurrentThreadId(), &streamingUnitQueue, (size_t)streamingUnitQueue.m_modifyMutex);
    }

    WaitForSignalWindows(m_mainThread_StreamingUnitQueue_IsIndex0_Mutex);
    NTF_LOG_STREAMING("%i:StreamingUnitLoadQueueManager::WaitForSignalWindows:WaitForSignalWindows(m_mainThread_StreamingUnitQueue_IsIndex0_Mutex=%zu)\n",
        GetCurrentThreadId(), (size_t)m_mainThread_StreamingUnitQueue_IsIndex0_Mutex);
    m_mainThreadStreamingUnitQueue0 = !m_mainThreadStreamingUnitQueue0;
    MutexRelease(m_mainThread_StreamingUnitQueue_IsIndex0_Mutex);
    NTF_LOG_STREAMING("%i:StreamingUnitLoadQueueManager::WaitForSignalWindows:MutexRelease(m_mainThread_StreamingUnitQueue_IsIndex0_Mutex=%zu)\n",
        GetCurrentThreadId(), (size_t)m_mainThread_StreamingUnitQueue_IsIndex0_Mutex);
}

void StreamingUnitLoadQueueManager::Destroy()
{
    auto& mainThreadStreamingUnitQueue = *GetMainThreadStreamingUnitLoadQueue_after_WaitForSignalWindows();
    auto& assetThreadStreamingUnitQueue = *GetAssetLoadingStreamingUnitLoadQueue_after_WaitForSignalWindows();

    //in case asset loading thread is loading one or more streaming units
    WaitForSignalWindows(mainThreadStreamingUnitQueue.m_streamingUnitsDoneLoadingHandle);
    WaitForSignalWindows(assetThreadStreamingUnitQueue.m_streamingUnitsDoneLoadingHandle);
    assert(mainThreadStreamingUnitQueue.m_queue.Empty());
    assert(assetThreadStreamingUnitQueue.m_queue.Empty());

    for (auto& streamingUnitQueue : m_streamingUnitQueues)
    {
        streamingUnitQueue.Destroy();
    }
    HandleCloseWindows(&m_mainThread_StreamingUnitQueue_IsIndex0_Mutex);
}
#include"StreamingUnitManager.h"

#include"ntf_math.h"
#include"StreamingUnit.h"
#include"WindowsUtil.h"

//#include "bmpImageFormat.h"

using namespace ntf;

#if NTF_UNIT_TEST_STREAMING_LOG
FILE* s_streamingDebug;
RTL_CRITICAL_SECTION s_streamingDebugCriticalSection;
#endif//#if NTF_UNIT_TEST_STREAMING_LOG

//extern LARGE_INTEGER g_queryPerformanceFrequency;

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

    const size_t stagingBufferCpuToGpuSizeBytes = 128 * 1024 * 1024;
    const VkDeviceSize stagingBufferCpuToGpuSizeAligned = AlignToNonCoherentAtomSize(stagingBufferCpuToGpuSizeBytes);
    CreateBuffer(
        &stagingBufferGpu,
        &stagingBufferGpuMemory,
        &deviceLocalMemoryPersistent,
        &offsetToFirstByteOfStagingBuffer,
        stagingBufferCpuToGpuSizeAligned,
        VulkanPagedStackAllocator::HeapSize::LARGE,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        true,///<this buffer will be memory mapped, so respect alignment
        device,
        physicalDevice);
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, stagingBufferGpu, &memRequirements);
    stagingBufferGpuAlignmentStandard = memRequirements.alignment;

    ArraySafeRef<uint8_t> stagingBufferMemoryMapCpuToGpuRet;
    MapMemory(&stagingBufferMemoryMapCpuToGpuRet, stagingBufferGpuMemory, offsetToFirstByteOfStagingBuffer, stagingBufferCpuToGpuSizeAligned, device);
    stagingBufferMemoryMapCpuToGpu.Initialize(stagingBufferMemoryMapCpuToGpuRet.data(), stagingBufferCpuToGpuSizeBytes);

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
    NTF_REF(threadArguments.m_commandBufferGraphics, commandBufferGraphics);
    NTF_REF(threadArguments.m_device, device);
    NTF_REF(threadArguments.m_deviceLocalMemoryCriticalSection, deviceLocalMemoryCriticalSection);
    NTF_REF(threadArguments.m_graphicsQueue, graphicsQueue);
    NTF_REF(threadArguments.m_graphicsQueueCriticalSection, graphicsQueueCriticalSection);
    NTF_REF(threadArguments.m_instance, instance);
    NTF_REF(threadArguments.m_physicalDevice, physicalDevice);
    NTF_REF(threadArguments.m_queueFamilyIndices, queueFamilyIndices);
	NTF_REF(threadArguments.m_renderPass, renderPass);
	NTF_REF(threadArguments.m_streamingUnitsToAddToLoadCriticalSection, streamingUnitsAddToLoadCriticalSection);
	NTF_REF(threadArguments.m_streamingUnitsToAddToRenderableCriticalSection, streamingUnitsAddToRenderableCriticalSection);
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
    assert(unifiedGraphicsAndTransferQueue == (queueFamilyIndices.TransferQueueIndex() == queueFamilyIndices.GraphicsQueueIndex()));
    RTL_CRITICAL_SECTION*const transferQueueCriticalSection = unifiedGraphicsAndTransferQueue ? &graphicsQueueCriticalSection : nullptr;//if we have a single queue for graphics and transfer rather than two separate queues, then we must criticalSection that one queue

    VkPipelineStageFlags transferFinishedPipelineStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VectorSafe<VkBuffer, 128> stagingBuffersGpu;

	VectorSafe<StreamingUnitRuntime*, kStreamingUnitCommandsNum> streamingUnitsToLoad;

    CriticalSectionEnter(&streamingUnitsAddToLoadCriticalSection);
	streamingUnitsToLoad.MemcpyFromStart(streamingUnitsToAddToLoad);
	streamingUnitsToAddToLoad.size(0);
	CriticalSectionLeave(&streamingUnitsAddToLoadCriticalSection);
   
    NTF_LOG_STREAMING(  "%s:%i:StreamingCommandsProcess():streamingUnitsToLoad.size()=%zu\n",
                        __FILE__, __LINE__, streamingUnitsToLoad.size());
    for (auto& streamingUnitToLoadPtr: streamingUnitsToLoad)
    {
        NTF_REF(streamingUnitToLoadPtr, streamingUnit);

        CriticalSectionEnter(&streamingUnit.m_stateCriticalSection);
        const bool stateWasLoading = streamingUnit.m_state == StreamingUnitRuntime::State::kLoading;
        NTF_LOG_STREAMING("%s:%i:StreamingCommandsProcess():%s.m_state=%zu\n", __FILE__, __LINE__, streamingUnit.m_filenameNoExtension.data(), streamingUnit.m_state);
        assert(stateWasLoading);
        if (stateWasLoading)
        {
            CriticalSectionLeave(&streamingUnit.m_stateCriticalSection);

            NTF_LOG_STREAMING(  "%s:%i:StreamingCommandsProcess():StreamingCommand::kLoad; %s.m_state=%i\n",
                                __FILE__, __LINE__, streamingUnit.m_filenameNoExtension.data(), streamingUnit.m_state);

            assert(stagingBuffersGpu.size() == 0);
            assert(stagingBufferMemoryMapCpuToGpu.IsEmptyAndAllocated());
            {
                //LARGE_INTEGER perfCount;
                //QueryPerformanceCounter(&perfCount);
                //printf("ASSET THREAD: AssetLoadingThread loading streaming unit; time=%f\n", static_cast<double>(perfCount.QuadPart)/ static_cast<double>(g_queryPerformanceFrequency.QuadPart));
            }

            //allocate a memory allocator to the streaming unit
            CriticalSectionEnter(&deviceLocalMemoryCriticalSection);
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
            CriticalSectionLeave(&deviceLocalMemoryCriticalSection);

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
            CommandBufferBegin(commandBufferTransfer, device);
            if (!unifiedGraphicsAndTransferQueue)
            {
                CommandBufferBegin(commandBufferGraphics, device);
            }
            CreateTextureSampler(&streamingUnit.m_textureSampler, device);

            VectorSafe<VkSemaphore, 1> transferFinishedSemaphores;

            ArraySafe<char, 512> streamingUnitFilePathRelative;
            streamingUnitFilePathRelative.Sprintf("%s\\%s.%s",
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
            NTF_REF(streamingUnit.m_deviceLocalMemory, deviceLocalMemory);//does not need to be criticalSection'd, because allocation and freeing of GPU deviceLocalMemory is criticalSection'd, and a streaming unit cannot get unloaded if it isn't loaded first, like here
            for (size_t texturedGeometryIndex = 0; texturedGeometryIndex < texturedGeometryNum; ++texturedGeometryIndex)
            {
                //load texture
                StreamingUnitTextureDimension textureWidth, textureHeight;
                uint32_t mipLevels;
                auto& texturedGeometry = streamingUnit.m_texturedGeometries[texturedGeometryIndex];
                size_t imageSizeBytes;
                const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
                VkMemoryRequirements memoryRequirements;
                const size_t stagingBuffersGpuIndexOfNextTransfer = stagingBuffersGpu.size();
                ReadTextureAndCreateImageAndCopyPixelsIfStagingBufferHasSpace(
                    &texturedGeometry.textureImage,
                    &deviceLocalMemory,
                    &textureWidth,
                    &textureHeight,
                    &mipLevels,
                    &stagingBufferMemoryMapCpuToGpu,
                    &imageSizeBytes,
                    &stagingBufferGpuOffsetToAllocatedBlock,
                    streamingUnitFile,
                    &memoryRequirements,
                    &stagingBuffersGpu,
                    stagingBufferGpuMemory,
                    offsetToFirstByteOfStagingBuffer,
                    imageFormat,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT/*accessible by shader*/,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    physicalDevice,
                    device);

                //BEG_HAC
                //{
                //    ArraySafe<char, 128> filename;
                //    filename.Sprintf("E:\\readbackImageMip0.bmp");
                //    WriteR8G8B8A8ToBmpFile(stagingBufferMemoryMapCpuToGpu.GetMemory(), textureWidth, textureHeight, filename);
                //}
                //END_HAC

                uint32_t textureWidthCurrentMipLevel = textureWidth;
                uint32_t textureHeightCurrentMipLevel = textureHeight;
                const size_t bytesPerPixel = 4;
                for (uint32_t mipLevel = 1; mipLevel < mipLevels; ++mipLevel)
                {
                    DivideByTwoIfGreaterThanOne(&textureWidthCurrentMipLevel);
                    DivideByTwoIfGreaterThanOne(&textureHeightCurrentMipLevel);

                    const size_t imageSizeBytesMipCurrent =
                        ImageSizeBytesCalculate(textureWidthCurrentMipLevel, textureHeightCurrentMipLevel, bytesPerPixel);

                    ArraySafe<char, 128> filename;
                    ArraySafeRef<StreamingUnitByte> pixelBufferRuntimeIn;

                    TextureSerializeImagePixels<SerializerRuntimeIn>(
                        streamingUnitFile,
                        ConstArraySafeRef<StreamingUnitByte>(),
                        &stagingBufferMemoryMapCpuToGpu,
                        memoryRequirements.alignment,
                        imageSizeBytesMipCurrent,
                        &stagingBufferGpuOffsetToAllocatedBlock);

                    stagingBuffersGpu.sizeIncrement();
                    VkBuffer& stagingBuffer = stagingBuffersGpu.back();
                    CreateBuffer(
                        &stagingBuffer,
                        &stagingBufferGpuOffsetToAllocatedBlock,
                        stagingBufferGpuMemory,
                        offsetToFirstByteOfStagingBuffer,
                        CastWithAssert<size_t, VkDeviceSize>(imageSizeBytesMipCurrent),
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        device,
                        physicalDevice);
                }

                TransferImageFromCpuToGpu(
                    texturedGeometry.textureImage,
                    textureWidth,
                    textureHeight,
                    mipLevels,
                    ConstVectorSafeRef<VkBuffer>(stagingBuffersGpu, stagingBuffersGpuIndexOfNextTransfer),
                    commandBufferTransfer,
                    queueFamilyIndices.TransferQueueIndex(),
                    commandBufferGraphics,
                    queueFamilyIndices.GraphicsQueueIndex(),
                    device,
                    instance);

                CreateTextureImageView(&streamingUnit.m_textureImageViews[texturedGeometryIndex], texturedGeometry.textureImage, mipLevels, device);
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
                    ConstArraySafeRef<Vertex>(),
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
                    queueFamilyIndices.TransferQueueIndex(),
                    commandBufferGraphics,
                    queueFamilyIndices.GraphicsQueueIndex(),
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
                    ConstArraySafeRef<IndexBufferValue>(),
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
                    queueFamilyIndices.TransferQueueIndex(),
                    commandBufferGraphics,
                    queueFamilyIndices.GraphicsQueueIndex(),
                    device,
                    physicalDevice,
                    instance);
                {
                    //LARGE_INTEGER perfCount;
                    //QueryPerformanceCounter(&perfCount);
                    //printf("ASSET THREAD: CreateBuffer()=%llu at time %f\n", (uint64_t)stagingBuffersGpu[stagingBufferGpuAllocateIndex-1], static_cast<double>(perfCount.QuadPart)/ static_cast<double>(g_queryPerformanceFrequency.QuadPart));
                }
                ///TODO_NEXT: try flushing entire staging buffer's contents -- imageOptimal, index/vertex buffer, everything
                //FlushMemoryMappedRange(stagingBufferGpuMemory, stagingBufferGpuOffsetToAllocatedBlock, AlignToNonCoherentAtomSize());
            }
            Fclose(streamingUnitFile);
            if (!unifiedGraphicsAndTransferQueue)
            {
                transferFinishedSemaphores.Push(transferFinishedSemaphore);
            }

            CommandBufferEnd(commandBufferTransfer);
            SubmitCommandBuffer(
                transferQueueCriticalSection,
                ConstVectorSafeRef<VkSemaphore>(),
                transferFinishedSemaphores,
                ConstArraySafeRef<VkPipelineStageFlags>(),
                commandBufferTransfer,
                transferQueue,
                streamingUnit.m_transferQueueFinishedFence,
                instance);

            if (!unifiedGraphicsAndTransferQueue)
            {
                CommandBufferEnd(commandBufferGraphics);
                SubmitCommandBuffer(
                    &graphicsQueueCriticalSection,
                    transferFinishedSemaphores,
                    ConstVectorSafeRef<VkSemaphore>(),
                    ConstArraySafeRef<VkPipelineStageFlags>(&transferFinishedPipelineStageFlags, 1),
                    commandBufferGraphics,
                    graphicsQueue,
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
                streamingUnit.m_textureImageViews,
                TODO_REFACTOR_NUM,//#NumberOfRenderablesHack
                streamingUnit.m_textureSampler,
                device);

            //clean up staging buffers if they were in use but have completed their transfers
            {
                FenceWaitUntilSignalled(streamingUnit.m_transferQueueFinishedFence, device);
                if (!unifiedGraphicsAndTransferQueue)
                {
                    FenceWaitUntilSignalled(streamingUnit.m_graphicsQueueFinishedFence, device);
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

            /*  streaming unit is now loaded so tag it renderable -- but don't set state to loaded until it is guaranteed to be rendered at least
                once (provided the app doesn't shut down first) */
            CriticalSectionEnter(&streamingUnitsAddToRenderableCriticalSection);
            streamingUnitsToAddToRenderable.Push(&streamingUnit);
            CriticalSectionLeave(&streamingUnitsAddToRenderableCriticalSection);

            NTF_LOG_STREAMING(  "%s:%i:StreamingCommandsProcess():Completed Gpu load: %s.m_state=%i -- placed on addToRenderable list\n",
                                __FILE__, __LINE__, streamingUnit.m_filenameNoExtension.data(), streamingUnit.m_state);
        }//if (stateWasLoading)
        else
        {
            CriticalSectionLeave(&streamingUnit.m_stateCriticalSection);
        }
    }//for (auto& streamingUnitToLoadPtr: streamingUnitsToLoad)
    streamingUnitsToLoad.size(0);//all should be processed
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

    NTF_LOG_STREAMING(  "%s:%i:AssetLoadingPersistentResourcesDestroy() completed\n", 
                        __FILE__, __LINE__);

    SignalSemaphoreWindows(threadDone);
}
DWORD WINAPI AssetLoadingThread(void* arg)
{
    //asset loading thread has one lower priority than all other threads (main and worker threads); prefer smooth framerate over speedy asset loading
    //"The system assigns time slices in a round-robin fashion to all threads with the highest priority.  If none of these threads are ready to run, the system assigns time slices in a round-robin fashion to all threads with the next highest priority. If a higher-priority thread becomes available to run, the system ceases to execute the lower-priority thread (without allowing it to finish using its time slice) and assigns a full time slice to the higher-priority thread." -- https://docs.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities?redirectedfrom=MSDN
    const BOOL setThreadPriorityResult = SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
    assert(setThreadPriorityResult);

    auto& threadArguments = *reinterpret_cast<AssetLoadingArguments*>(arg);
    threadArguments.AssertValid();

    NTF_REF(threadArguments.m_assetLoadingThreadIdle, assetLoadingThreadIdle);
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
        
        NTF_LOG_STREAMING(  "%s:%i:AssetLoadingThread() about to call WaitForSignalWindows(threadWake)\n",
                            __FILE__, __LINE__);
        assetLoadingThreadIdle = true;
        WaitForSignalWindows(threadWake);
        assetLoadingThreadIdle = false;
        NTF_LOG_STREAMING(  "%s:%i:AssetLoadingThread() returned from WaitForSignalWindows(threadWake)\n", 
                            __FILE__, __LINE__);

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

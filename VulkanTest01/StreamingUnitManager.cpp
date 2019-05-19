#include"StreamingUnitManager.h"

//extern LARGE_INTEGER g_queryPerformanceFrequency;

#define NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE (128 * 1024 * 1024)
DWORD WINAPI AssetLoadingThread(void* arg)
{
    auto& threadArguments = *reinterpret_cast<AssetLoadingArguments*>(arg);
    threadArguments.AssertValid();
    NTF_REF(threadArguments.m_commandBufferTransfer, commandBufferTransfer);
    NTF_REF(threadArguments.m_commandBufferTransitionImage, commandBufferTransitionImage);
    NTF_REF(threadArguments.m_device, device);
    NTF_REF(threadArguments.m_deviceLocalMemory, deviceLocalMemory);
    NTF_REF(threadArguments.m_graphicsQueue, graphicsQueue);
    NTF_REF(threadArguments.m_physicalDevice, physicalDevice);
    NTF_REF(threadArguments.m_queueFamilyIndices, queueFamilyIndices);
    NTF_REF(threadArguments.m_streamingUnit, streamingUnit);
    auto& threadCommand = *threadArguments.m_threadCommand;
    NTF_REF(threadArguments.m_threadDone, threadDone);
    NTF_REF(threadArguments.m_threadWake, threadWake);
    NTF_REF(threadArguments.m_transferQueue, transferQueue);

    NTF_REF(threadArguments.m_renderPass, renderPass);
    NTF_REF(threadArguments.m_swapChainExtent, swapChainExtent);

    StackCpu stackAllocatorHack;///<@todo: Streaming Memory -- this isn't a hack, you need space for the shaders to load
    const size_t stackAllocatorHackMemorySizeBytes = 64 * 1024 * 1024;
    static StreamingUnitByte stackAllocatorHackMemory[stackAllocatorHackMemorySizeBytes];
    stackAllocatorHack.Initialize(&stackAllocatorHackMemory[0], stackAllocatorHackMemorySizeBytes);

    StackCpu stagingBufferMemoryMapCpuToGpu;
    VkDeviceMemory stagingBufferGpuMemory;
    VkBuffer stagingBufferGpu;

    VkDeviceSize stagingBufferGpuOffsetToAllocatedBlock;
    VkDeviceSize offsetToFirstByteOfStagingBuffer;

    const bool unifiedGraphicsAndTransferQueue = graphicsQueue == transferQueue;
    assert(unifiedGraphicsAndTransferQueue == (queueFamilyIndices.transferFamily == queueFamilyIndices.graphicsFamily));

    ///@todo: pass in extra alignment requirement since VK_MEMORY_PROPERTY_HOST_COHERENT_BIT wasn't used -- must align both offsetToAllocatedBlockPtr and size arguments
    CreateBuffer(
        &stagingBufferGpu,
        &stagingBufferGpuMemory,
        &deviceLocalMemory,
        &offsetToFirstByteOfStagingBuffer,///<@todo: need to make sure this takes into account nonCoherentAtomSize when doing offset for if VK_MEMORY_PROPERTY_HOST_COHERENT_BIT is absent
        NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE,///<@todo: need to make sure this takes into account nonCoherentAtomSize when doing offset for if VK_MEMORY_PROPERTY_HOST_COHERENT_BIT is absent
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        true,
        device,
        physicalDevice);
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, stagingBufferGpu, &memRequirements);
    const VkDeviceSize stagingBufferGpuAlignmentStandard = memRequirements.alignment;

    void* stagingBufferMemoryMapCpuToGpuPtr;
    const VkResult vkMapMemoryResult = vkMapMemory(
        device,
        stagingBufferGpuMemory,
        offsetToFirstByteOfStagingBuffer,
        NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE,
        0,
        &stagingBufferMemoryMapCpuToGpuPtr);
    NTF_VK_ASSERT_SUCCESS(vkMapMemoryResult);
    stagingBufferMemoryMapCpuToGpu.Initialize(reinterpret_cast<uint8_t*>(stagingBufferMemoryMapCpuToGpuPtr), NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE);

    ArraySafe<VkBuffer, 32> stagingBuffersGpu;///<@todo: should be VectorSafe
    size_t stagingBufferGpuAllocateIndex = 0;

    VkSemaphore transferFinishedSemaphore;
    CreateVulkanSemaphore(&transferFinishedSemaphore, device);
    VkPipelineStageFlags transferFinishedPipelineStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;

    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    for (;;)
    {
        //#Wait
        //WaitOnAddress(&signalMemory, &undesiredValue, sizeof(AssetLoadingArguments::SignalMemoryType), INFINITE);//#SynchronizationWindows8+Only
        const DWORD waitForSingleObjectResult = WaitForSingleObject(threadWake, INFINITE);
        assert(waitForSingleObjectResult == WAIT_OBJECT_0);

        if (threadCommand == AssetLoadingArguments::ThreadCommand::kCleanupAndTerminate)
        {
            break;
        }
        assert(threadCommand == AssetLoadingArguments::ThreadCommand::kLoadStreamingUnit);

        assert(stagingBufferGpuAllocateIndex == 0);
        assert(stagingBufferMemoryMapCpuToGpu.IsEmptyAndAllocated());
        {
            //LARGE_INTEGER perfCount;
            //QueryPerformanceCounter(&perfCount);
            //printf("ASSET THREAD: AssetLoadingThread loading streaming unit; time=%f\n", static_cast<double>(perfCount.QuadPart)/ static_cast<double>(g_queryPerformanceFrequency.QuadPart));
        }

        const VkFenceCreateFlagBits fenceCreateFlagBits = static_cast<VkFenceCreateFlagBits>(0);
        FenceCreate(&streamingUnit.m_transferQueueFinishedFence, fenceCreateFlagBits, device);
        FenceCreate(&streamingUnit.m_graphicsQueueFinishedFence, fenceCreateFlagBits, device);
        
        assert(streamingUnit.StateMutexed() == StreamingUnitRuntime::kLoading);

        const VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        CreateDescriptorPool(&streamingUnit.m_descriptorPool, descriptorType, device, TODO_REFACTOR_NUM);
        CreateDescriptorSetLayout(&streamingUnit.m_descriptorSetLayout, descriptorType, device, TODO_REFACTOR_NUM);
        CreateGraphicsPipeline(
            &streamingUnit.m_pipelineLayout,
            &streamingUnit.m_graphicsPipeline,
            &stackAllocatorHack,
            renderPass,
            streamingUnit.m_descriptorSetLayout,
            swapChainExtent,
            device);

        streamingUnit.m_uniformBufferSizeAligned = UniformBufferCpuAlignmentCalculate(streamingUnit.m_uniformBufferSizeUnaligned, physicalDevice);
        BeginCommands(commandBufferTransfer, device);
        if (!unifiedGraphicsAndTransferQueue)
        {
            BeginCommands(commandBufferTransitionImage, device);
        }
        CreateTextureSampler(&streamingUnit.m_textureSampler, device);

        VectorSafe<VkSemaphore, 1> transferFinishedSemaphores;

        ArraySafe<char, 512> streamingUnitFilePathRelative;
        streamingUnitFilePathRelative.Snprintf("%s\\%s.%s",
            CookedFileDirectoryGet(), threadArguments.m_streamingUnitFilenameNoExtension, StreamingUnitFilenameExtensionGet());
        FILE* streamingUnitFile;

        Fopen(&streamingUnitFile, streamingUnitFilePathRelative.begin(), "rb");

        //BEG_GENERALIZE_READER_WRITER
        StreamingUnitVersion version;
        StreamingUnitTexturedGeometryNum texturedGeometryNum;
        Fread(streamingUnitFile, &version, sizeof(version), 1);
        Fread(streamingUnitFile, &texturedGeometryNum, sizeof(texturedGeometryNum), 1);
        //END_GENERALIZE_READER_WRITER
        stagingBufferGpuOffsetToAllocatedBlock = 0;
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
                false,
                device,
                physicalDevice);

            CreateBuffer(
                &stagingBuffersGpu[stagingBufferGpuAllocateIndex],
                &stagingBufferGpuOffsetToAllocatedBlock,
                stagingBufferGpuMemory,
                offsetToFirstByteOfStagingBuffer,
                imageSizeBytes,
                0,
                device,
                physicalDevice);

            TransferImageFromCpuToGpu(
                texturedGeometry.textureImage,
                textureWidth,
                textureHeight,
                imageFormat,
                stagingBuffersGpu[stagingBufferGpuAllocateIndex],
                commandBufferTransfer,
                transferQueue,
                queueFamilyIndices.transferFamily,
                commandBufferTransitionImage,
                graphicsQueue,
                queueFamilyIndices.graphicsFamily,
                device);

            CreateTextureImageView(&streamingUnit.m_textureImageViews[texturedGeometryIndex], texturedGeometry.textureImage, device);
            ++stagingBufferGpuAllocateIndex;
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
                &stagingBufferGpuAllocateIndex,
                &stagingBufferGpuOffsetToAllocatedBlock,
                stagingBufferGpuMemory,
                stagingBufferGpuAlignmentStandard,
                offsetToFirstByteOfStagingBuffer,
                vertexBufferSizeBytes,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,/*specifies that the buffer is suitable for passing as an element of the pBuffers array to vkCmdBindVertexBuffers*/
                false,
                commandBufferTransfer,
                device,
                physicalDevice);

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
                &stagingBufferGpuAllocateIndex,
                &stagingBufferGpuOffsetToAllocatedBlock,
                stagingBufferGpuMemory,
                stagingBufferGpuAlignmentStandard,
                offsetToFirstByteOfStagingBuffer,
                indexBufferSizeBytes,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                false,
                commandBufferTransfer,
                device,
                physicalDevice);
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
            streamingUnit.m_transferQueueFinishedFence);

        if (!unifiedGraphicsAndTransferQueue)
        {
            EndCommandBuffer(commandBufferTransitionImage);
            SubmitCommandBuffer(
                ConstVectorSafeRef<VkSemaphore>(),
                transferFinishedSemaphores,
                ArraySafeRef<VkPipelineStageFlags>(&transferFinishedPipelineStageFlags, 1),///<@todo: ArraySafeRefConst
                commandBufferTransitionImage,
                graphicsQueue,
                streamingUnit.m_graphicsQueueFinishedFence);
        }

        const VkDeviceSize uniformBufferSize = streamingUnit.m_uniformBufferSizeAligned;
        CreateUniformBuffer(
            &streamingUnit.m_uniformBufferCpuMemory,
            &streamingUnit.m_uniformBufferGpuMemory,
            &streamingUnit.m_uniformBuffer,
            &deviceLocalMemory,
            &streamingUnit.m_uniformBufferOffsetToGpuMemory,
            uniformBufferSize,
            false,
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
            if (!unifiedGraphicsAndTransferQueue)
            {
                FenceWaitUntilSignalled(streamingUnit.m_graphicsQueueFinishedFence, device);
            }

            streamingUnit.StateMutexed(StreamingUnitRuntime::kReady);//streaming unit is ready to render on the main thread

            //clean up staging memory
            stagingBufferMemoryMapCpuToGpu.Clear();

            for (   size_t stagingBufferGpuAllocateIndexFree = 0;
                    stagingBufferGpuAllocateIndexFree < stagingBufferGpuAllocateIndex;
                    ++stagingBufferGpuAllocateIndexFree)
            {
                vkDestroyBuffer(device, stagingBuffersGpu[stagingBufferGpuAllocateIndexFree], GetVulkanAllocationCallbacks());

                //LARGE_INTEGER perfCount;
                //QueryPerformanceCounter(&perfCount);
                //printf("ASSET THREAD: vkDestroyBuffer(%llu) at time %f\n", (uint64_t)stagingBuffersGpu[stagingBufferGpuAllocateIndexFree], static_cast<double>(perfCount.QuadPart)/ static_cast<double>(g_queryPerformanceFrequency.QuadPart));
            }
            stagingBufferGpuAllocateIndex = 0;

            {
                //LARGE_INTEGER perfCount;
                //QueryPerformanceCounter(&perfCount);
                //printf("ASSET THREAD: AssetLoadingThread finished destroying staging buffers; time=%f\n", static_cast<double>(perfCount.QuadPart)/ static_cast<double>(g_queryPerformanceFrequency.QuadPart));
            }
        }
    }

    //cleanup
    stackAllocatorHack.Destroy();

    vkUnmapMemory(device, stagingBufferGpuMemory);
    vkDestroyBuffer(device, stagingBufferGpu, GetVulkanAllocationCallbacks());

    stagingBufferMemoryMapCpuToGpu.Destroy();
    vkDestroySemaphore(device, transferFinishedSemaphore, GetVulkanAllocationCallbacks());

    SignalSemaphoreWindows(threadDone);
    return 0;
}

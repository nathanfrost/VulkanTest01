#include"StreamingUnitManager.h"

#define NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE (128 * 1024 * 1024)
DWORD WINAPI AssetLoadingThread(void* arg)
{
    auto& threadArguments = *reinterpret_cast<AssetLoadingArguments*>(arg);
    threadArguments.AssertValid();
    NTF_REF(threadArguments.m_commandBufferTransfer, commandBufferTransfer);
    NTF_REF(threadArguments.m_commandBufferTransitionImage, commandBufferTransitionImage);
    NTF_REF(threadArguments.m_device, device);
    NTF_REF(threadArguments.m_deviceLocalMemory, deviceLocalMemory);
    NTF_REF(threadArguments.m_graphicsPipeline, graphicsPipeline);
    NTF_REF(threadArguments.m_graphicsQueue, graphicsQueue);
    NTF_REF(threadArguments.m_physicalDevice, physicalDevice);
    NTF_REF(threadArguments.m_queueFamilyIndices, queueFamilyIndices);
    NTF_REF(threadArguments.m_streamingUnit, streamingUnit);
    auto& threadCommand = threadArguments.m_threadCommand;
    NTF_REF(threadArguments.m_threadDone, threadDone);
    NTF_REF(threadArguments.m_threadWake, threadWake);
    NTF_REF(threadArguments.m_transferQueue, transferQueue);

    NTF_REF(threadArguments.m_renderPass, renderPass);
    NTF_REF(threadArguments.m_swapChainExtent, swapChainExtent);

    StackCpu stackAllocatorHack;///<@todo: Streaming Memory
    const size_t stackAllocatorHackMemorySizeBytes = 64 * 1024 * 1024;
    static StreamingUnitByte stackAllocatorHackMemory[stackAllocatorHackMemorySizeBytes];
    stackAllocatorHack.Initialize(&stackAllocatorHackMemory[0], stackAllocatorHackMemorySizeBytes);

    StackNTF<VkDeviceSize> stagingBufferGpuStack;
    stagingBufferGpuStack.Allocate(NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE);

    StackCpu stagingBufferMemoryMapCpuToGpu;
    VkDeviceMemory stagingBufferGpuMemory;
    VkBuffer stagingBufferGpu;

    VkDeviceSize stagingBufferGpuOffsetToAllocatedBlock;
    VkDeviceSize offsetToFirstByteOfStagingBuffer;
    VkDescriptorPool descriptorPool;

    VkFence transferQueueFinishedFence;
    FenceCreate(&transferQueueFinishedFence, static_cast<VkFenceCreateFlagBits>(0), device);

    CreateBuffer(
        &stagingBufferGpu,
        &stagingBufferGpuMemory,
        &deviceLocalMemory,
        &offsetToFirstByteOfStagingBuffer,
        NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
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

    ArraySafe<VkBuffer, 32> stagingBuffersGpu;
    size_t stagingBufferGpuAllocateIndex = 0;

    assert(stagingBufferGpuAllocateIndex == 0);
    assert(stagingBufferMemoryMapCpuToGpu.IsEmptyAndAllocated());

    VkSemaphore transferFinishedSemaphore;
    CreateVulkanSemaphore(&transferFinishedSemaphore, device);
    VkPipelineStageFlags transferFinishedPipelineStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;

    for (;;)
    {
        //clean up staging buffers if they were in use but have completed their transfers
        ///@todo NTF: try to do this constantly and immediately after initiating transfers so this work is likely to be completed before the next transfers is requested #StreamingMemory
        const bool loadingOperationsWereInFlight = stagingBufferGpuAllocateIndex > 0;
        while (stagingBufferGpuAllocateIndex > 0)
        {
            FenceWaitUntilSignalled(transferQueueFinishedFence, device);
            SignalSemaphoreWindows(threadDone);///<@todo NTF: generalize for #StreamingMemory by signalling a unique semaphore for each streaming unit loaded

                                               //clean up staging memory
            stagingBufferMemoryMapCpuToGpu.Clear();

            for (size_t stagingBufferGpuAllocateIndexFree = 0;
            stagingBufferGpuAllocateIndexFree < stagingBufferGpuAllocateIndex;
                ++stagingBufferGpuAllocateIndexFree)
            {
                vkDestroyBuffer(device, stagingBuffersGpu[stagingBufferGpuAllocateIndexFree], GetVulkanAllocationCallbacks());
            }
            stagingBufferGpuAllocateIndex = 0;
        }

        //#Wait
        //WaitOnAddress(&signalMemory, &undesiredValue, sizeof(AssetLoadingArguments::SignalMemoryType), INFINITE);//#SynchronizationWindows8+Only
        DWORD waitForSingleObjectResult = WaitForSingleObject(threadWake, INFINITE);
        assert(waitForSingleObjectResult == WAIT_OBJECT_0);

        if (*threadCommand == AssetLoadingArguments::ThreadCommand::kCleanupAndTerminate)
        {
            break;
        }
        assert(*threadCommand == AssetLoadingArguments::ThreadCommand::kLoadStreamingUnit);

        FenceReset(transferQueueFinishedFence, device);//commencing loading
                                                       ///@todo: generalize #StreamingMemory
        const VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        CreateDescriptorPool(&descriptorPool, descriptorType, device, TODO_REFACTOR_NUM);
        CreateDescriptorSetLayout(&streamingUnit.m_descriptorSetLayout, descriptorType, device, TODO_REFACTOR_NUM);
        CreateGraphicsPipeline(
            &streamingUnit.m_pipelineLayout,
            &graphicsPipeline,
            &stackAllocatorHack,
            renderPass,
            streamingUnit.m_descriptorSetLayout,
            swapChainExtent,
            device);

        streamingUnit.m_uniformBufferSizeAligned = UniformBufferCpuAlignmentCalculate(streamingUnit.m_uniformBufferSizeUnaligned, physicalDevice);
        const bool unifiedGraphicsAndTransferQueue = graphicsQueue == transferQueue;
        assert(unifiedGraphicsAndTransferQueue == (queueFamilyIndices.transferFamily == queueFamilyIndices.graphicsFamily));
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
        for (size_t texturedGeometryIndex = 0; texturedGeometryIndex < texturedGeometryNum; ++texturedGeometryIndex)
        {
            //load texture
            StreamingUnitTextureDimension textureWidth, textureHeight;
            auto& texturedGeometry = streamingUnit.m_texturedGeometries[texturedGeometryIndex];
            size_t imageSizeBytes;
            VkDeviceSize alignment;
            const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
            ReadTextureAndCreateImageAndCopyPixelsIfStagingBufferHasSpace(
                &texturedGeometry.textureImage,
                &deviceLocalMemory,
                &alignment,
                &textureWidth,
                &textureHeight,
                &stagingBufferMemoryMapCpuToGpu,
                &imageSizeBytes,
                streamingUnitFile,
                imageFormat,
                VK_IMAGE_TILING_OPTIMAL/*could also pass VK_IMAGE_TILING_LINEAR so texels are laid out in row-major order for debugging (less performant)*/,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT/*accessible by shader*/,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                false,
                device,
                physicalDevice);

            const bool pushAllocSuccess = stagingBufferGpuStack.PushAlloc(&stagingBufferGpuOffsetToAllocatedBlock, alignment, imageSizeBytes);
            assert(pushAllocSuccess);
            CreateBuffer(
                &stagingBuffersGpu[stagingBufferGpuAllocateIndex],
                stagingBufferGpuMemory,
                offsetToFirstByteOfStagingBuffer + stagingBufferGpuOffsetToAllocatedBlock,
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

            //load vertex and index buffer
            ArraySafeRef<StreamingUnitByte> stagingBufferCpuToGpuVertices, stagingBufferCpuToGpuIndices;
            StreamingUnitVerticesNum verticesNum;
            StreamingUnitIndicesNum indicesNum;
            size_t vertexBufferSizeBytes, indexBufferSizeBytes;
            ModelSerialize<SerializerRuntimeIn>(
                streamingUnitFile,
                &stagingBufferMemoryMapCpuToGpu,
                stagingBufferGpuAlignmentStandard,
                &verticesNum,
                ArraySafeRef<Vertex>(),
                stagingBufferCpuToGpuVertices,
                &vertexBufferSizeBytes,
                &indicesNum,
                ArraySafeRef<IndexBufferValue>(),
                stagingBufferCpuToGpuIndices,
                &indexBufferSizeBytes);
            texturedGeometry.indicesSize = CastWithAssert<size_t, uint32_t>(indicesNum);

            CopyBufferToGpuPrepare(
                &deviceLocalMemory,
                &texturedGeometry.vertexBuffer,
                &texturedGeometry.vertexBufferMemory,
                &stagingBuffersGpu,
                &stagingBufferGpuAllocateIndex,
                &stagingBufferGpuStack,
                stagingBufferGpuMemory,
                stagingBufferGpuAlignmentStandard,
                offsetToFirstByteOfStagingBuffer,
                vertexBufferSizeBytes,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,/*specifies that the buffer is suitable for passing as an element of the pBuffers array to vkCmdBindVertexBuffers*/
                false,
                commandBufferTransfer,
                device,
                physicalDevice);
            CopyBufferToGpuPrepare(
                &deviceLocalMemory,
                &texturedGeometry.indexBuffer,
                &texturedGeometry.indexBufferMemory,
                &stagingBuffersGpu,
                &stagingBufferGpuAllocateIndex,
                &stagingBufferGpuStack,
                stagingBufferGpuMemory,
                stagingBufferGpuAlignmentStandard,
                offsetToFirstByteOfStagingBuffer,
                indexBufferSizeBytes,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                false,
                commandBufferTransfer,
                device,
                physicalDevice);
        }
        Fclose(streamingUnitFile);
        if (!unifiedGraphicsAndTransferQueue)
        {
            transferFinishedSemaphores.Push(transferFinishedSemaphore);
        }
        vkEndCommandBuffer(commandBufferTransfer);
        SubmitCommandBuffer(
            transferFinishedSemaphores,
            ConstVectorSafeRef<VkSemaphore>(),
            ArraySafeRef<VkPipelineStageFlags>(),
            commandBufferTransfer,
            transferQueue,
            transferQueueFinishedFence);

        if (!unifiedGraphicsAndTransferQueue)
        {
            vkEndCommandBuffer(commandBufferTransitionImage);
            SubmitCommandBuffer(
                ConstVectorSafeRef<VkSemaphore>(),
                transferFinishedSemaphores,
                ArraySafeRef<VkPipelineStageFlags>(&transferFinishedPipelineStageFlags, 1),///<@todo: ArraySafeRefConst
                commandBufferTransitionImage,
                graphicsQueue,
                VK_NULL_HANDLE);
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
            descriptorPool,
            streamingUnit.m_uniformBuffer,
            uniformBufferSize,
            &streamingUnit.m_textureImageViews,///<@todo NTF: @todo: ConstArraySafeRef that does not need ambersand here
            TODO_REFACTOR_NUM,
            streamingUnit.m_textureSampler,
            device);
    }

    //cleanup
    stackAllocatorHack.Destroy();

    vkDestroyFence(device, transferQueueFinishedFence, GetVulkanAllocationCallbacks());

    vkUnmapMemory(device, stagingBufferGpuMemory);
    vkDestroyBuffer(device, stagingBufferGpu, GetVulkanAllocationCallbacks());

    stagingBufferMemoryMapCpuToGpu.Destroy();
    vkDestroySemaphore(device, transferFinishedSemaphore, GetVulkanAllocationCallbacks());

    vkDestroyDescriptorPool(device, descriptorPool, GetVulkanAllocationCallbacks());

    SignalSemaphoreWindows(threadDone);
    return 0;
}

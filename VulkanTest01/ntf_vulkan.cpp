#include"ntf_vulkan.h"
#include"ntf_vulkan_utility.h"

#define STB_IMAGE_IMPLEMENTATION
//BEG_#StbMemoryManagement
StackCpu* g_stbAllocator;
void* __cdecl stb_malloc(size_t _Size)
{ 
    assert(g_stbAllocator);
    
    void* memory;
    g_stbAllocator->PushAlloc(&memory, 0, _Size);
    assert(memory);
    return memory;
}
void* __cdecl stb_assertRealloc(void*  _Block, size_t _Size) { assert(false); return nullptr; }
void __cdecl stb_nullFree(void* const block) {}
#include"stb_image.h"
//END_#StbMemoryManagement

#define TINYOBJLOADER_IMPLEMENTATION
#include"tiny_obj_loader.h"

#if NTF_WIN_TIMER
FILE* s_winTimer;
#endif//NTF_WIN_TIMER

//BEG_#StbMemoryManagement
void STBAllocatorCreate()
{
    g_stbAllocator = new StackCpu();
    const size_t sizeBytes = 128 * 1024 * 1024;
    g_stbAllocator->Initialize(reinterpret_cast<uint8_t*>(malloc(sizeBytes)), sizeBytes);
}
void STBAllocatorDestroy()
{
    free(g_stbAllocator->GetMemory());
    g_stbAllocator->Destroy();
    delete g_stbAllocator;
}
//END_#StbMemoryManagement

//BEG_#AllocationCallbacks
static VkAllocationCallbacks s_allocationCallbacks;
VkAllocationCallbacks* GetVulkanAllocationCallbacks() { return nullptr;/* &s_allocationCallbacks;*/ }

#if NTF_DEBUG
//BEG_HAC_#AllocationTracking
static size_t s_vulkanApiCpuBytesAllocated, s_vulkanApiCpuBytesAllocatedMax;
size_t GetVulkanApiCpuBytesAllocatedMax() { return s_vulkanApiCpuBytesAllocatedMax; }
struct PointerSizePair
{
    void* p;
    size_t s;
};
static std::vector<PointerSizePair> hackPointerSizePair;
static void TrackNewAllocation(void*const p, const size_t size)
{
    if (p == 0)
    {
        return;
    }
    assert(size > 0);

    s_vulkanApiCpuBytesAllocated += size;
    if (s_vulkanApiCpuBytesAllocatedMax < s_vulkanApiCpuBytesAllocated)
    {
        s_vulkanApiCpuBytesAllocatedMax = s_vulkanApiCpuBytesAllocated;
    }

    PointerSizePair psp;
    psp.p = p;
    psp.s = size;
    hackPointerSizePair.push_back(psp);
}
static void TrackDeletedAllocation(void*const p)
{
    if (p == 0)
    {
        return;
    }

    auto& end = hackPointerSizePair.end();
    auto& it = hackPointerSizePair.begin();
    for (; it != end; ++it)
    {
        if (it->p == p)
        {
            s_vulkanApiCpuBytesAllocated -= it->s;
            hackPointerSizePair.erase(it);
            break;
        }
    }
    assert(it != end);
}
//END_HAC_#AllocationTracking
#endif//#if NTF_DEBUG
static void* VKAPI_CALL NTF_vkAllocationFunction(
    void*                                       pUserData,
    size_t                                      size,
    size_t                                      alignment,
    VkSystemAllocationScope                     allocationScope)
{
    void*const ret = AlignedMalloc(size, alignment);
#if NTF_DEBUG
    TrackNewAllocation(ret, size);
    //printf("NTF_vkAllocationFunction  :s_vulkanApiCpuBytesAllocated=%zu\n", s_vulkanApiCpuBytesAllocated);
#endif//#if NTF_DEBUG
    return ret;
}

static void* VKAPI_CALL NTF_vkReallocationFunction(
    void*                                       pUserData,
    void*                                       pOriginal,
    size_t                                      size,
    size_t                                      alignment,
    VkSystemAllocationScope                     allocationScope)
{
    void*const ret = AlignedRealloc(pOriginal, size, alignment);
#if NTF_DEBUG
    TrackDeletedAllocation(pOriginal);
    TrackNewAllocation(ret, size);
    //printf("NTF_vkReallocationFunction:s_vulkanApiCpuBytesAllocated=%zu\n", s_vulkanApiCpuBytesAllocated);
#endif//#if NTF_DEBUG
    return ret;
}

static void VKAPI_CALL NTF_vkFreeFunction(
    void*                                       pUserData,
    void*                                       pMemory)
{
#if NTF_DEBUG
    TrackDeletedAllocation(pMemory);
    //printf("NTF_vkFreeFunction        :s_vulkanApiCpuBytesAllocated=%zu\n", s_vulkanApiCpuBytesAllocated);
#endif//#if NTF_DEBUG
    AlignedFree(pMemory);
}

static void VKAPI_CALL NTF_vkInternalAllocationNotification(
    void*                                       pUserData,
    size_t                                      size,
    VkInternalAllocationType                    allocationType,
    VkSystemAllocationScope                     allocationScope)
{
    assert(false);
}

static void VKAPI_CALL NTF_vkInternalFreeNotification(
    void*                                       pUserData,
    size_t                                      size,
    VkInternalAllocationType                    allocationType,
    VkSystemAllocationScope                     allocationScope)
{
    assert(false);
}
//END_#AllocationCallbacks

//BEG_#SecondaryCommandBufferMultithreading
HANDLE ThreadSignalingEventCreate()
{
    return CreateEvent(
        NULL,               // default security attributes
        FALSE,              // auto-reset; after signaling immediately set to nonsignaled
        FALSE,              // initial state is nonsignaled
        NULL                // no name -- if you have two events with the same name, the more recent one stomps the less recent one
    );
}

void CommandBufferSecondaryThreadsCreate(
    ArraySafeRef<CommandBufferSecondaryThread> threadData,
    ArraySafeRef<HANDLE> threadDoneEvents,
    ArraySafeRef<CommandBufferThreadArguments> threadArguments,
    const size_t threadsNum)
{
    const unsigned int threadsHardwareNum = std::thread::hardware_concurrency();
    assert(threadsHardwareNum > 0);
    //BEG_THREADING_HACK
    ///@todo: cleanly handle any number of nonzero threads
    assert(threadsHardwareNum >= threadsNum);
    //END_THREADING_HACK

    for (size_t threadIndex = 0; threadIndex < threadsNum; ++threadIndex)
    {
        auto& threadHandle = threadData[threadIndex].threadHandle;
        auto& commandBufferThreadArguments = threadArguments[threadIndex];

        auto& commandBufferThreadWake = threadData[threadIndex].wakeEventHandle;
        commandBufferThreadWake = ThreadSignalingEventCreate();
        commandBufferThreadArguments.commandBufferThreadWake = &commandBufferThreadWake;

        auto& commandBufferThreadDone = threadDoneEvents[threadIndex];
        commandBufferThreadDone = ThreadSignalingEventCreate();
        commandBufferThreadArguments.commandBufferThreadDone = &commandBufferThreadDone;

        threadHandle = CreateThread(
            nullptr,                                        //child processes irrelevant; no suspending or resuming privileges
            0,                                              //default stack size
            CommandBufferThread,                            //starting address to execute
            &commandBufferThreadArguments,                  //argument
            0,                                              //Run immediately; "commit" (eg map) stack memory for immediate use
            nullptr);                                       //ignore thread id
        assert(threadHandle);///@todo: investigate SetThreadPriority() if default priority (THREAD_PRIORITY_NORMAL) seems inefficient
    }
    ///@todo: CloseHandle() Shutdown
}

DWORD WINAPI CommandBufferThread(void* arg)
{
    auto& commandBufferThreadArguments = *reinterpret_cast<CommandBufferThreadArguments*>(arg);
    for (;;)
    {
        HANDLE& commandBufferThreadWake = *commandBufferThreadArguments.commandBufferThreadWake;

        //#Wait
        //WaitOnAddress(&signalMemory, &undesiredValue, sizeof(CommandBufferThreadArguments::SignalMemoryType), INFINITE);//#SynchronizationWindows8+Only
        DWORD waitForSingleObjectResult = WaitForSingleObject(commandBufferThreadWake, INFINITE);
        assert(waitForSingleObjectResult == WAIT_OBJECT_0);

        VkCommandBuffer& commandBufferSecondary = *commandBufferThreadArguments.commandBuffer;
        VkDescriptorSet& descriptorSet = *commandBufferThreadArguments.descriptorSet;
        VkRenderPass& renderPass = *commandBufferThreadArguments.renderPass;
        VkExtent2D& swapChainExtent = *commandBufferThreadArguments.swapChainExtent;
        VkPipelineLayout& pipelineLayout = *commandBufferThreadArguments.pipelineLayout;
        VkBuffer& vertexBuffer = *commandBufferThreadArguments.vertexBuffer;
        VkBuffer& indexBuffer = *commandBufferThreadArguments.indexBuffer;
        VkFramebuffer& swapChainFramebuffer = *commandBufferThreadArguments.swapChainFramebuffer;
        uint32_t& objectIndex = *commandBufferThreadArguments.objectIndex;
        uint32_t& indicesNum = *commandBufferThreadArguments.indicesNum;
        VkPipeline& graphicsPipeline = *commandBufferThreadArguments.graphicsPipeline;
        HANDLE& commandBufferThreadDone = *commandBufferThreadArguments.commandBufferThreadDone;

        VkCommandBufferInheritanceInfo commandBufferInheritanceInfo;
        commandBufferInheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO; //only option
        commandBufferInheritanceInfo.pNext = nullptr;                                           //only option
        commandBufferInheritanceInfo.renderPass = renderPass;                                   //only executes in this renderpass
        commandBufferInheritanceInfo.subpass = 0;                                               //only executes in this subpass
        commandBufferInheritanceInfo.framebuffer = swapChainFramebuffer;                        //framebuffer to execute in
        commandBufferInheritanceInfo.occlusionQueryEnable = VK_FALSE;                           //can't execute in an occlusion query
        commandBufferInheritanceInfo.queryFlags = 0;                                            //no occlusion query flags
        commandBufferInheritanceInfo.pipelineStatistics = 0;                                    //no query counter operations

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        beginInfo.pInheritanceInfo = &commandBufferInheritanceInfo;

        const VkBuffer vertexBuffers[] = { vertexBuffer };
        const VkDeviceSize offsets[] = { 0 };

        vkBeginCommandBuffer(commandBufferSecondary, &beginInfo);  //implicitly resets the command buffer (you can't append commands to an existing buffer)
        vkCmdBindPipeline(commandBufferSecondary, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        vkCmdBindVertexBuffers(commandBufferSecondary, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBufferSecondary, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(commandBufferSecondary, VK_PIPELINE_BIND_POINT_GRAPHICS/*graphics not compute*/, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        vkCmdPushConstants(commandBufferSecondary, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantBindIndexType), &objectIndex);
        vkCmdDrawIndexed(commandBufferSecondary, indicesNum, 1, 0, 0, 0);

        const VkResult endCommandBufferResult = vkEndCommandBuffer(commandBufferSecondary);
        NTF_VK_ASSERT_SUCCESS(endCommandBufferResult);

        const BOOL setEventResult = SetEvent(commandBufferThreadDone);
        assert(setEventResult);
    }
}
//END_#SecondaryCommandBufferMultithreading

void CreateTextureImageView(VkImageView*const textureImageViewPtr, const VkImage& textureImage, const VkDevice& device)
{
    assert(textureImageViewPtr);
    auto& textureImageView = *textureImageViewPtr;

    CreateImageView(&textureImageView, device, textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
}

void CopyBufferToImage(
    const VkBuffer& buffer,
    const VkImage& image,
    const uint32_t width,
    const uint32_t height,
    const VkCommandBuffer& commandBuffer,
    const VkDevice& device)
{
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;//extra row padding; 0 indicates tightly packed
    region.bufferImageHeight = 0;//extra height padding; 0 indicates tightly packed
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width,height,1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void ImageMemoryBarrier(
    const VkImageLayout& oldLayout,
    const VkImageLayout& newLayout,
    const uint32_t srcQueueFamilyIndex, 
    const uint32_t dstQueueFamilyIndex,
    const VkImageAspectFlagBits& aspectMask,
    const VkImage& image,
    const VkAccessFlags& srcAccessMask, 
    const VkAccessFlags& dstAccessMask, 
    const VkPipelineStageFlags& srcStageMask,
    const VkPipelineStageFlags& dstStageMask,
    const VkCommandBuffer& commandBuffer)
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    barrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;

    //not an array and has no mipmapping levels
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask,//perform source operation immediately (and not at some later stage, like the vertex shader or fragment shader),
        dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1,
        &barrier);
}

VkResult SubmitCommandBuffer(
    ConstVectorSafeRef<VkSemaphore> signalSemaphores,
    ConstVectorSafeRef<VkSemaphore> waitSemaphores,
    ArraySafeRef<VkPipelineStageFlags> waitStages,///<@todo: ConstArraySafeRef
    const VkCommandBuffer& commandBuffer,
    const VkQueue& queue,
    const VkFence& fence)
{
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    submitInfo.signalSemaphoreCount = Cast_size_t_uint32_t(signalSemaphores.size());
    submitInfo.pSignalSemaphores = signalSemaphores.GetAddressOfUnderlyingArray();

    submitInfo.waitSemaphoreCount = Cast_size_t_uint32_t(waitSemaphores.size());
    submitInfo.pWaitSemaphores = waitSemaphores.GetAddressOfUnderlyingArray();
    submitInfo.pWaitDstStageMask = waitStages.GetAddressOfUnderlyingArray();

    const VkResult queueSubmitResult = vkQueueSubmit(queue, 1, &submitInfo, fence);
    NTF_VK_ASSERT_SUCCESS(queueSubmitResult);
    return queueSubmitResult;
}

void TransferImageFromCpuToGpu(
    const VkImage& image,
    const uint32_t width,
    const uint32_t height,
    const VkFormat& format,
    const VkBuffer& stagingBuffer,
    const VkCommandBuffer commandBufferTransfer,
    const VkQueue& transferQueue,
    const uint32_t transferQueueFamilyIndex,
    const VkCommandBuffer commandBufferGraphics,
    const VkQueue& graphicsQueue,
    const uint32_t graphicsQueueFamilyIndex,
    const VkDevice& device)
{
    const bool unifiedGraphicsAndTransferQueue = graphicsQueue == transferQueue;
    assert(unifiedGraphicsAndTransferQueue == (transferQueueFamilyIndex == graphicsQueueFamilyIndex));

    //transition memory to format optimal for copying from CPU->GPU
    ImageMemoryBarrier(
        VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        transferQueueFamilyIndex, 
        transferQueueFamilyIndex, 
        VK_IMAGE_ASPECT_COLOR_BIT,
        image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,//perform source operation immediately (and not at some later stage, like the vertex shader or fragment shader), 
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        commandBufferTransfer);

    CopyBufferToImage(stagingBuffer, image, width, height, commandBufferTransfer, device);    
    if (unifiedGraphicsAndTransferQueue)
    {
        //transferQueue == graphicsQueue, so prepare image for shader reads with no change of queue ownership
        ImageMemoryBarrier(
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            transferQueueFamilyIndex,
            transferQueueFamilyIndex,
            VK_IMAGE_ASPECT_COLOR_BIT,
            image,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            commandBufferTransfer);
    }
    else
    {
        //transition resource ownership from transfer queue to graphics queue
        ImageMemoryBarrier(
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            transferQueueFamilyIndex,
            graphicsQueueFamilyIndex,
            VK_IMAGE_ASPECT_COLOR_BIT,
            image,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            0,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            commandBufferTransfer);

        //prepare texture for shader reads
        ImageMemoryBarrier(
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            transferQueueFamilyIndex,
            graphicsQueueFamilyIndex,
            VK_IMAGE_ASPECT_COLOR_BIT,
            image,
            0,
            VK_ACCESS_SHADER_READ_BIT,//specifies read access to a storage buffer, uniform texel buffer, storage texel buffer, sampled image, or storage image.
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            commandBufferGraphics);
    }
}

bool CreateAllocateBindImageIfAllocatorHasSpace(
    VkImage*const imagePtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    VkDeviceSize*const alignmentPtr,
    const uint32_t width,
    const uint32_t height,
    const VkFormat& format,
    const VkImageTiling& tiling,
    const VkImageUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const bool residentForever,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    assert(imagePtr);
    auto& image = *imagePtr;

    assert(allocatorPtr);
    auto& allocator = *allocatorPtr;

    NTF_REF(alignmentPtr, alignment);

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;//used by only one queue family at a time

    const VkResult createImageResult = vkCreateImage(device, &imageInfo, GetVulkanAllocationCallbacks(), &image);
    NTF_VK_ASSERT_SUCCESS(createImageResult);

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);
    alignment = memRequirements.alignment;

    VkDeviceSize memoryOffset;
    VkDeviceMemory memoryHandle;
    const bool allocateMemoryResult = allocator.PushAlloc(
        &memoryOffset,
        &memoryHandle,
        memRequirements,
        properties,
        residentForever, 
        tiling == VK_IMAGE_TILING_LINEAR,
        device,
        physicalDevice);
    if (allocateMemoryResult)
    {
        vkBindImageMemory(device, image, memoryHandle, memoryOffset);
    }
    else
    {
        vkDestroyImage(device, image, GetVulkanAllocationCallbacks());
    }

    return allocateMemoryResult;
}

void CopyBuffer(const VkBuffer& srcBuffer,const VkBuffer& dstBuffer,const VkDeviceSize& size, const VkCommandBuffer commandBuffer)
{
    VkBufferCopy copyRegion = {};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
}

/* Heap classification:
1. vkGetPhysicalDeviceMemoryProperties​() returns memoryHeapCount, memoryTypes (what types of memory are supported by each heap), and the memoryHeaps themselves
2. vkGetImageMemoryRequirements() or vkGetBufferMemoryRequirements​() return a bitmask for each resource.  If this bitmask shares a bit with the index of a 
   given memoryTypes::propertyFlags, then the heap indexed by the corresponding memoryTypes::heapIndex supports this resource
3. of the subset of heaps defined by step 2., you can choose the heap that contains the desired memoryTypes::propertyFlags​(eg 
   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, etc)​ */
///@ret: index of of VkPhysicalDeviceMemoryProperties::memProperties.memoryTypes that maps to the user's arguments
uint32_t FindMemoryType(const uint32_t typeFilter, const VkMemoryPropertyFlags& properties, const VkPhysicalDevice& physicalDevice)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    assert(false);//failed to find suitable memory type
    return -1;
}
uint32_t FindMemoryHeapIndex(const VkMemoryPropertyFlags& properties, const VkPhysicalDevice& physicalDevice)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < memProperties.memoryTypeCount; ++memoryTypeIndex)
    {
        auto& memoryType = memProperties.memoryTypes[memoryTypeIndex];
        if (memoryType.propertyFlags & properties)
        {
            return memoryType.heapIndex;//assume the first heap found that satisfies properties is the only one
        }
    }

    assert(false);//failed to find heap
    return -1;
}

void CreateBuffer(VkBuffer*const vkBufferPtr, const VkDeviceSize& vkBufferSizeBytes, const VkBufferUsageFlags& usage, const VkDevice& device)
{
    NTF_REF(vkBufferPtr, vkBuffer);

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = vkBufferSizeBytes;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    const VkResult createBufferResult = vkCreateBuffer(device, &bufferInfo, GetVulkanAllocationCallbacks(), &vkBuffer);
    NTF_VK_ASSERT_SUCCESS(createBufferResult);
}

///user is responsible for ensuring the VkDeviceMemory was allocated from the heap that supports all operations this buffer is intended for
void CreateBuffer(
    VkBuffer*const vkBufferPtr,
    const VkDeviceMemory& vkBufferMemory,
    const VkDeviceSize& offsetToAllocatedBlock,
    const VkDeviceSize& vkBufferSizeBytes,
    const VkMemoryPropertyFlags& flags,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    NTF_REF(vkBufferPtr, vkBuffer);

    CreateBuffer(&vkBuffer, vkBufferSizeBytes, flags, device);

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, vkBuffer, &memoryRequirements);
    assert(memoryRequirements.size >= vkBufferSizeBytes);

    const VkResult bindBufferResult = vkBindBufferMemory(device, vkBuffer, vkBufferMemory, offsetToAllocatedBlock);
    NTF_VK_ASSERT_SUCCESS(bindBufferResult);
}
void CreateBuffer(
    VkBuffer*const bufferPtr,
    VkDeviceMemory*const bufferMemoryPtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    VkDeviceSize*const offsetToAllocatedBlockPtr,
    const VkDeviceSize& size,
    const VkBufferUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const bool residentForever,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    assert(bufferPtr);
    VkBuffer& buffer = *bufferPtr;

    assert(bufferMemoryPtr);
    auto& bufferMemory = *bufferMemoryPtr;

    assert(allocatorPtr);
    auto& allocator = *allocatorPtr;

    assert(offsetToAllocatedBlockPtr);
    auto& offsetToAllocatedBlock = *offsetToAllocatedBlockPtr;

    CreateBuffer(&buffer, size, usage, device);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    const bool allocateMemoryResult = allocator.PushAlloc(
        &offsetToAllocatedBlock,
        &bufferMemory,
        memRequirements,
        properties,
        residentForever,
        true,
        device,
        physicalDevice);
    assert(allocateMemoryResult);

    const VkResult bindBufferResult = vkBindBufferMemory(device, buffer, bufferMemory, offsetToAllocatedBlock);
    NTF_VK_ASSERT_SUCCESS(bindBufferResult);
}

VkFormat FindDepthFormat(const VkPhysicalDevice& physicalDevice)
{
    VectorSafe<VkFormat, 3> candidates =
    {
        VK_FORMAT_D32_SFLOAT, /**<*32bit depth*/
        VK_FORMAT_D32_SFLOAT_S8_UINT, /**<*32bit depth, 8bit stencil*/
        VK_FORMAT_D24_UNORM_S8_UINT/**<*24bit depth, 8bit stencil*/
    };
    return FindSupportedFormat(physicalDevice, candidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void CreateShaderModule(VkShaderModule*const shaderModulePtr, char*const code, const size_t codeSizeBytes, const VkDevice& device)
{
    assert(shaderModulePtr);
    VkShaderModule& shaderModule = *shaderModulePtr;

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSizeBytes;
    createInfo.pCode = reinterpret_cast<uint32_t*>(code);

    const VkResult createShaderModuleResult = vkCreateShaderModule(device, &createInfo, GetVulkanAllocationCallbacks(), &shaderModule);
    NTF_VK_ASSERT_SUCCESS(createShaderModuleResult);
}

bool CheckValidationLayerSupport(ConstVectorSafeRef<const char*> validationLayers)
{
    const int layersMax = 32;
    uint32_t layerCount;
    {
        const VkResult enumerateInstanceLayerPropertiesResult = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        NTF_VK_ASSERT_SUCCESS(enumerateInstanceLayerPropertiesResult);
    }

    VectorSafe<VkLayerProperties, layersMax> availableLayers(layerCount);
    {
        const VkResult enumerateInstanceLayerPropertiesResult = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        NTF_VK_ASSERT_SUCCESS(enumerateInstanceLayerPropertiesResult);
        availableLayers.size(layerCount);
    }

    for (const char*const layerName : validationLayers)
    {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

void CreateImageView(VkImageView*const imageViewPtr, const VkDevice& device, const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectFlags)
{
    assert(imageViewPtr);
    auto& imageView = *imageViewPtr;

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;//2D texture (not 1D or 3D textures, or a cubemap)
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    const VkResult createImageViewResult = vkCreateImageView(device, &viewInfo, GetVulkanAllocationCallbacks(), &imageView);
    NTF_VK_ASSERT_SUCCESS(createImageViewResult);
}

void ReadFile(char**const fileData, StackCpu*const allocatorPtr, size_t*const fileSizeBytesPtr, const char*const filename)
{
    assert(fileData);

    assert(allocatorPtr);
    auto& allocator = *allocatorPtr;

    assert(fileSizeBytesPtr);
    auto& fileSizeBytes = *fileSizeBytesPtr;

    FILE* f;
    fopen_s(&f, filename, "rb");
    assert(f);

    struct stat fileInfo;
    const int fileStatResult = stat(filename, &fileInfo);
    assert(fileStatResult == 0);

    fileSizeBytes = fileInfo.st_size;
    allocator.PushAlloc(reinterpret_cast<void**>(fileData), 0, fileSizeBytes);
    const size_t freadResult = fread(*fileData, 1, fileSizeBytes, f);
    assert(freadResult == fileInfo.st_size);
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char* layerPrefix,
    const char* msg,
    void* userData) 
{

    std::cerr << std::endl << "validation layer: " << msg << std::endl;

    return VK_FALSE;
}

VkResult CreateDebugReportCallbackEXT(
    VkInstance instance,
    const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugReportCallbackEXT* pCallback)
{
    auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pCallback);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
    if (func != nullptr)
    {
        func(instance, callback, pAllocator);
    }
}

/*static*/VkVertexInputBindingDescription Vertex::GetBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//not using instanced rendering, so index vertex attributes by vertex, not instance

    return bindingDescription;
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

bool Vertex::operator==(const Vertex& other) const
{
    return pos == other.pos && color == other.color && texCoord == other.texCoord;
}

void BeginCommands(const VkCommandBuffer& commandBuffer, const VkDevice& device)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
}

size_t std::hash<Vertex>::operator()(Vertex const& vertex) const
{
    return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
}

bool CheckDeviceExtensionSupport(const VkPhysicalDevice& physicalDevice, ConstVectorSafeRef<const char*> deviceExtensions)
{
    uint32_t supportedExtensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedExtensionCount, nullptr);

    const size_t maxExtensionCount = 256;
    if (supportedExtensionCount > maxExtensionCount)
    {
        return false;
    }
    VectorSafe<VkExtensionProperties, maxExtensionCount> supportedExtensions(supportedExtensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedExtensionCount, supportedExtensions.data());

    const char*const extensionSupportedSymbol = 0;
    VectorSafe<const char*, NTF_DEVICE_EXTENSIONS_NUM> requiredExtensions(deviceExtensions);
    const size_t requiredExtensionsSize = requiredExtensions.size();
    for (VkExtensionProperties supportedExtension : supportedExtensions)
    {
        for (size_t requiredExtensionsIndex = 0; requiredExtensionsIndex < requiredExtensionsSize; ++requiredExtensionsIndex)
        {
            const char*const requiredExtensionName = requiredExtensions[requiredExtensionsIndex];
            if (requiredExtensionName != extensionSupportedSymbol &&
                strcmp(requiredExtensionName, supportedExtension.extensionName) == 0)
            {
                requiredExtensions[requiredExtensionsIndex] = extensionSupportedSymbol;
                break;
            }
        }
    }

    for (const char*const requiredExtension : requiredExtensions)
    {
        if (requiredExtension != extensionSupportedSymbol)
        {
            return false;//a required supportedExtension was not supported
        }
    }
    return true;//all required extensions are supported
}

bool IsDeviceSuitable(
    const VkPhysicalDevice& physicalDevice,
    const VkSurfaceKHR& surface,
    ConstVectorSafeRef<const char*> deviceExtensions)
{
    QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
    const bool extensionsSupported = CheckDeviceExtensionSupport(physicalDevice, deviceExtensions);
    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport;
        QuerySwapChainSupport(&swapChainSupport, surface, physicalDevice);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

    return indices.IsComplete() && extensionsSupported && supportedFeatures.samplerAnisotropy;
}

bool PickPhysicalDevice(
    VkPhysicalDevice*const physicalDevicePtr,
    const VkSurfaceKHR& surface,
    ConstVectorSafeRef<const char*> deviceExtensions,
    const VkInstance& instance)
{
    assert(physicalDevicePtr);
    VkPhysicalDevice& physicalDevice = *physicalDevicePtr;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        //failed to find GPUs with Vulkan support
        assert(false);
        return false;
    }

    const uint32_t deviceMax = 8;
    VectorSafe<VkPhysicalDevice, deviceMax> devices;
    devices.size(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    devices.size(deviceCount);
    for (const VkPhysicalDevice& device : devices)
    {
        if (IsDeviceSuitable(device, surface, deviceExtensions))
        {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE)
    {
        //failed to find a suitable GPU
        assert(false);
        return false;
    }

    return true;
}

void CreateLogicalDevice(
    VkDevice*const devicePtr,
    VkQueue*const graphicsQueuePtr,
    VkQueue*const presentQueuePtr,
    VkQueue*const transferQueuePtr,
    ConstVectorSafeRef<const char*> deviceExtensions,
    ConstVectorSafeRef<const char*> validationLayers,
    const VkSurfaceKHR& surface,
    const VkPhysicalDevice& physicalDevice)
{
    assert(graphicsQueuePtr);
    VkQueue& graphicsQueue = *graphicsQueuePtr;

    assert(presentQueuePtr);
    VkQueue& presentQueue = *presentQueuePtr;

    assert(transferQueuePtr);
    VkQueue& transferQueue = *transferQueuePtr;

    assert(devicePtr);
    auto& device = *devicePtr;

    QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);

    const uint32_t queueFamiliesNum = 3;
    VectorSafe<VkDeviceQueueCreateInfo, queueFamiliesNum> queueCreateInfos(0);
    VectorSafe<int, queueFamiliesNum> uniqueQueueFamilies({ indices.graphicsFamily, indices.presentFamily, indices.transferFamily });
    SortAndRemoveDuplicatesFromVectorSafe(&uniqueQueueFamilies);

    const float queuePriority = 1.0f;
    for (const int queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.Push(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = true;

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());//require swapchain extension
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();//require swapchain extension

    if (s_enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    const VkResult createDeviceResult = vkCreateDevice(physicalDevice, &createInfo, GetVulkanAllocationCallbacks(), &device);
    NTF_VK_ASSERT_SUCCESS(createDeviceResult);

    vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
    vkGetDeviceQueue(device, indices.transferFamily, 0, &transferQueue);
}

void DescriptorTypeAssertOnInvalid(const VkDescriptorType descriptorType)
{
    assert(descriptorType >= VK_DESCRIPTOR_TYPE_BEGIN_RANGE && descriptorType <= VK_DESCRIPTOR_TYPE_END_RANGE);
}

void CreateDescriptorSetLayout(
    VkDescriptorSetLayout*const descriptorSetLayoutPtr, 
    const VkDescriptorType descriptorType, 
    const VkDevice& device, 
    const uint32_t texturesNum)
{
    assert(descriptorSetLayoutPtr);
    VkDescriptorSetLayout& descriptorSetLayout = *descriptorSetLayoutPtr;

    DescriptorTypeAssertOnInvalid(descriptorType);

    uint32_t bindingIndex = 0;
    VkDescriptorSetLayoutBinding uboLayoutBinding = {};
    uboLayoutBinding.binding = bindingIndex++;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = descriptorType;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = bindingIndex++;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;///@todo: consider using this; immutable samplers compile sampler into shader, reducing latency in shader (on AMD the Scalar Arithmetic Logic Unit [SALU] is often underutilized, and is used to construct immutable samplers)
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding sampledImageLayoutBinding = {};
    sampledImageLayoutBinding.binding = bindingIndex++;
    sampledImageLayoutBinding.descriptorCount = texturesNum;
    sampledImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    sampledImageLayoutBinding.pImmutableSamplers = nullptr;///@todo: consider using this; immutable samplers compile sampler into shader, reducing latency in shader (on AMD the Scalar Arithmetic Logic Unit [SALU] is often underutilized, and is used to construct immutable samplers)
    sampledImageLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VectorSafe<VkDescriptorSetLayoutBinding, 3> bindings({ uboLayoutBinding,samplerLayoutBinding,sampledImageLayoutBinding });

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    const VkResult createDescriptorSetLayoutResult = vkCreateDescriptorSetLayout(device, &layoutInfo, GetVulkanAllocationCallbacks(), &descriptorSetLayout);
    NTF_VK_ASSERT_SUCCESS(createDescriptorSetLayoutResult);
}

void CreateGraphicsPipeline(
    VkPipelineLayout*const pipelineLayoutPtr,
    VkPipeline*const graphicsPipelinePtr,
    StackCpu*const allocatorPtr,
    const VkRenderPass& renderPass,
    const VkDescriptorSetLayout& descriptorSetLayout,
    const VkExtent2D& swapChainExtent,
    const VkDevice& device)
{
    assert(pipelineLayoutPtr);
    VkPipelineLayout& pipelineLayout = *pipelineLayoutPtr;
    
    assert(graphicsPipelinePtr);
    VkPipeline& graphicsPipeline = *graphicsPipelinePtr;
    
    assert(allocatorPtr);
    auto& allocator = *allocatorPtr;

    char* vertShaderCode;
    char* fragShaderCode;
    size_t vertShaderCodeSizeBytes, fragShaderCodeSizeBytes;
    assert(allocator.GetFirstByteFree() == 0);//ensure we can Clear() the whole stack correctly (eg there's nothing already allocated in the stack)
    ReadFile(&vertShaderCode, &allocator, &vertShaderCodeSizeBytes, "shaders/vert.spv");
    ReadFile(&fragShaderCode, &allocator, &fragShaderCodeSizeBytes, "shaders/frag.spv");

    //create wrappers around SPIR-V bytecodes
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    CreateShaderModule(&vertShaderModule, vertShaderCode, vertShaderCodeSizeBytes, device);
    CreateShaderModule(&fragShaderModule, fragShaderCode, fragShaderCodeSizeBytes, device);

    allocator.Clear();

    //vertex shader creation
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";
    vertShaderStageInfo.pSpecializationInfo = nullptr;//shader constants can be specified here -- for example, a shader might have several different behaviors that are arbitrated between by a constant; dead code will be stripped away at pipline creation time

    //fragment shader creation
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    fragShaderStageInfo.pSpecializationInfo = nullptr;//shader constants can be specified here -- for example, a shader might have several different behaviors that are arbitrated between by a constant; dead code will be stripped away at pipline creation time

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription = Vertex::GetBindingDescription();
    const int attributeDescriptionsSize = 3;
    VectorSafe<VkVertexInputAttributeDescription, attributeDescriptionsSize> attributeDescriptions(attributeDescriptionsSize);
    Vertex::GetAttributeDescriptions(&attributeDescriptions);

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    //specify triangle list
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    //use entire depth range
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    //no scissor screenspace-culling
    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;         //fragments beyond near or far planes should be culled, and not clamped to these planes (enabling this requires enabling the corresponding GPU feature)
    rasterizer.rasterizerDiscardEnable = VK_FALSE;  //don't discard all geometry
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;  //any other setting (eg wireframe or point rendering) requires enabling the corresponding GPU feature
    rasterizer.lineWidth = 1.0f;                    //any setting greater than 1 requires enabling the wideLines GPU feature

    //standard backface culling
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    //no depth biasing (for example, might be used to help with peter-panning issues in projected shadows)
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    //no MSAA -- enabling it requires enabling the corresponding GPU feature
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; /// Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    //allows you to only keep fragments that fall within a specific depth-range
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional

    //stencil not being used
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {}; // Optional
    depthStencil.back = {}; // Optional

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;//no blending

    ////additive alpha
    //colorBlendAttachment.blendEnable = VK_TRUE;
    //colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    //colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    //colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    //colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    //colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    //colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    /*  implements the following:
        finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
        finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);*/
    //colorBlendAttachment.blendEnable = VK_TRUE;
    //colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    //colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    //colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    //colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    //colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    //colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    ////standard alpha blending
    //colorBlendAttachment.blendEnable = VK_TRUE;
    //colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    //colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    //colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    //colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    //colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    //colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    //sets blend constants for any blend operations defined above for the entire pipeline
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE; //no logic op -- if this is set to true then it automatically sets (colorBlendAttachment.blendEnable = false)
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    ////dynamic states can be changed without recreating the pipeline!
    //VkDynamicState dynamicStates[] = {
    //    VK_DYNAMIC_STATE_VIEWPORT,
    //    VK_DYNAMIC_STATE_LINE_WIDTH
    //};
    //VkPipelineDynamicStateCreateInfo dynamicState = {};
    //dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    //dynamicState.dynamicStateCount = 2;
    //dynamicState.pDynamicStates = dynamicStates;

    //allows setting of uniform values across all shaders, like local-to-world matrix for vertex shader and texture samplers for fragment shader
    VkPushConstantRange pushConstantRange;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstantBindIndexType);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    const VkResult createPipelineLayoutResult = vkCreatePipelineLayout(device, &pipelineLayoutInfo, GetVulkanAllocationCallbacks(), &pipelineLayout);
    NTF_VK_ASSERT_SUCCESS(createPipelineLayoutResult);

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.pDepthStencilState = &depthStencil;

    const VkResult createGraphicsPipelineResult = 
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, GetVulkanAllocationCallbacks(), &graphicsPipeline);
    NTF_VK_ASSERT_SUCCESS(createGraphicsPipelineResult);

    vkDestroyShaderModule(device, fragShaderModule, GetVulkanAllocationCallbacks());
    vkDestroyShaderModule(device, vertShaderModule, GetVulkanAllocationCallbacks());
}

void CreateRenderPass(
    VkRenderPass*const renderPassPtr,
    const VkFormat& swapChainImageFormat,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice
    )
{
    assert(renderPassPtr);
    VkRenderPass& renderPass = *renderPassPtr;

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;                    //no MSAA
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;               //clear to a constant value defined in VkRenderPassBeginInfo; other options are VK_ATTACHMENT_LOAD_OP_LOAD: Preserve the existing contents of the attachment and VK_ATTACHMENT_LOAD_OP_DONT_CARE: Existing contents are undefined; we don't care about them
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;             //store buffer in memory for later; other option is VK_ATTACHMENT_STORE_OP_DONT_CARE: Contents of the framebuffer will be undefined after the rendering operation
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;    //not doing anything with stencil buffer
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  //not doing anything with stencil buffer
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;          //don't care about what layout the buffer was when we begin the renderpass
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;      //when the renderpass is complete the layout will be ready for presentation in the swap chain
                                                                        /*  other layouts:
                                                                        * VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: Images used as color attachment
                                                                        * VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: Images to be presented in the swap chain
                                                                        * VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : Images to be used as destination for a memory copy operation */

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = FindDepthFormat(physicalDevice);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;//graphics subpass, not compute subpass
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    /*  The operations right before and right after this subpass also count as implicit "subpasses".  There are two
        built-in dependencies that take care of the transition at the start of the render pass and at the end of
        the render pass, but the former does not occur at the right time. It assumes that the transition occurs at the
        start of the pipeline, but we haven't acquired the image yet at that point! There are two ways to deal with
        this problem. We could change the waitStages for the m_imageAvailableSemaphore to VK_PIPELINE_STAGE_TOP_OF_PIPELINE_BIT
        to ensure that the render passes don't begin until the image is available, or we can make the render pass
        wait for the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage; we'll do the latter to illustrate subpasses */
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;//implicit subpass before or after the render pass depending on whether it is specified in srcSubpass or dstSubpass
    dependency.dstSubpass = 0;//must always be higher than srcSubpass to prevent cycles in dependency graph
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    //The operations that should wait on this are in the color attachment stage and involve the reading and writing of 
    //the color attachment.  These settings will prevent the transition from happening until it's actually necessary 
    //(and allowed): when we want to start writing colors to it
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VectorSafe<VkAttachmentDescription, 2> attachments({ colorAttachment,depthAttachment });
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    const VkResult createRenderPassResult = vkCreateRenderPass(device, &renderPassInfo, GetVulkanAllocationCallbacks(), &renderPass);
    NTF_VK_ASSERT_SUCCESS(createRenderPassResult);
}

void AllocateCommandBuffers(
    ArraySafeRef<VkCommandBuffer> commandBuffers,
    const VkCommandPool& commandPool,
    const VkCommandBufferLevel& commandBufferLevel,
    const uint32_t commandBuffersNum,
    const VkDevice& device)
{
    assert(commandBuffersNum > 0);

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;//only value allowed
    allocInfo.commandPool = commandPool;
    allocInfo.level = commandBufferLevel;//primary can submit to execution queue, but not be submitted to other command buffers; secondary can't be submitted to execution queue but can be submitted to other command buffers (for example, to factor out common sequences of commands)
    allocInfo.commandBufferCount = commandBuffersNum;

    const VkResult allocateCommandBuffersResult = vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());
    NTF_VK_ASSERT_SUCCESS(allocateCommandBuffersResult);
}

void FillSecondaryCommandBuffers(
    ArraySafeRef<VkCommandBuffer> commandBuffersSecondary,
    ArraySafeRef<CommandBufferSecondaryThread> commandBuffersSecondaryThreads,
    ArraySafeRef<HANDLE> commandBufferThreadDoneEvents,
    ArraySafeRef<CommandBufferThreadArguments> commandBufferThreadArgumentsArray,
    VkDescriptorSet*const descriptorSet,
    VkFramebuffer*const swapChainFramebuffer,
    VkRenderPass*const renderPass,
    VkExtent2D*const swapChainExtent,
    VkPipelineLayout*const pipelineLayout,
    VkPipeline*const graphicsPipeline,
    VkBuffer*const vertexBuffer,
    VkBuffer*const indexBuffer,
    uint32_t*const indicesSize,
    ArraySafeRef<uint32_t> objectIndex,
    const size_t objectsNum)
{
    assert(descriptorSet);
    assert(swapChainFramebuffer);
    assert(renderPass);
    assert(swapChainExtent);
    assert(pipelineLayout);
    assert(graphicsPipeline);
    assert(vertexBuffer);
    assert(indexBuffer);

    assert(indicesSize);
    assert(*indicesSize > 0);

    assert(objectsNum > 0);

    const size_t threadNum = objectsNum;
    for (size_t threadIndex = 0; threadIndex < threadNum; ++threadIndex)
    {
        auto& commandBufferThreadArguments = commandBufferThreadArgumentsArray[threadIndex];
        commandBufferThreadArguments.commandBuffer = &commandBuffersSecondary[threadIndex];
        commandBufferThreadArguments.descriptorSet = descriptorSet;
        commandBufferThreadArguments.graphicsPipeline = graphicsPipeline;
        commandBufferThreadArguments.indexBuffer = indexBuffer;
        commandBufferThreadArguments.indicesNum = indicesSize;

        objectIndex[threadIndex] = Cast_size_t_uint32_t(threadIndex);
        commandBufferThreadArguments.objectIndex = &objectIndex[threadIndex];

        commandBufferThreadArguments.pipelineLayout = pipelineLayout;
        commandBufferThreadArguments.renderPass = renderPass;
        commandBufferThreadArguments.swapChainExtent = swapChainExtent;
        commandBufferThreadArguments.swapChainFramebuffer = swapChainFramebuffer;
        commandBufferThreadArguments.vertexBuffer = vertexBuffer;

        //#Wait
        //WakeByAddressSingle(commandBufferThreadArguments.signalMemory);//#SynchronizationWindows8+Only
        const BOOL setEventResult = SetEvent(commandBuffersSecondaryThreads[threadIndex].wakeEventHandle);
        assert(setEventResult);
    }
    WaitForMultipleObjects(Cast_size_t_DWORD(threadNum), commandBufferThreadDoneEvents.begin(), TRUE, INFINITE);
}

void FillCommandBufferPrimary(
    const VkCommandBuffer& commandBufferPrimary,
    const ArraySafeRef<TexturedGeometry> texturedGeometries,
    const VkDescriptorSet descriptorSet,
    const VkDeviceSize& uniformBufferCpuAlignment,
    const size_t objectNum,
    const size_t drawCallsPerObjectNum,
    const VkFramebuffer& swapChainFramebuffer,
    const VkRenderPass& renderPass,
    const VkExtent2D& swapChainExtent,
    const VkPipelineLayout& pipelineLayout,
    const VkPipeline& graphicsPipeline,
    const VkDevice& device)
{
    assert(uniformBufferCpuAlignment > 0);
    assert(objectNum > 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  /* options: * VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once
                                                                                * VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be entirely within a single render pass.
                                                                                * VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT : The command buffer can be resubmitted while it is also already pending execution. */
    beginInfo.pInheritanceInfo = nullptr; //specifies what state a secondary buffer should inherit from the primary buffer
    vkBeginCommandBuffer(commandBufferPrimary, &beginInfo);  //implicitly resets the command buffer (you can't append commands to an existing buffer)

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffer;

    //any pixels outside of the area defined here have undefined values; we don't want that
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapChainExtent;

    const size_t kClearValueNum = 2;
    VectorSafe<VkClearValue, kClearValueNum> clearValues(kClearValueNum);
    clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBufferPrimary, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE/**<no secondary buffers will be executed; VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS = secondary command buffers will execute these commands*/);
    vkCmdBindPipeline(commandBufferPrimary, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    //bind a single descriptorset per streaming unit.  Could also bind this descriptor set once at startup time for each primary command buffer, and then leave it bound indefinitely (this behavior was discovered on a UHD graphics 620; haven't tested on other hardware)
    vkCmdBindDescriptorSets(
        commandBufferPrimary,
        VK_PIPELINE_BIND_POINT_GRAPHICS/*graphics not compute*/,
        pipelineLayout,
        0,
        1,
        &descriptorSet,
        0,
        nullptr);
    for (size_t objectIndex = 0; objectIndex < objectNum; ++objectIndex)
    {
        auto& texturedGeometry = texturedGeometries[objectIndex];
        assert(texturedGeometry.Valid());

        VkBuffer vertexBuffers[] = { texturedGeometry.vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBufferPrimary, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBufferPrimary, texturedGeometry.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        for (uint32_t drawCallIndex = 0; drawCallIndex < drawCallsPerObjectNum; ++drawCallIndex)
        {
            const uint32_t pushConstantValue = Cast_size_t_uint32_t(objectIndex*drawCallsPerObjectNum + drawCallIndex);
            vkCmdPushConstants(commandBufferPrimary, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantBindIndexType), &pushConstantValue);
            vkCmdDrawIndexed(commandBufferPrimary, Cast_size_t_uint32_t(texturedGeometry.indicesSize), 1, 0, 0, 0);
        }
    }
    
    vkCmdEndRenderPass(commandBufferPrimary);

    const VkResult endCommandBufferResult = vkEndCommandBuffer(commandBufferPrimary);
    NTF_VK_ASSERT_SUCCESS(endCommandBufferResult);
}
VkDeviceSize UniformBufferCpuAlignmentCalculate(const VkDeviceSize bufferElementSize, const VkPhysicalDevice& physicalDevice)
{
    assert(bufferElementSize > 0);

    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
    const VkDeviceSize minUniformBufferOffsetAlignment = physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;

    VkDeviceSize uniformBufferAlignment = bufferElementSize;
    if (minUniformBufferOffsetAlignment > 0)
    {
        uniformBufferAlignment = RoundToNearest(uniformBufferAlignment, minUniformBufferOffsetAlignment);
    }
    return uniformBufferAlignment;
}

void CreateUniformBuffer(
    ArraySafeRef<uint8_t>*const uniformBufferCpuMemoryPtr,
    VkDeviceMemory*const uniformBufferGpuMemoryPtr,
    VkBuffer*const uniformBufferPtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    VkDeviceSize*const offsetToGpuMemoryPtr,
    const VkDeviceSize bufferSize,
    const bool residentForever,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    assert(uniformBufferPtr);
    auto& uniformBuffer = *uniformBufferPtr;

    assert(uniformBufferGpuMemoryPtr);
    auto& uniformBufferGpuMemory = *uniformBufferGpuMemoryPtr;

    assert(uniformBufferCpuMemoryPtr);
    auto& uniformBufferCpuMemory = *uniformBufferCpuMemoryPtr;

    assert(allocatorPtr);
    auto& allocator = *allocatorPtr;

    assert(offsetToGpuMemoryPtr);
    auto& offsetToGpuMemory = *offsetToGpuMemoryPtr;

    assert(bufferSize > 0);

    CreateBuffer(
        &uniformBuffer,
        &uniformBufferGpuMemory,
        &allocator,
        &offsetToGpuMemory,
        bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        residentForever,
        device,
        physicalDevice);

    void* uniformBufferCpuMemoryCPtr;
    vkMapMemory(device, uniformBufferGpuMemory, offsetToGpuMemory, bufferSize, 0, &uniformBufferCpuMemoryCPtr);
    uniformBufferCpuMemory.SetArray(reinterpret_cast<uint8_t*>(uniformBufferCpuMemoryCPtr), Cast_VkDeviceSize_size_t(bufferSize));
}

void DestroyUniformBuffer(
    ArraySafeRef<uint8_t> uniformBufferCpuMemory,
    const VkDeviceMemory uniformBufferGpuMemory,
    const VkBuffer uniformBuffer,
    const VkDevice& device)
{
    vkUnmapMemory(device, uniformBufferGpuMemory);
    uniformBufferCpuMemory.Reset();
    vkDestroyBuffer(device, uniformBuffer, GetVulkanAllocationCallbacks());
}

void CreateDescriptorPool(
    VkDescriptorPool*const descriptorPoolPtr, 
    const VkDescriptorType descriptorType, 
    const VkDevice& device, 
    const uint32_t texturesNum)
{
    assert(descriptorPoolPtr);
    VkDescriptorPool& descriptorPool = *descriptorPoolPtr;

    assert(texturesNum > 0);

    DescriptorTypeAssertOnInvalid(descriptorType);

    ///@todo NTF: rework this to use the "one giant bound descriptor set with offsets" approach -- probably one descriptor set for each streaming unit
    const size_t kPoolSizesNum = 3;
    VectorSafe<VkDescriptorPoolSize, kPoolSizesNum> poolSizes(kPoolSizesNum);
    poolSizes[0].type = descriptorType;
    poolSizes[0].descriptorCount = 1;///<number of descriptors of this type that can be allocated from this pool amongst all descriptorsets allocated from this pool///<@todo NTF: should be one per frame?
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[1].descriptorCount = 1;///<@todo NTF: should be one per frame?
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[2].descriptorCount = texturesNum;///<@todo NTF: should be n per frame?

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());//number of elements in pPoolSizes
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;//max number of descriptor sets that can be allocated from the pool
    poolInfo.flags = 0;//if you allocate and free descriptors, don't use VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT here because that's abdicating memory allocation to the driver.  Instead use vkResetDescriptorPool() because it amounts to changing an offset for (de)allocation


    const VkResult createDescriptorPoolResult = vkCreateDescriptorPool(device, &poolInfo, GetVulkanAllocationCallbacks(), &descriptorPool);
    NTF_VK_ASSERT_SUCCESS(createDescriptorPoolResult);
}

void WriteDescriptorSet(
    VkWriteDescriptorSet*const descriptorWritePtr, 
    const VkDescriptorSet dstSet, 
    const uint32_t dstBinding, 
    const uint32_t dstArrayElement, 
    const VkDescriptorType descriptorType,
    const uint32_t descriptorCount,
    const void*const pNext,
    const VkDescriptorBufferInfo*const pBufferInfo,
    const VkDescriptorImageInfo*const pImageInfo,
    const VkBufferView*const pTexelBufferView)
{
    NTF_REF(descriptorWritePtr, descriptorWrite);

    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = dstSet;
    descriptorWrite.dstBinding = dstBinding;
    descriptorWrite.dstArrayElement = dstArrayElement;//might be nonzero if part of an array
    descriptorWrite.descriptorType = descriptorType;
    descriptorWrite.descriptorCount = descriptorCount;
    descriptorWrite.pNext = pNext;//non-null if there's an extension

    //one of the following three must be non-null
    descriptorWrite.pBufferInfo = pBufferInfo;//if buffer data
    descriptorWrite.pImageInfo = pImageInfo; //if image or sampler data
    descriptorWrite.pTexelBufferView = pTexelBufferView; //if render view
}

void CreateDescriptorSet(
    VkDescriptorSet*const descriptorSetPtr,
    const VkDescriptorType descriptorType,
    const VkDescriptorSetLayout& descriptorSetLayout,
    const VkDescriptorPool& descriptorPool,
    const VkBuffer& uniformBuffer,
    const VkDeviceSize uniformBufferSize,
    const ArraySafeRef<VkImageView> textureImageViews,
    const size_t texturesNum,
    const VkSampler textureSampler,
    const VkDevice& device)
{
    assert(descriptorSetPtr);
    VkDescriptorSet& descriptorSet = *descriptorSetPtr;

    DescriptorTypeAssertOnInvalid(descriptorType);
    assert(uniformBufferSize > 0);

    assert(texturesNum > 0);

    VkDescriptorSetLayout layouts[] = { descriptorSetLayout };
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = layouts;

    const VkResult allocateDescriptorSetsResult = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
    NTF_VK_ASSERT_SUCCESS(allocateDescriptorSetsResult);

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = uniformBufferSize;

    VkDescriptorImageInfo samplerInfo = {};
    samplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    samplerInfo.imageView = VK_NULL_HANDLE;
    samplerInfo.sampler = textureSampler;

    const size_t texturesMax=2;
    ArraySafe<VkDescriptorImageInfo, texturesMax> imageInfos;
    for (size_t textureIndex = 0; textureIndex < texturesNum; ++textureIndex)
    {
        auto& imageInfo = imageInfos[textureIndex];
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageViews[textureIndex];
        imageInfo.sampler = VK_NULL_HANDLE;///<@todo: should I use the combined sampler/image imageLayout?
    }

    const size_t kDescriptorWritesNum = 3;
    VectorSafe<VkWriteDescriptorSet, kDescriptorWritesNum> descriptorWrites(kDescriptorWritesNum);

    uint32_t bindingIndex = 0;
    WriteDescriptorSet(&descriptorWrites[bindingIndex], descriptorSet, bindingIndex, 0, descriptorType, 1, nullptr, &bufferInfo, nullptr, nullptr);
    bindingIndex++;

    WriteDescriptorSet(
        &descriptorWrites[bindingIndex], 
        descriptorSet, 
        bindingIndex, 
        0, 
        VK_DESCRIPTOR_TYPE_SAMPLER, 
        1, 
        nullptr, 
        nullptr, 
        &samplerInfo, 
        nullptr);
    bindingIndex++;

    WriteDescriptorSet(
        &descriptorWrites[bindingIndex], 
        descriptorSet, 
        bindingIndex, 
        0, 
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 
        Cast_size_t_uint32_t(texturesNum), 
        nullptr, 
        nullptr, 
        imageInfos.data(), 
        nullptr);
    bindingIndex++;

    vkUpdateDescriptorSets(
        device,
        static_cast<uint32_t>(descriptorWrites.size()),
        descriptorWrites.data()/*write to descriptor set*/,
        0, /*copy descriptor sets from one to another*/
        nullptr);
}
void LoadModel(std::vector<Vertex>*const verticesPtr, std::vector<uint32_t>*const indicesPtr, const char*const modelPath, const float uniformScale)
{
    assert(verticesPtr);
    auto& vertices = *verticesPtr;

    assert(indicesPtr);
    auto& indices = *indicesPtr;

    assert(modelPath);

    //BEG_#StreamingMemory: replace OBJ with binary FBX loading
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;///<@todo: streaming memory management
    std::vector<tinyobj::material_t> materials;///<@todo: streaming memory management
    std::string err;///<@todo: streaming memory management

    const bool loadObjResult = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, modelPath);
    assert(loadObjResult);
    //END_#StreamingMemory: replace OBJ with binary FBX loading

    ///@todo: #StreamingMemory: replace this STL with a good, static-memory hashmap
    //build index list and un-duplicate vertices
    std::unordered_map<Vertex, uint32_t> uniqueVertices = {};

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            Vertex vertex = {};

            vertex.pos =
            {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord =
            {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1] //the origin of texture coordinates in Vulkan is the top-left corner, whereas the OBJ format assumes the bottom-left corner
            };

            vertex.color = { 1.0f, 1.0f, 1.0f };

            if (uniqueVertices.count(vertex) == 0)
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }
    if (uniformScale != 1.f)
    {
        for (auto& vertex : vertices)
        {
            vertex.pos *= uniformScale;
        }
    }
}

void CopyBufferToGpuPrepare(
    VulkanPagedStackAllocator*const deviceLocalMemoryPtr,
    VkBuffer*const gpuBufferPtr,
    VkDeviceMemory*const gpuBufferMemoryPtr,
    VkBuffer*const stagingBufferGpuPtr,
    StackCpu*const stagingBufferMemoryMapCpuToGpuPtr,
    size_t*const stagingBufferGpuAllocateIndexPtr,
    StackNTF<VkDeviceSize>*const stagingBufferGpuStackPtr,
    const VkDeviceMemory stagingBufferGpuMemory,
    const VkDeviceSize stagingBufferGpuAlignmentStandard,
    const VkDeviceSize offsetToFirstByteOfStagingBuffer,
    const void*const cpuBufferSource,
    const VkDeviceSize bufferSize,
    const VkMemoryPropertyFlags& memoryPropertyFlags,
    const bool residentForever,
    const VkCommandBuffer& commandBuffer,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    NTF_REF(deviceLocalMemoryPtr, deviceLocalMemory);
    NTF_REF(gpuBufferPtr, gpuBuffer);
    NTF_REF(gpuBufferMemoryPtr, gpuBufferMemory);
    NTF_REF(stagingBufferGpuPtr, stagingBufferGpu);
    NTF_REF(stagingBufferMemoryMapCpuToGpuPtr, stagingBufferMemoryMapCpuToGpu);
    NTF_REF(stagingBufferGpuAllocateIndexPtr, stagingBufferGpuAllocateIndex);
    NTF_REF(stagingBufferGpuStackPtr, stagingBufferGpuStack);

    VkDeviceSize stagingBufferGpuOffsetToAllocatedBlock;
    stagingBufferGpuStack.PushAlloc(&stagingBufferGpuOffsetToAllocatedBlock, stagingBufferGpuAlignmentStandard, bufferSize);
    CreateBuffer(
        &stagingBufferGpu,
        stagingBufferGpuMemory,
        offsetToFirstByteOfStagingBuffer + stagingBufferGpuOffsetToAllocatedBlock,
        bufferSize,
        0,
        device,
        physicalDevice);

#if NTF_DEBUG
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, stagingBufferGpu, &memRequirements);
    assert(memRequirements.alignment == stagingBufferGpuAlignmentStandard);
#endif//#if NTF_DEBUG

    ArraySafeRef<uint8_t> stagingBufferCpuToGpu;
    stagingBufferMemoryMapCpuToGpu.PushAlloc(
        &stagingBufferCpuToGpu,
        Cast_VkDeviceSize_size_t(stagingBufferGpuAlignmentStandard),
        Cast_VkDeviceSize_size_t(bufferSize));
    CreateAndCopyToGpuBuffer(
        &deviceLocalMemory,
        &gpuBuffer,
        &gpuBufferMemory,
        stagingBufferCpuToGpu,
        cpuBufferSource,
        stagingBufferGpu,
        bufferSize,
        memoryPropertyFlags,
        false,
        commandBuffer,
        device,
        physicalDevice);
    ++stagingBufferGpuAllocateIndex;
}

void CreateAndCopyToGpuBuffer(
    VulkanPagedStackAllocator*const allocatorPtr,
    VkBuffer*const gpuBufferPtr,
    VkDeviceMemory*const gpuBufferMemoryPtr,
    ArraySafeRef<uint8_t> stagingBufferMemoryMapCpuToGpu,
    const void*const cpuBufferSource,
    const VkBuffer& stagingBufferGpu,
    const VkDeviceSize bufferSize,
    const VkMemoryPropertyFlags& flags,
    const bool residentForever,
    const VkCommandBuffer& commandBuffer,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    assert(allocatorPtr);
    auto& allocator = *allocatorPtr;

    assert(gpuBufferPtr);
    auto& gpuBuffer = *gpuBufferPtr;

    assert(gpuBufferMemoryPtr);
    auto& gpuBufferMemory = *gpuBufferMemoryPtr;

    assert(cpuBufferSource);
    assert(bufferSize > 0);

    stagingBufferMemoryMapCpuToGpu.MemcpyFromStart(cpuBufferSource, static_cast<size_t>(bufferSize));

    VkDeviceSize dummy;
    CreateBuffer(
        &gpuBuffer,
        &gpuBufferMemory,
        &allocator,
        &dummy,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | flags,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,//most optimal graphics memory
        residentForever,
        device,
        physicalDevice);

    CopyBuffer(stagingBufferGpu, gpuBuffer, bufferSize, commandBuffer);
}

void EndSingleTimeCommandsHackDeleteSoon(const VkCommandBuffer& commandBuffer, const VkQueue& queue, const VkDevice& device)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);//could use a fence, which would allow you to schedule multiple transfers simultaneously and wait for all of them complete, instead of executing one at a time
}

void CreateCommandPool(VkCommandPool*const commandPoolPtr, const uint32_t& queueFamilyIndex, const VkDevice& device, const VkPhysicalDevice& physicalDevice)
{
    assert(commandPoolPtr);
    auto& commandPool = *commandPoolPtr;

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;   //options:  VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often(may change memory allocation behavior)
                                                                        //          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
    const VkResult createCommandPoolResult = vkCreateCommandPool(device, &poolInfo, GetVulkanAllocationCallbacks(), &commandPool);
    NTF_VK_ASSERT_SUCCESS(createCommandPoolResult);
}

void CreateDepthResources(
    VkImage*const depthImagePtr,
    VkImageView*const depthImageViewPtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    const VkExtent2D& swapChainExtent,
    const VkCommandBuffer& commandBuffer,
    const VkQueue& graphicsQueue,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    assert(depthImagePtr);
    auto& depthImage = *depthImagePtr;

    assert(depthImageViewPtr);
    auto& depthImageView = *depthImageViewPtr;

    assert(allocatorPtr);
    auto& allocator = *allocatorPtr;

    VkFormat depthFormat = FindDepthFormat(physicalDevice);

    VkDeviceSize alignment;
    CreateAllocateBindImageIfAllocatorHasSpace(
        &depthImage,
        &allocator,
        &alignment,
        swapChainExtent.width,
        swapChainExtent.height,
        depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true,
        device,
        physicalDevice);

    CreateImageView(&depthImageView, device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    BeginCommands(commandBuffer, device);
    ImageMemoryBarrier(
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,//  | HasStencilComponent(format)) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        depthImage,
        0,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        commandBuffer);
    EndSingleTimeCommandsHackDeleteSoon(commandBuffer, graphicsQueue, device);
}

VkFormat FindSupportedFormat(
    const VkPhysicalDevice& physicalDevice,
    ConstVectorSafeRef<VkFormat> candidates,
    const VkImageTiling& tiling,
    const VkFormatFeatureFlags& features)
{
    for (const VkFormat& format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    assert(false);//failed to find supported format
    return VK_FORMAT_UNDEFINED;
}

bool HasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

/** use when you're done with the data returned from stbi_load(); never call stbi_image_free() directly; only use this function to clear all stack 
    allocations stbi made using the (global) stbAllocatorPtr */
void STBIImageFree(void*const retval_from_stbi_load, StackCpu*const stbAllocatorPtr)
{
    assert(stbAllocatorPtr);
    auto& stbAllocator = *stbAllocatorPtr;

    stbi_image_free(retval_from_stbi_load);
    stbAllocator.Clear();
}

bool CreateImageAndCopyPixelsIfStagingBufferHasSpace(
    VkImage*const imagePtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    VkDeviceSize*const alignmentPtr,
    int*const textureWidthPtr,
    int*const textureHeightPtr,
    StackCpu*const stagingBufferMemoryMapCpuToGpuStackPtr,
    size_t*const imageSizeBytesPtr,
    StackCpu*const stbAllocatorPtr, 
    const char*const texturePathRelative,
    const VkFormat& format,
    const VkImageTiling& tiling,
    const VkImageUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const bool residentForever,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    NTF_REF(imagePtr, image);
    NTF_REF(allocatorPtr, allocator);
    NTF_REF(alignmentPtr, alignment);

    assert(textureWidthPtr);
    auto& textureWidth = *textureWidthPtr;

    assert(textureHeightPtr);
    auto& textureHeight = *textureHeightPtr;

    NTF_REF(stagingBufferMemoryMapCpuToGpuStackPtr, stagingBufferMemoryMapCpuToGpuStack);
    NTF_REF(imageSizeBytesPtr, imageSizeBytes);

    assert(stbAllocatorPtr);
    auto& stbAllocator = *stbAllocatorPtr;

    assert(texturePathRelative);

    int textureChannels;
    assert(stbAllocator.GetFirstByteFree() == 0);//ensure we can Clear() the whole stack correctly in STBIImageFree() (eg there's nothing already allocated in the stack)
    stbi_uc* pixels = stbi_load(texturePathRelative, &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);
    assert(pixels);
    imageSizeBytes = textureWidth * textureHeight * 4;

    CreateAllocateBindImageIfAllocatorHasSpace(
        &image,
        &allocator,
        &alignment,
        textureWidth,
        textureHeight,
        format,
        tiling,
        usage,
        properties,
        residentForever,
        device,
        physicalDevice);

    ArraySafeRef<uint8_t> stagingBufferSuballocatedFromStack;
    const bool stagingBufferHasSpace = 
        stagingBufferMemoryMapCpuToGpuStack.MemcpyIfPushAllocSucceeds(
            &stagingBufferSuballocatedFromStack, 
            pixels, 
            Cast_VkDeviceSize_size_t(alignment), 
            imageSizeBytes);
    STBIImageFree(pixels, &stbAllocator);
    return stagingBufferHasSpace;
}
void CreateTextureImage(
    VkImage*const textureImagePtr,
    VulkanPagedStackAllocator*const allocatorPtr,
    const uint32_t widthPixels,
    const uint32_t heightPixels,
    const VkBuffer& stagingBufferGpu,
    const bool residentForever,
    const VkQueue& transferQueue,
    const VkCommandBuffer& commandBufferTransfer,
    const uint32_t transferQueueFamilyIndex,
    const VkSemaphore transferFinishedSemaphore,
    const VkQueue& graphicsQueue,
    const VkCommandBuffer& commandBufferGraphics,
    const uint32_t graphicsQueueFamilyIndex,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    assert(textureImagePtr);
    auto& textureImage = *textureImagePtr;

    assert(allocatorPtr);
    auto& allocator = *allocatorPtr;

    assert(widthPixels > 0);
    assert(heightPixels > 0);

    VkDeviceSize alignment;
    const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    CreateAllocateBindImageIfAllocatorHasSpace(
        &textureImage,
        &allocator,
        &alignment,
        widthPixels,
        heightPixels,
        imageFormat,
        VK_IMAGE_TILING_OPTIMAL/*could also pass VK_IMAGE_TILING_LINEAR so texels are laid out in row-major order for debugging (less performant)*/,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT/*accessible by shader*/,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        residentForever,
        device,
        physicalDevice);

    TransferImageFromCpuToGpu(
        textureImage,
        widthPixels,
        heightPixels,
        imageFormat,
        stagingBufferGpu,
        commandBufferTransfer,
        transferQueue,
        transferQueueFamilyIndex,
        commandBufferGraphics,
        graphicsQueue,
        graphicsQueueFamilyIndex,
        device);
}

void CreateTextureSampler(VkSampler*const textureSamplerPtr, const VkDevice& device)
{
    assert(textureSamplerPtr);
    auto& textureSampler = *textureSamplerPtr;

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    /* options for addressing modes:
    VK_SAMPLER_ADDRESS_MODE_REPEAT: Repeat the texture when going beyond the image dimensions.
    VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: Like repeat, but inverts the coordinates to mirror the image when going beyond the dimensions.
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: Take the color of the edge closest to the coordinate beyond the image dimensions.
    VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: Like clamp to edge, but instead uses the edge opposite to the closest edge.
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: Return a solid color when sampling beyond the dimensions of the image.
    */
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;//if true, then address in [0, texWidth) and [0, texHeight) ranges; otherwise [0,1) ranges
    samplerInfo.compareEnable = VK_FALSE;//if true, texels will first be compared to a value, and the result of that comparison is used in filtering operations (as in Percentage Closer Filtering for soft shadows)
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    const VkResult createSamplerResult = vkCreateSampler(device, &samplerInfo, GetVulkanAllocationCallbacks(), &textureSampler);
    NTF_VK_ASSERT_SUCCESS(createSamplerResult);
}

void CreateFramebuffers(
    VectorSafeRef<VkFramebuffer> swapChainFramebuffers,
    ConstVectorSafeRef<VkImageView> swapChainImageViews,
    const VkRenderPass& renderPass,
    const VkExtent2D& swapChainExtent,
    const VkImageView& depthImageView,
    const VkDevice& device)
{
    const size_t swapChainImageViewsSize = swapChainImageViews.size();
    swapChainFramebuffers.size(swapChainImageViewsSize);

    for (size_t i = 0; i < swapChainImageViewsSize; i++)
    {
        VectorSafe<VkImageView, 2> attachments =
        {
            swapChainImageViews[i],
            depthImageView    //only need one depth buffer, since there's only one frame being actively rendered to at any given time
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;//number of image arrays -- each swap chain image in pAttachments is a single image

        const VkResult createFramebufferResult = vkCreateFramebuffer(device, &framebufferInfo, GetVulkanAllocationCallbacks(), &swapChainFramebuffers[i]);
        NTF_VK_ASSERT_SUCCESS(createFramebufferResult);
    }
}

void CreateSurface(VkSurfaceKHR*const surfacePtr, GLFWwindow*const window, const VkInstance& instance)
{
    assert(surfacePtr);
    auto& surface = *surfacePtr;

    assert(window);

    const VkResult createWindowSurfaceResult = glfwCreateWindowSurface(instance, window, GetVulkanAllocationCallbacks(), &surface);//cross-platform window creation
    NTF_VK_ASSERT_SUCCESS(createWindowSurfaceResult);
}

void CreateFrameSyncPrimitives(
    VectorSafeRef<VkSemaphore> imageAvailable, 
    VectorSafeRef<VkSemaphore> renderFinished, 
    const size_t transferFinishedSemaphoreSize,
    ArraySafeRef<VkSemaphore> transferFinishedSemaphorePool,
    ArraySafeRef<VkPipelineStageFlags> transferFinishedPipelineStageFlags,
    VectorSafeRef<VkFence> fence, 
    const size_t framesNum,
    const VkDevice& device)
{
    assert(framesNum);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;//fence starts signaled

    for (size_t frameIndex = 0; frameIndex < framesNum; ++frameIndex)
    {
        if (vkCreateSemaphore(device, &semaphoreInfo, GetVulkanAllocationCallbacks(), &imageAvailable[frameIndex]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, GetVulkanAllocationCallbacks(), &renderFinished[frameIndex]) != VK_SUCCESS)
        {
            assert(false);//failed to create semaphores
        }
        if (vkCreateFence(device, &fenceInfo, GetVulkanAllocationCallbacks(), &fence[frameIndex]) != VK_SUCCESS)
        {
            assert(false);//failed to create fence
        }
    }

    for (size_t transferFinishedSemaphoreIndex = 0; transferFinishedSemaphoreIndex < transferFinishedSemaphoreSize; ++transferFinishedSemaphoreIndex)
    {
        const VkResult transferFinishedSemaphoreCreateResult = 
            vkCreateSemaphore(device, &semaphoreInfo, GetVulkanAllocationCallbacks(), &transferFinishedSemaphorePool[transferFinishedSemaphoreIndex]);
        NTF_VK_ASSERT_SUCCESS(transferFinishedSemaphoreCreateResult);

        transferFinishedPipelineStageFlags[transferFinishedSemaphoreIndex] = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
}

///@todo: use push constants instead, since it's more efficient
void UpdateUniformBuffer(
    ArraySafeRef<uint8_t> uniformBufferCpuMemory,
    const glm::vec3 cameraTranslation,
    const VkDeviceMemory& uniformBufferGpuMemory, 
    const VkDeviceSize& offsetToGpuMemory,
    const size_t drawCallsNum,
    const VkDeviceSize uniformBufferSize, 
    const VkExtent2D& swapChainExtent, 
    const VkDevice& device)
{
    assert(drawCallsNum > 0);
    assert(uniformBufferSize > 0);

    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;

    const glm::mat4 worldRotation = glm::rotate(glm::mat4(), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    UniformBufferObject ubo = {};
    for (VkDeviceSize drawCallIndex = 0; drawCallIndex < drawCallsNum; ++drawCallIndex)
    {
        const glm::mat4 scale = glm::scale(glm::mat4(), glm::vec3(1.f));
        const glm::mat4 worldTranslation = glm::translate(glm::mat4(), glm::vec3(-1.f, -1.f, 0.f) + glm::vec3(0.f,2.f,0.f)*static_cast<float>(drawCallIndex));
        const glm::mat4 modelToWorld = worldTranslation*worldRotation*scale;
        const glm::mat4 worldToCamera = glm::inverse(glm::translate(glm::mat4(), cameraTranslation));
        const glm::mat4 cameraToView = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(-2.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        glm::mat4 viewToClip = glm::perspective(glm::radians(45.0f), swapChainExtent.width / static_cast<float>(swapChainExtent.height), 0.1f, 10.0f);
        viewToClip[1][1] *= -1;//OpenGL's clipspace y-axis points in opposite direction of Vulkan's y-axis; doing this requires counterclockwise vertex winding

        ubo.modelToClip = viewToClip*cameraToView*worldToCamera*modelToWorld;
        const size_t sizeofUbo = sizeof(ubo);
        uniformBufferCpuMemory.MemcpyFromIndex(&ubo, Cast_VkDeviceSize_size_t(drawCallIndex)*sizeofUbo, sizeofUbo);
    }

    VkMappedMemoryRange mappedMemoryRange;
    mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedMemoryRange.pNext = nullptr;
    mappedMemoryRange.memory = uniformBufferGpuMemory;
    mappedMemoryRange.offset = offsetToGpuMemory;
    mappedMemoryRange.size = uniformBufferSize;
    vkFlushMappedMemoryRanges(device, 1, &mappedMemoryRange);
}

void AcquireNextImage(
    uint32_t*const acquiredImageIndexPtr,
    const VkSwapchainKHR& swapChain, 
    const VkSemaphore& imageAvailableSemaphore, 
    const VkDevice& device)
{
    assert(acquiredImageIndexPtr);
    auto& acquiredImageIndex = *acquiredImageIndexPtr;

    const VkResult result = vkAcquireNextImageKHR(device, swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &acquiredImageIndex);//place the vkAcquireNextImageKHR() call as late as possible in the frame because this call can block according to the Vulkan spec.  Also note the spec allows the Acquire to return Image indexes in random order, so an application cannot assume round-robin order even with FIFO mode and a 2-deep swap chain
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        ///#TODO_CALLBACK
        //swap chain can no longer be used for rendering
        //hackToRecreateSwapChainIfNecessary.SwapChainRecreate();//haven't seen this get hit yet, even when minimizing and resizing the window
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR/*VK_SUBOPTIMAL_KHR indicates swap chain can still present image, but surface properties don't entirely match; for example, during resizing*/)
    {
        assert(false);//failed to acquire swap chain image
    }
    ///@todo: handle handle VK_ERROR_SURFACE_LOST_KHR return value
}

WIN_TIMER_DEF(s_frameTimer);

void DrawFrame(
    //VulkanRendererNTF*const hackToRecreateSwapChainIfNecessaryPtr,///#TODO_CALLBACK: clean this up with a proper callback
    const VkSwapchainKHR& swapChain,
    ConstVectorSafeRef<VkCommandBuffer> commandBuffers,
    const uint32_t acquiredImageIndex,
    const VkQueue& graphicsQueue,
    const VkQueue& presentQueue,
    const VkFence& fence,
    const VkSemaphore& imageAvailableSemaphore,
    const VkSemaphore& renderFinishedSemaphore,
    const VkDevice& device)
{
#if NTF_WIN_TIMER
    WIN_TIMER_STOP(s_frameTimer);
    const int maxLen = 256;
    char buf[maxLen];
    snprintf(&buf[0], maxLen, "s_frameTimer:%fms\n", WIN_TIMER_ELAPSED_MILLISECONDS(s_frameTimer));
    fwrite(&buf[0], sizeof(buf[0]), strlen(&buf[0]), s_winTimer);
    WIN_TIMER_START(s_frameTimer);
#endif//#if NTF_WIN_TIMER

    ///#TODO_CALLBACK
    //assert(hackToRecreateSwapChainIfNecessaryPtr);
    //auto& hackToRecreateSwapChainIfNecessary = *hackToRecreateSwapChainIfNecessaryPtr;

    WIN_TIMER_DEF_START(waitForFences);
    vkWaitForFences(device, 1, &fence,  VK_TRUE, UINT64_MAX/*wait until fence is signaled*/);
    WIN_TIMER_STOP(waitForFences);
    //const int maxLen = 256;
    //char buf[maxLen];
    //snprintf(&buf[0], maxLen, "waitForFences:%fms\n", WIN_TIMER_ELAPSED_MILLISECONDS(waitForFences));
    //fwrite(&buf[0], sizeof(buf[0]), strlen(&buf[0]), s_winTimer);
    vkResetFences(device, 1, &fence);//queue has completed on the GPU and is ready to be prepared on the CPU

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;//only value allowed

    //theoretically the implementation can already start executing our vertex shader and such while the image is not
    //available yet. Each entry in the waitStages array corresponds to the semaphore with the same index in pWaitSemaphores
    VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[acquiredImageIndex];

    //signal these semaphores once the command buffer(s) have finished execution
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    const VkResult queueSubmitResult = vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence);
    NTF_VK_ASSERT_SUCCESS(queueSubmitResult);

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pResults = nullptr; // allows you to specify an array of VkResult values to check for every individual swap chain if presentation was successful
    presentInfo.pImageIndices = &acquiredImageIndex;

    const VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR/*swap chain can no longer be used for rendering*/ ||
        result == VK_SUBOPTIMAL_KHR/*swap chain can still present image, but surface properties don't entirely match; for example, during resizing*/)
    {
        ///#TODO_CALLBACK
        //hackToRecreateSwapChainIfNecessary.SwapChainRecreate();//haven't seen this get hit yet, even when minimizing and resizing the window
    }
    NTF_VK_ASSERT_SUCCESS(result);
}

void GetRequiredExtensions(VectorSafeRef<const char*> requiredExtensions)
{
    requiredExtensions.size(0);
    unsigned int glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    for (unsigned int i = 0; i < glfwExtensionCount; i++)
    {
        requiredExtensions.Push(glfwExtensions[i]);
    }

    if (s_enableValidationLayers)
    {
        requiredExtensions.Push(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);//VulkanSDK\VERSION_NUMBER\Config\vk_layer_settings.txt sets many options about layer strictness (warning,performance,error) and action taken (callback, log, breakpoint, Visual Studio output, nothing), as well as dump behavior (level of detail, output to file vs stdout, I/O flush behavior)
    }
}

VkInstance CreateInstance(ConstVectorSafeRef<const char*> validationLayers)
{
    //BEG_#AllocationCallbacks
    s_allocationCallbacks.pfnAllocation = NTF_vkAllocationFunction;
    s_allocationCallbacks.pfnFree = NTF_vkFreeFunction;
    s_allocationCallbacks.pfnInternalAllocation = NTF_vkInternalAllocationNotification;
    s_allocationCallbacks.pfnInternalFree = NTF_vkInternalFreeNotification;
    s_allocationCallbacks.pfnReallocation = NTF_vkReallocationFunction;
    s_allocationCallbacks.pUserData = nullptr;
    //END_#AllocationCallbacks

#if NTF_WIN_TIMER
    fopen_s(&s_winTimer, "WinTimer.txt", "w+");
    assert(s_winTimer);
#endif//NTF_WIN_TIMER

    if (s_enableValidationLayers && !CheckValidationLayerSupport(validationLayers))
    {
        assert(false);//validation layers requested, but not available
    }

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanNTF Test";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;


    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VectorSafe<const char*, 32> extensions(0);
    GetRequiredExtensions(&extensions);
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (s_enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    VkInstance instance;
    const VkResult createInstanceResult = vkCreateInstance(&createInfo, GetVulkanAllocationCallbacks(), &instance);
    NTF_VK_ASSERT_SUCCESS(createInstanceResult);
    return instance;
}

VkDebugReportCallbackEXT SetupDebugCallback(const VkInstance& instance)
{
    if (!s_enableValidationLayers) return static_cast<VkDebugReportCallbackEXT>(0);

    VkDebugReportCallbackCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;//which events trigger the callback
    createInfo.pfnCallback = DebugCallback;

    VkDebugReportCallbackEXT callback;
    const VkResult createDebugReportCallbackEXTResult = CreateDebugReportCallbackEXT(instance, &createInfo, GetVulkanAllocationCallbacks(), &callback);//@todo NTF: this callback spits out the error messages to the command window, which vanishes upon application exit.  Should really throw up a dialog or something far more noticeable and less ignorable
    NTF_VK_ASSERT_SUCCESS(createDebugReportCallbackEXTResult);
    return callback;
}

void QuerySwapChainSupport(SwapChainSupportDetails*const swapChainSupportDetails, const VkSurfaceKHR& surface, const VkPhysicalDevice& device)
{
    assert(swapChainSupportDetails);

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainSupportDetails->capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    swapChainSupportDetails->formats.size(formatCount);
    if (swapChainSupportDetails->formats.size() != 0)
    {
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainSupportDetails->formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    swapChainSupportDetails->presentModes.size(presentModeCount);
    if (swapChainSupportDetails->presentModes.size() != 0)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device,
            surface,
            &presentModeCount,
            swapChainSupportDetails->presentModes.data());
    }
}

QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& device, const VkSurfaceKHR& surface)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    VectorSafe<VkQueueFamilyProperties, 8> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex)
    {
        const VkQueueFamilyProperties& queueFamilyProperties = queueFamilies[queueFamilyIndex];
        if (queueFamilyProperties.queueCount > 0)
        {
            ///@todo NTF: add logic to explicitly prefer a physical device that supports drawing and presentation in the same queue for improved performance rather than use presentFamily and graphicsFamily as separate queues
            if ((queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) && !(queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                indices.transferFamily = queueFamilyIndex;
            }
            
            if (queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = queueFamilyIndex;//queue supports rendering functionality
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, queueFamilyIndex, surface, &presentSupport);
            if (presentSupport)
            {
                indices.presentFamily = queueFamilyIndex;//queue supports present functionality
            }
        }

        if (indices.IsComplete())
        {
            break;
        }
    }

    if (indices.transferFamily < 0)//couldn't find a specialized transfer queue (here defined to be a queue that supports transfer but not graphics)
    {
        for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex)
        {
            const VkQueueFamilyProperties& queueFamilyProperties = queueFamilies[queueFamilyIndex];
            if (queueFamilyProperties.queueCount > 0)
            {
                if (queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT)
                {
                    indices.transferFamily = queueFamilyIndex;//accept any queue that supports transfer
                    break;
                }
            }
        }
    }

    return indices;
}

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(ConstVectorSafeRef<VkSurfaceFormatKHR> availableFormats)
{
    size_t availableFormatsNum = availableFormats.size();
    assert(availableFormatsNum > 0);

    if (availableFormatsNum == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        //all formats are supported, so return whatever we want
        return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    }

    //there are some format limitations; see if we can find the desired format
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];//couldn't find the desired format
}

VkPresentModeKHR ChooseSwapPresentMode(ConstVectorSafeRef<VkPresentModeKHR> availablePresentModes)
{
    assert(availablePresentModes.size());

    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            /*  instead of blocking the application when the queue is full, the images that are already queued are simply replaced with the newer 
                ones -- eg wait for the next vertical blanking interval to update the image. If we render another image, the image waiting to be 
                displayed is overwritten. */
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;/* display takes an image from the front of the queue on a vertical blank and the program inserts rendered images 
                                        at the back of the queue.  If the queue is full then the program has to wait */
    /* could also return:
    * VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from VK_PRESENT_MODE_FIFO_KHR if the application is late and the queue was 
                                        empty at the last vertical blank. Instead of waiting for the next vertical blank, the image is
                                        transferred right away when it finally arrives. This may result in visible tearing.  In other words, 
                                        wait for the next vertical blanking interval to update the image. If we've missed the interval, we do 
                                        not wait. We will append already rendered images to the pending presentation queue. We present as soon as 
                                        possible
    * VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away, which may result in tearing.
    */
}

///choose the resolution of the render target based on surface capabilities and window resolution
VkExtent2D ChooseSwapExtent(GLFWwindow*const window, const VkSurfaceCapabilitiesKHR& capabilities)
{
    assert(window);

    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);

        VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

void CreateSwapChain(
    GLFWwindow*const window,
    VkSwapchainKHR*const swapChainPtr,
    VectorSafeRef<VkImage> swapChainImages,
    VkFormat*const swapChainImageFormatPtr,
    VkExtent2D*const swapChainExtentPtr,
    const VkPhysicalDevice& physicalDevice,
    const uint32_t framesNum,
    const VkSurfaceKHR& surface,
    const VkDevice& device)
{
    assert(window);

    assert(swapChainPtr);
    VkSwapchainKHR& swapChain = *swapChainPtr;

    assert(swapChainImageFormatPtr);
    auto& swapChainImageFormat = *swapChainImageFormatPtr;

    assert(swapChainExtentPtr);
    auto& swapChainExtent = *swapChainExtentPtr;

    assert(framesNum > 0);

    SwapChainSupportDetails swapChainSupport;
    QuerySwapChainSupport(&swapChainSupport, surface, physicalDevice);

    const VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    const VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    const VkExtent2D extent = ChooseSwapExtent(window, swapChainSupport.capabilities);

    /*  #FramesInFlight:    Example: If we're GPU-bound, we might want to able to acquire at most 3 images without presenting, so we must exceed minImageCount by 
                            one less than this number.  This is because, for example, if the minImageCount member of VkSurfaceCapabilitiesKHR is 
                            2, and the application creates a swapchain with 2 presentable images, the application can acquire one image, and must 
                            present it before trying to acquire another image -- per Vulkan spec */
    const uint32_t swapChainImagesNumRequired = swapChainSupport.capabilities.minImageCount + framesNum;
    uint32_t swapChainImagesNum = swapChainImagesNumRequired;
    if (swapChainSupport.capabilities.maxImageCount > 0 && //0 means max image count is unlimited
        swapChainImagesNum > swapChainSupport.capabilities.maxImageCount)
    {
        swapChainImagesNum = swapChainSupport.capabilities.maxImageCount;
    }
    assert(swapChainImagesNum >= swapChainImagesNumRequired);

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = swapChainImagesNum;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;//1 for regular rendering; 2 for stereoscopic
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;//color attachment, since this is a render target
                                                                //if you pass the previous swap chain to createInfo.oldSwapChain, then that swap chain will be destroyed once it is finished with its work

    QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
    uint32_t queueFamilyIndices[] = { static_cast<uint32_t>(indices.graphicsFamily), static_cast<uint32_t>(indices.presentFamily) };
    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;//Images can be used across multiple queue families without explicit ownership transfers.
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;//image is owned by one queue family at a time and ownership must be explicitly transfered before using it in another queue family. This option offers the best performance.
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;//no transform.  If swapChainSupport.capabilities.supportedTransforms allow it, transforms like 90 degree rotations or horizontal-flips can be done
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;//ignore alpha, since this is a final render target and we won't be blending it with other render targets
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;//don't render pixels obscured by another window in front of our render target
    createInfo.oldSwapchain = VK_NULL_HANDLE;//assume we only need one swap chain (although it's possible for swap chains to get invalidated and need to be recreated by events like resizing the window)  TODO: understand more

    const VkResult createSwapchainKHRResult = vkCreateSwapchainKHR(device, &createInfo, GetVulkanAllocationCallbacks(), &swapChain);
    NTF_VK_ASSERT_SUCCESS(createSwapchainKHRResult);

    //extract swap chain image handles
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImagesNum, nullptr);
    swapChainImages.AssertSufficient(swapChainImagesNum);
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImagesNum, swapChainImages.data());
    swapChainImages.size(swapChainImagesNum);

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void FreeCommandBuffers(ArraySafeRef<VkCommandBuffer> commandBuffers, const uint32_t commandBuffersNum, const VkDevice& device, const VkCommandPool& commandPool)
{
    assert(commandBuffersNum > 0);
    vkFreeCommandBuffers(device, commandPool, commandBuffersNum, commandBuffers.data());
}

void CleanupSwapChain(
    VectorSafeRef<VkCommandBuffer> commandBuffersPrimary,
    const VkDevice& device,
    const VkImageView& depthImageView,
    const VkImage& depthImage,
    ConstVectorSafeRef<VkFramebuffer> swapChainFramebuffers,
    const VkCommandPool& commandPoolPrimary,
    ConstVectorSafeRef<ArraySafe<VkCommandPool, 2>> commandPoolsSecondary,///<@todo NTF: refactor out magic number 2 (meant to be NTF_OBJECTS_NUM) and either support VectorSafeRef<ArraySafeRef<T>> or repeatedly call FreeCommandBuffers on each VectorSafe outside of this function
    const VkPipeline& graphicsPipeline,
    const VkPipelineLayout& pipelineLayout,
    const VkRenderPass& renderPass,
    ConstVectorSafeRef<VkImageView> swapChainImageViews,
    const VkSwapchainKHR& swapChain)
{
    assert(commandBuffersPrimary.size() == swapChainFramebuffers.size());
    assert(swapChainFramebuffers.size() == swapChainImageViews.size());
    
    vkDestroyImageView(device, depthImageView, GetVulkanAllocationCallbacks());
    vkDestroyImage(device, depthImage, GetVulkanAllocationCallbacks());

    for (const VkFramebuffer vkFramebuffer : swapChainFramebuffers)
    {
        vkDestroyFramebuffer(device, vkFramebuffer, GetVulkanAllocationCallbacks());
    }

    //return command buffers to the pool from whence they came
    const size_t secondaryCommandBufferPerThread = 1;
    FreeCommandBuffers(&commandBuffersPrimary, Cast_size_t_uint32_t(commandBuffersPrimary.size()), device, commandPoolPrimary);

    vkDestroyPipeline(device, graphicsPipeline, GetVulkanAllocationCallbacks());
    vkDestroyPipelineLayout(device, pipelineLayout, GetVulkanAllocationCallbacks());
    vkDestroyRenderPass(device, renderPass, GetVulkanAllocationCallbacks());

    for (const VkImageView vkImageView : swapChainImageViews)
    {
        vkDestroyImageView(device, vkImageView, GetVulkanAllocationCallbacks());
    }

    vkDestroySwapchainKHR(device, swapChain, GetVulkanAllocationCallbacks());
}

void CreateImageViews(
    VectorSafeRef<VkImageView> swapChainImageViews,
    ConstVectorSafeRef<VkImage> swapChainImages,
    const VkFormat& swapChainImageFormat,
    const VkDevice& device)
{
    const size_t swapChainImagesSize = swapChainImages.size();
    swapChainImageViews.size(swapChainImagesSize);

    for (size_t i = 0; i < swapChainImagesSize; i++)
    {
        CreateImageView(&swapChainImageViews[i], device, swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanPagedStackAllocator::Initialize(const VkDevice& device,const VkPhysicalDevice& physicalDevice)
{
#if NTF_DEBUG
    assert(!m_initialized);
    m_initialized = true;
#endif//#if NTF_DEBUG

    m_device = device; 
    m_physicalDevice = physicalDevice;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    m_vulkanMemoryHeaps.size(memProperties.memoryTypeCount);
    for (size_t memoryTypeIndex = 0; memoryTypeIndex < memProperties.memoryTypeCount; ++memoryTypeIndex)
    {
        m_vulkanMemoryHeaps[memoryTypeIndex].Initialize(Cast_size_t_uint32_t(memoryTypeIndex), 128 * 1024 * 1024);
    }
}

void VulkanPagedStackAllocator::Destroy(const VkDevice& device)
{
#if NTF_DEBUG
    assert(m_initialized);
    m_initialized = false;
#endif//#if NTF_DEBUG

    for (auto& heap : m_vulkanMemoryHeaps)
    {
        heap.Destroy(m_device);
    }
}

///@todo: unit test
bool VulkanPagedStackAllocator::PushAlloc(
    VkDeviceSize* memoryOffsetPtr,
    VkDeviceMemory* memoryHandlePtr,
    const VkMemoryRequirements& memRequirements,
    const VkMemoryPropertyFlags& properties,
    const bool residentForever,
    const bool linearResource,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    assert(m_initialized);

    assert(memoryOffsetPtr);
    auto& memoryOffset = *memoryOffsetPtr;

    assert(memoryHandlePtr);
    auto& memoryHandle = *memoryHandlePtr;

    assert(memRequirements.size > 0);
    assert(memRequirements.alignment > 0);
    assert(memRequirements.alignment % 2 == 0);

    auto& heap = m_vulkanMemoryHeaps[FindMemoryType(memRequirements.memoryTypeBits, properties, physicalDevice)];
    const bool allocResult = heap.PushAlloc(
        &memoryOffset, 
        &memoryHandle, 
        memRequirements, 
        properties, 
        residentForever, 
        linearResource, 
        device, 
        physicalDevice);
    assert(allocResult);
    return allocResult;
}

void VulkanMemoryHeap::Initialize(const uint32_t memoryTypeIndex, const VkDeviceSize memoryHeapPageSizeBytes)
{
#if NTF_DEBUG
    assert(!m_initialized);
    m_initialized = true;
#endif//#if NTF_DEBUG

    m_memoryTypeIndex = memoryTypeIndex;
    m_pageSizeBytes = memoryHeapPageSizeBytes;
    m_pageAllocatedNonlinearFirst = m_pageAllocatedLinearFirst = nullptr;

    const size_t pagesNum = m_pagePool.size();
    VulkanMemoryHeapPage* pageCurrent = m_pageFreeFirst = &m_pagePool[0];
    for (size_t pageIndex = 1; pageIndex < pagesNum; ++pageIndex)
    {
        pageCurrent->m_next = &m_pagePool[pageIndex];
        pageCurrent = pageCurrent->m_next;
    }
    pageCurrent->m_next = nullptr;
}

///@todo: unit tests
static void VulkanMemoryHeapPageFreeAll(
    VulkanMemoryHeapPage**const pageAllocatedFirstPtrPtr, 
    VulkanMemoryHeapPage**const pageFreeFirstPtrPtr, 
    const VkDevice device)
{
    assert(pageAllocatedFirstPtrPtr);
    assert(pageFreeFirstPtrPtr);

    //free pages and add them to freelist
    VulkanMemoryHeapPage*& pageFreeFirst = *pageFreeFirstPtrPtr;
    VulkanMemoryHeapPage*& allocatedPage = *pageAllocatedFirstPtrPtr;
    while (allocatedPage)
    {
        allocatedPage->Free(device);

        VulkanMemoryHeapPage*const nextAllocatedPage = allocatedPage->m_next;
        allocatedPage->m_next = pageFreeFirst;
        pageFreeFirst = allocatedPage;
        allocatedPage = nextAllocatedPage;
    }
    *pageAllocatedFirstPtrPtr = nullptr;
}
void VulkanMemoryHeap::Destroy(const VkDevice device)
{
#if NTF_DEBUG
    assert(m_initialized);
    m_initialized = false;
#endif//#if NTF_DEBUG    

    if (m_pageResidentForeverNonlinear.Allocated())
    {
        m_pageResidentForeverNonlinear.Free(device);
    }
    if (m_pageResidentForeverLinear.Allocated())
    {
        m_pageResidentForeverLinear.Free(device);
    }

    VulkanMemoryHeapPageFreeAll(&m_pageAllocatedLinearFirst, &m_pageFreeFirst, device);
    VulkanMemoryHeapPageFreeAll(&m_pageAllocatedNonlinearFirst, &m_pageFreeFirst, device);
}

bool VulkanMemoryHeap::PushAlloc(
    VkDeviceSize* memoryOffsetPtr,
    VkDeviceMemory* memoryHandlePtr,
    const VkMemoryRequirements& memRequirements,
    const VkMemoryPropertyFlags& properties,
    const bool residentForever,
    const bool linearResource,///<true for buffers and VK_IMAGE_TILING_LINEAR images; false for VK_IMAGE_TILING_OPTIMAL images
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    assert(m_initialized);

    assert(memoryOffsetPtr);
    auto& memoryOffset = *memoryOffsetPtr;

    assert(memoryHandlePtr);
    auto& memoryHandle = *memoryHandlePtr;

    VulkanMemoryHeapPage* pageAllocatedCurrent;
    if (residentForever)
    {
        pageAllocatedCurrent = linearResource ? &m_pageResidentForeverLinear : &m_pageResidentForeverNonlinear;
        if (!pageAllocatedCurrent->Allocated())
        {
            pageAllocatedCurrent->Allocate(256*1024*1024, m_memoryTypeIndex, device);
        }
    }
    else
    {
        VulkanMemoryHeapPage*& pageAllocatedFirst = linearResource ? m_pageAllocatedLinearFirst : m_pageAllocatedNonlinearFirst;
        pageAllocatedCurrent = pageAllocatedFirst;
        VulkanMemoryHeapPage* pageAllocatedPrevious = nullptr;
        while (pageAllocatedCurrent)
        {
            if (pageAllocatedCurrent->SufficientMemory(memRequirements))
            {
                break;
            }
            else
            {
                pageAllocatedPrevious = pageAllocatedCurrent;
                pageAllocatedCurrent = pageAllocatedCurrent->m_next;
            }
        }
        if (!pageAllocatedCurrent)
        {
            VulkanMemoryHeapPage& pageNew = *m_pageFreeFirst;
            m_pageFreeFirst = m_pageFreeFirst->m_next;

            pageNew.Allocate(m_pageSizeBytes, m_memoryTypeIndex, device);
            (pageAllocatedPrevious ? pageAllocatedPrevious->m_next : pageAllocatedFirst) = pageAllocatedCurrent = &pageNew;
        }
    }
    assert(pageAllocatedCurrent);
    assert(pageAllocatedCurrent->SufficientMemory(memRequirements));

    memoryHandle = pageAllocatedCurrent->GetMemoryHandle();
    const bool allocResult = pageAllocatedCurrent->PushAlloc(&memoryOffset, memRequirements);
    assert(allocResult);
    return allocResult;
}

bool VulkanMemoryHeapPage::Allocate(const VkDeviceSize memoryMaxBytes, const uint32_t memoryTypeIndex, const VkDevice& device)
{
    m_stack.Allocate(memoryMaxBytes);
    m_next = nullptr;

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryMaxBytes;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    allocInfo.pNext = nullptr;

    const VkResult allocateMemoryResult = vkAllocateMemory(device, &allocInfo, GetVulkanAllocationCallbacks(), &m_memoryHandle);
    printf("vkAllocateMemory(memoryTypeIndex=%u, memoryMaxBytes=%u)=%x; m_memoryHandle=%p\n", memoryTypeIndex, Cast_VkDeviceSize_uint32_t(memoryMaxBytes), allocateMemoryResult, (void*)m_memoryHandle);
    NTF_VK_ASSERT_SUCCESS(allocateMemoryResult);
    return NTF_VK_SUCCESS(allocateMemoryResult);
}

bool VulkanMemoryHeapPage::PushAlloc(VkDeviceSize* memoryOffsetPtr, const VkMemoryRequirements& memRequirements)
{
    assert(memoryOffsetPtr);
    auto& memoryOffset = *memoryOffsetPtr;

    const bool allocateResult = m_stack.PushAlloc(&memoryOffset, memRequirements.alignment, memRequirements.size);
    assert(allocateResult);
    return allocateResult;
}

bool VulkanMemoryHeapPage::PushAlloc(
    VkDeviceSize*const firstByteFreePtr,
    VkDeviceSize*const firstByteReturnedPtr,
    const VkMemoryRequirements& memRequirements) const
{
    return m_stack.PushAllocInternal(firstByteFreePtr, firstByteReturnedPtr, memRequirements.alignment, memRequirements.size);
}

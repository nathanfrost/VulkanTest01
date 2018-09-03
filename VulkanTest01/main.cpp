#include"ntf_vulkan.h"

VectorSafe<const char*, NTF_VALIDATION_LAYERS_SIZE> s_validationLayers;

extern StackCpu* g_stbAllocator;

#include"ntf_vulkan_utility.h"

//don't complain about scanf being unsafe
#pragma warning(disable : 4996)
///@todo: figure out which libraries I'm linking that trigger LNK4098 (seems like some libraries are linking /MD and /MDd and others are linking /MT and /MTd for C-runtime) -- for now, pass /IGNORE:4098 to the linker
class VulkanRendererNTF 
{
public:
#define NTF_FRAMES_IN_FLIGHT_NUM 2//#FramesInFlight
#define NTF_OBJECTS_NUM 2//number of models to draw
#define NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE (128 * 1024 * 1024)

    void run() 
	{
        initWindow(&m_window);
        initVulkan();

        CommandBufferSecondaryThreadsCreate(&m_commandBufferSecondaryThreads, &m_commandBufferThreadDoneEvents, &m_commandBufferThreadArguments, NTF_OBJECTS_NUM);

        mainLoop(m_window);
        cleanup();

        int i;
#if NTF_DEBUG
        printf("s_vulkanApiCpuBytesAllocatedMax=%zu\n", GetVulkanApiCpuBytesAllocatedMax());
#endif//#if NTF_DEBUG
        printf("Enter a character and press ENTER to exit\n");
        scanf("%i", &i);
    }

    ///@todo: unit test
    void recreateSwapChain()
    {
        vkDeviceWaitIdle(m_device);

        CleanupSwapChain(
            &m_commandBuffersPrimary,
            &m_commandBuffersSecondary,
            m_device,
            m_depthImageView,
            m_depthImage,
            m_swapChainFramebuffers,
            m_commandPoolPrimary,
            m_commandPoolsSecondary,
            m_graphicsPipeline,
            m_pipelineLayout,
            m_renderPass,
            m_swapChainImageViews,
            m_swapChain);

        m_deviceLocalMemory.Destroy(m_device);
        m_deviceLocalMemory.Initialize(m_device, m_physicalDevice);

        CreateSwapChain(
            m_window, 
            &m_swapChain, 
            &m_swapChainImages, 
            &m_swapChainImageFormat, 
            &m_swapChainExtent, 
            m_physicalDevice, 
            NTF_FRAMES_IN_FLIGHT_NUM, 
            m_surface, 
            m_device);
        CreateImageViews(&m_swapChainImageViews, m_swapChainImages, m_swapChainImageFormat, m_device);
        CreateRenderPass(&m_renderPass, m_swapChainImageFormat, m_device, m_physicalDevice);
        CreateGraphicsPipeline(
            &m_pipelineLayout, 
            &m_graphicsPipeline, 
            g_stbAllocator, 
            m_renderPass, 
            m_descriptorSetLayout, 
            m_swapChainExtent, 
            m_device);
        
        //#CommandPoolDuplication
        
        AllocateCommandBuffers(
            ArraySafeRef<VkCommandBuffer>(&m_commandBufferTransfer, 1),
            m_commandPoolTransfer,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1,
            m_device);
        AllocateCommandBuffers(
            ArraySafeRef<VkCommandBuffer>(&m_commandBufferTransitionImage, 1),
            m_commandPoolPrimary,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1,
            m_device);

        CreateDepthResources(
            &m_depthImage,
            &m_depthImageView,
            &m_deviceLocalMemory,
            m_swapChainExtent,
            m_commandBufferTransitionImage,
            m_graphicsQueue,
            m_device,
            m_physicalDevice);
        CreateFramebuffers(&m_swapChainFramebuffers, m_swapChainImageViews, m_renderPass, m_swapChainExtent, m_depthImageView, m_device);

        //#CommandPoolDuplication
        const uint32_t swapChainFramebuffersSize = Cast_size_t_uint32_t(m_swapChainFramebuffers.size());
        m_commandBuffersPrimary.size(swapChainFramebuffersSize);//bake one command buffer for every image in the swapchain so Vulkan can blast through them
        AllocateCommandBuffers(
            &m_commandBuffersPrimary,
            m_commandPoolPrimary,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            swapChainFramebuffersSize,
            m_device);

        const size_t commandBuffersSecondaryPerFrame = NTF_OBJECTS_NUM;
        const size_t commandBufferSecondaryPerCreateCall = 1;
        m_commandBuffersSecondary.size(swapChainFramebuffersSize);
        m_commandPoolsSecondary.size(swapChainFramebuffersSize);
        for (size_t frameBufferIndex = 0; frameBufferIndex < swapChainFramebuffersSize; ++frameBufferIndex)
        {
            for (size_t commandBufferSecondaryIndex = 0; commandBufferSecondaryIndex < commandBuffersSecondaryPerFrame; ++commandBufferSecondaryIndex)
            {
                AllocateCommandBuffers(
                    ArraySafeRef<VkCommandBuffer>(&m_commandBuffersSecondary[frameBufferIndex][commandBufferSecondaryIndex], commandBufferSecondaryPerCreateCall),
                    m_commandPoolsSecondary[frameBufferIndex][commandBufferSecondaryIndex],
                    VK_COMMAND_BUFFER_LEVEL_SECONDARY,
                    commandBufferSecondaryPerCreateCall,
                    m_device);
            }
        }
        //#CommandPoolDuplication
    }

private:
    VkDeviceSize UniformBufferSizeCalculate() const
    {
        return NTF_OBJECTS_NUM*m_uniformBufferCpuAlignment;
    }
    void initWindow(GLFWwindow**const windowPtrPtr)
    {
        assert(windowPtrPtr);
        GLFWwindow*& windowPtr = *windowPtrPtr;

        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API/**<hard constrain API...*/, GLFW_NO_API/**<...to Vulkan, which does not use an API*/);
        //takes values set by glfwWindowHint() -- width and height may vary as they're soft constraints
        windowPtr = glfwCreateWindow(
            s_kWidth,
            s_kHeight,
            "Vulkan window",
            nullptr/*windowed mode, not full-screen monitor*/,
            nullptr/*no sharing objects with another window; that's OpenGL, not Vulkan anyway*/);

        glfwSetWindowUserPointer(windowPtr, this);
        glfwSetWindowSizeCallback(windowPtr, VulkanRendererNTF::onWindowResized);
    }

    /*  Viewport and scissor rectangle size is specified during graphics pipeline creation, so the pipeline also needs to be rebuilt when the window 
        is resized. It is possible to avoid this by using dynamic state for the viewports and scissor rectangles */
    static void onWindowResized(GLFWwindow* window, const int width, const int height)
    {
        if (width == 0 || height == 0) 
		{
			return;//handle the case where the window was minimized
		}

        VulkanRendererNTF* app = reinterpret_cast<VulkanRendererNTF*>(glfwGetWindowUserPointer(window));
        app->recreateSwapChain();
    }

    void cleanup()
    {
        STBAllocatorDestroy();

        CleanupSwapChain(
            &m_commandBuffersPrimary,
            &m_commandBuffersSecondary,
            m_device,
            m_depthImageView,
            m_depthImage,
            m_swapChainFramebuffers,
            m_commandPoolPrimary,
            m_commandPoolsSecondary,
            m_graphicsPipeline,
            m_pipelineLayout,
            m_renderPass,
            m_swapChainImageViews,
            m_swapChain);
        
        vkDestroySampler(m_device, m_textureSampler, GetVulkanAllocationCallbacks());
        vkDestroyImageView(m_device, m_textureImageView, GetVulkanAllocationCallbacks());

        vkDestroyImage(m_device, m_textureImage, GetVulkanAllocationCallbacks());

        vkDestroyDescriptorPool(m_device, m_descriptorPool, GetVulkanAllocationCallbacks());
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, GetVulkanAllocationCallbacks());
        
        DestroyUniformBuffer(m_uniformBufferCpuMemory, m_uniformBufferGpuMemory, m_uniformBuffer, m_device);
        vkDestroyBuffer(m_device, m_indexBuffer, GetVulkanAllocationCallbacks());
        vkDestroyBuffer(m_device, m_vertexBuffer, GetVulkanAllocationCallbacks());

        for (size_t frameIndex = 0; frameIndex < NTF_FRAMES_IN_FLIGHT_NUM; ++frameIndex)
        {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphore[frameIndex], GetVulkanAllocationCallbacks());
            vkDestroySemaphore(m_device, m_imageAvailableSemaphore[frameIndex], GetVulkanAllocationCallbacks());
            vkDestroyFence(m_device, m_fence[frameIndex], GetVulkanAllocationCallbacks());
        }
        vkDestroySemaphore(m_device, m_transferFinishedSemaphore, GetVulkanAllocationCallbacks());

        vkDestroyCommandPool(m_device, m_commandPoolPrimary, GetVulkanAllocationCallbacks());
        if (m_commandPoolTransfer != m_commandPoolPrimary)
        {
            vkDestroyCommandPool(m_device, m_commandPoolTransfer, GetVulkanAllocationCallbacks());
        }
        for (auto& commandPoolSecondaryArray : m_commandPoolsSecondary)
        {
            for (auto& commandPoolSecondary : commandPoolSecondaryArray)
            {
                vkDestroyCommandPool(m_device, commandPoolSecondary, GetVulkanAllocationCallbacks());
            }
        }

        vkUnmapMemory(m_device, m_stagingBufferGpuMemory);
        //BEG_#StagingBuffer
        vkDestroyBuffer(m_device, m_stagingBufferGpu, GetVulkanAllocationCallbacks());
        for (auto& buffer : m_stagingBuffersGpu)
        {
            vkDestroyBuffer(m_device, buffer, GetVulkanAllocationCallbacks());
        }
        //END_#StagingBuffer
        m_stagingBufferMemoryMapCpuToGpu.Destroy();

        m_deviceLocalMemory.Destroy(m_device);
        vkDestroyDevice(m_device, GetVulkanAllocationCallbacks());
        DestroyDebugReportCallbackEXT(m_instance, m_callback, GetVulkanAllocationCallbacks());
        vkDestroySurfaceKHR(m_instance, m_surface, GetVulkanAllocationCallbacks());
        vkDestroyInstance(m_instance, GetVulkanAllocationCallbacks());

        glfwDestroyWindow(m_window);

        glfwTerminate();
    }

    void initVulkan()
    {
        s_validationLayers.size(0);
        s_validationLayers.Push("VK_LAYER_LUNARG_standard_validation");
#if NTF_API_DUMP_VALIDATION_LAYER_ON
        s_validationLayers.Push("VK_LAYER_LUNARG_api_dump");///<this produces "file not found" after outputting to (I believe) stdout for a short while; seems like it overruns Windows 7's file descriptor or something.  Weirdly, running from Visual Studio 2015 does not seem to have this problem, but then I'm limited to 9999 lines of the command prompt VS2015 uses for output.  Not ideal
#endif//NTF_API_DUMP_VALIDATION_LAYER_ON

        m_deviceExtensions = VectorSafe<const char*, NTF_DEVICE_EXTENSIONS_NUM>({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });

        STBAllocatorCreate();

        m_instance = CreateInstance(s_validationLayers);
        m_callback = SetupDebugCallback(m_instance);
        CreateSurface(&m_surface, m_window, m_instance);//window surface needs to be created right before physical device creation, because it can actually influence the physical device selection: TODO: learn more about this influence
        PickPhysicalDevice(&m_physicalDevice, m_surface, m_deviceExtensions, m_instance);
        m_queueFamilyIndices = FindQueueFamilies(m_physicalDevice, m_surface);
        CreateLogicalDevice(
            &m_device, 
            &m_graphicsQueue, 
            &m_presentQueue, 
            &m_transferQueue, 
            m_deviceExtensions, 
            s_validationLayers, 
            m_surface, 
            m_physicalDevice);
        CreateSwapChain(
            m_window, 
            &m_swapChain, 
            &m_swapChainImages, 
            &m_swapChainImageFormat, 
            &m_swapChainExtent, 
            m_physicalDevice, 
            NTF_FRAMES_IN_FLIGHT_NUM, 
            m_surface, 
            m_device);
        CreateImageViews(&m_swapChainImageViews, m_swapChainImages, m_swapChainImageFormat, m_device);
        CreateRenderPass(&m_renderPass, m_swapChainImageFormat, m_device, m_physicalDevice);
        
        const VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        CreateDescriptorSetLayout(&m_descriptorSetLayout, descriptorType, m_device);
        CreateGraphicsPipeline(
            &m_pipelineLayout, 
            &m_graphicsPipeline, 
            g_stbAllocator, 
            m_renderPass, 
            m_descriptorSetLayout, 
            m_swapChainExtent, 
            m_device);

        const QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_physicalDevice, m_surface);
        CreateCommandPool(&m_commandPoolPrimary, queueFamilyIndices.graphicsFamily, m_device, m_physicalDevice);
        if (queueFamilyIndices.graphicsFamily != queueFamilyIndices.transferFamily)
        {
            CreateCommandPool(&m_commandPoolTransfer, queueFamilyIndices.transferFamily, m_device, m_physicalDevice);
        }
        else
        {
            m_commandPoolTransfer = m_commandPoolPrimary;
        }

        m_deviceLocalMemory.Initialize(m_device, m_physicalDevice);

        //#CommandPoolDuplication
        AllocateCommandBuffers(
            ArraySafeRef<VkCommandBuffer>(&m_commandBufferTransfer, 1),
            m_commandPoolTransfer,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1,
            m_device);
        AllocateCommandBuffers(
            ArraySafeRef<VkCommandBuffer>(&m_commandBufferTransitionImage, 1),
            m_commandPoolPrimary,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1,
            m_device);

        CreateFrameSyncPrimitives(
            &m_imageAvailableSemaphore,
            &m_renderFinishedSemaphore,
            &m_transferFinishedSemaphore,
            &m_fence,
            NTF_FRAMES_IN_FLIGHT_NUM,
            m_device);

        CreateDepthResources(
            &m_depthImage, 
            &m_depthImageView, 
            &m_deviceLocalMemory,
            m_swapChainExtent, 
            m_commandBufferTransitionImage,
            m_graphicsQueue, 
            m_device,
            m_physicalDevice);
        CreateFramebuffers(&m_swapChainFramebuffers, m_swapChainImageViews, m_renderPass, m_swapChainExtent, m_depthImageView, m_device);
        
        const uint32_t swapChainFramebuffersSize = Cast_size_t_uint32_t(m_swapChainFramebuffers.size());
        m_commandPoolsSecondary.size(swapChainFramebuffersSize);
        for (auto& commandPoolSecondaryArray : m_commandPoolsSecondary)
        {
            for (auto& commandPoolSecondary : commandPoolSecondaryArray)
            {
                CreateCommandPool(&commandPoolSecondary, queueFamilyIndices.graphicsFamily, m_device, m_physicalDevice);
            }
        }

        StackNTF<VkDeviceSize> stagingBufferGpuStack;
        VkDeviceSize stagingBufferGpuOffsetToAllocatedBlock;
        stagingBufferGpuStack.Allocate(NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE);
        CreateBuffer(
            &m_stagingBufferGpu,
            &m_stagingBufferGpuMemory,
            &m_deviceLocalMemory,
            &m_offsetToFirstByteOfStagingBuffer,
            NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true,
            m_device,
            m_physicalDevice);
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device, m_stagingBufferGpu, &memRequirements);
        m_stagingBufferGpuAlignmentStandard = memRequirements.alignment;

        void* stagingBufferMemoryMapCpuToGpu;
        const VkResult vkMapMemoryResult = vkMapMemory(
            m_device, 
            m_stagingBufferGpuMemory, 
            m_offsetToFirstByteOfStagingBuffer,
            NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE, 
            0, 
            &stagingBufferMemoryMapCpuToGpu);
        NTF_VK_ASSERT_SUCCESS(vkMapMemoryResult);
        m_stagingBufferMemoryMapCpuToGpu.Initialize(reinterpret_cast<uint8_t*>(stagingBufferMemoryMapCpuToGpu), NTF_STAGING_BUFFER_CPU_TO_GPU_SIZE);
        size_t stagingBufferGpuIndex = 0;
        
        int textureWidth, textureHeight;
        size_t imageSizeBytes;
        VkDeviceSize alignment;
        const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        const bool copyPixelsIfStagingBufferHasSpaceResult = CreateImageAndCopyPixelsIfStagingBufferHasSpace(
            &m_textureImage,
            &m_deviceLocalMemory,
            &alignment,
            &textureWidth, 
            &textureHeight, 
            &m_stagingBufferMemoryMapCpuToGpu,
            &imageSizeBytes, 
            g_stbAllocator,
            "textures/chalet.jpg",
            imageFormat,
            VK_IMAGE_TILING_OPTIMAL/*could also pass VK_IMAGE_TILING_LINEAR so texels are laid out in row-major order for debugging (less performant)*/,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT/*accessible by shader*/,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            false,
            m_device,
            m_physicalDevice);
        assert(copyPixelsIfStagingBufferHasSpaceResult);

        const bool pushAllocSuccess = stagingBufferGpuStack.PushAlloc(&stagingBufferGpuOffsetToAllocatedBlock, alignment, imageSizeBytes);
        assert(pushAllocSuccess);
        CreateBuffer(
            &m_stagingBuffersGpu[stagingBufferGpuIndex],
            m_stagingBufferGpuMemory,
            m_offsetToFirstByteOfStagingBuffer + stagingBufferGpuOffsetToAllocatedBlock,
            imageSizeBytes,
            0,
            m_device,
            m_physicalDevice);

        const bool unifiedGraphicsAndTransferQueue = m_graphicsQueue == m_transferQueue;
        assert(unifiedGraphicsAndTransferQueue == (m_queueFamilyIndices.transferFamily == m_queueFamilyIndices.graphicsFamily));
        BeginCommands(m_commandBufferTransfer, m_device);
        if (!unifiedGraphicsAndTransferQueue)
        {
            BeginCommands(m_commandBufferTransitionImage, m_device);
        }
        TransferImageFromCpuToGpu(
            m_textureImage, 
            textureWidth, 
            textureHeight, 
            imageFormat,
            m_stagingBuffersGpu[stagingBufferGpuIndex], 
            m_commandBufferTransfer, 
            m_transferQueue, 
            m_queueFamilyIndices.transferFamily, 
            m_transferFinishedSemaphore, 
            m_commandBufferTransitionImage, 
            m_graphicsQueue, 
            m_queueFamilyIndices.graphicsFamily, 
            m_device);

        CreateTextureImageView(&m_textureImageView, m_textureImage, m_device);
        CreateTextureSampler(&m_textureSampler, m_device);

        ++stagingBufferGpuIndex;

        //BEG_#StreamingMemory
        LoadModel(&m_vertices, &m_indices);
        m_indicesSize = Cast_size_t_uint32_t(m_indices.size());//store since we need secondary buffers to point to this
        //END_#StreamingMemory
        {
            const size_t bufferSize = sizeof(m_vertices[0]) * m_vertices.size();

            stagingBufferGpuStack.PushAlloc(&stagingBufferGpuOffsetToAllocatedBlock, m_stagingBufferGpuAlignmentStandard, bufferSize);
            CreateBuffer(
                &m_stagingBuffersGpu[stagingBufferGpuIndex],
                m_stagingBufferGpuMemory,
                m_offsetToFirstByteOfStagingBuffer + stagingBufferGpuOffsetToAllocatedBlock,
                bufferSize,
                0,
                m_device,
                m_physicalDevice);

#if NTF_DEBUG
            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(m_device, m_stagingBuffersGpu[stagingBufferGpuIndex], &memRequirements);
            assert(memRequirements.alignment == m_stagingBufferGpuAlignmentStandard);
#endif//#if NTF_DEBUG

            ArraySafeRef<uint8_t> vertexBufferStagingBufferCpuToGpu;
            m_stagingBufferMemoryMapCpuToGpu.PushAlloc(
                &vertexBufferStagingBufferCpuToGpu, 
                Cast_VkDeviceSize_size_t(m_stagingBufferGpuAlignmentStandard), 
                bufferSize);
            CreateAndCopyToGpuBuffer(
                &m_deviceLocalMemory,
                &m_vertexBuffer,
                &m_vertexBufferMemory,
                vertexBufferStagingBufferCpuToGpu,
                m_vertices.data(),
                m_stagingBuffersGpu[stagingBufferGpuIndex],
                bufferSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,/*specifies that the buffer is suitable for passing as an element of the pBuffers array to vkCmdBindVertexBuffers*/
                false,
                m_commandBufferTransfer,
                m_device,
                m_physicalDevice);
            ++stagingBufferGpuIndex;
        }
        {
            const size_t bufferSize = sizeof(m_indices[0]) * m_indices.size();
            stagingBufferGpuStack.PushAlloc(&stagingBufferGpuOffsetToAllocatedBlock, m_stagingBufferGpuAlignmentStandard, bufferSize);
            CreateBuffer(
                &m_stagingBuffersGpu[stagingBufferGpuIndex],
                m_stagingBufferGpuMemory,
                m_offsetToFirstByteOfStagingBuffer + stagingBufferGpuOffsetToAllocatedBlock,
                bufferSize,
                0,
                m_device,
                m_physicalDevice);

#if NTF_DEBUG
            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(m_device, m_stagingBuffersGpu[stagingBufferGpuIndex], &memRequirements);
            assert(memRequirements.alignment == m_stagingBufferGpuAlignmentStandard);
#endif//#if NTF_DEBUG

            ArraySafeRef<uint8_t> indexBufferStagingBufferCpuToGpu;
            m_stagingBufferMemoryMapCpuToGpu.PushAlloc(
                &indexBufferStagingBufferCpuToGpu, 
                Cast_VkDeviceSize_size_t(m_stagingBufferGpuAlignmentStandard), 
                bufferSize);
            CreateAndCopyToGpuBuffer(
                &m_deviceLocalMemory,
                &m_indexBuffer,
                &m_indexBufferMemory,
                indexBufferStagingBufferCpuToGpu,
                m_indices.data(),
                m_stagingBuffersGpu[stagingBufferGpuIndex],
                bufferSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                false,
                m_commandBufferTransfer,
                m_device,
                m_physicalDevice);
        }
        VectorSafe<VkSemaphore, 1> transferFinishedSemaphore;
        if (!unifiedGraphicsAndTransferQueue)
        {
            transferFinishedSemaphore.Push(m_transferFinishedSemaphore);
        }
        vkEndCommandBuffer(m_commandBufferTransfer);
        SubmitCommandBuffer(
            transferFinishedSemaphore,
            ConstVectorSafeRef<VkSemaphore>(),
            ArraySafeRef<VkPipelineStageFlags>(),
            m_commandBufferTransfer,
            m_transferQueue);
        if (!unifiedGraphicsAndTransferQueue)
        {
            vkEndCommandBuffer(m_commandBufferTransitionImage);
            ArraySafe<VkPipelineStageFlags, 1> waitStages({ VK_PIPELINE_STAGE_TRANSFER_BIT });
            SubmitCommandBuffer(
                ConstVectorSafeRef<VkSemaphore>(),
                transferFinishedSemaphore,
                &waitStages,
                m_commandBufferTransitionImage,
                m_graphicsQueue);
        }

        m_uniformBufferCpuAlignment = UniformBufferCpuAlignmentCalculate(sm_uniformBufferElementSize, m_physicalDevice);
        CreateUniformBuffer(
            &m_uniformBufferCpuMemory,
            &m_uniformBufferGpuMemory,
            &m_uniformBuffer, 
            &m_deviceLocalMemory,
            &m_uniformBufferOffsetToGpuMemory,
            UniformBufferSizeCalculate(),
            false,
            m_device, 
            m_physicalDevice);

        CreateDescriptorPool(&m_descriptorPool, descriptorType, m_device);
        CreateDescriptorSet(
            &m_descriptorSet, 
            descriptorType, 
            m_descriptorSetLayout, 
            m_descriptorPool, 
            m_uniformBuffer, 
            sm_uniformBufferElementSize, 
            m_textureImageView, 
            m_textureSampler, 
            m_device);

        //#CommandPoolDuplication
        m_commandBuffersPrimary.size(swapChainFramebuffersSize);//bake one command buffer for every image in the swapchain so Vulkan can blast through them
        AllocateCommandBuffers(
            &m_commandBuffersPrimary,
            m_commandPoolPrimary,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            swapChainFramebuffersSize,
            m_device);

        const size_t commandBuffersSecondaryPerFrame = NTF_OBJECTS_NUM;
        const size_t commandBufferSecondaryPerCreateCall = 1;
        m_commandBuffersSecondary.size(swapChainFramebuffersSize);
        m_commandPoolsSecondary.size(swapChainFramebuffersSize);
        for (size_t frameBufferIndex = 0; frameBufferIndex < swapChainFramebuffersSize; ++frameBufferIndex)
        {
            for (size_t commandBufferSecondaryIndex = 0; commandBufferSecondaryIndex < commandBuffersSecondaryPerFrame; ++commandBufferSecondaryIndex)
            {
                AllocateCommandBuffers(
                    ArraySafeRef<VkCommandBuffer>(&m_commandBuffersSecondary[frameBufferIndex][commandBufferSecondaryIndex], commandBufferSecondaryPerCreateCall),
                    m_commandPoolsSecondary[frameBufferIndex][commandBufferSecondaryIndex],
                    VK_COMMAND_BUFFER_LEVEL_SECONDARY,
                    commandBufferSecondaryPerCreateCall,
                    m_device);
            }
        }
        //#CommandPoolDuplication
    }

    void mainLoop(GLFWwindow* window) 
    {
        assert(window);
        size_t frameIndex = 0;
        while (!glfwWindowShouldClose(window)) 
        {
            glfwPollEvents();

            UpdateUniformBuffer(
                m_uniformBufferCpuMemory, 
                m_uniformBufferGpuMemory, 
                m_uniformBufferOffsetToGpuMemory,
                NTF_OBJECTS_NUM,
                UniformBufferSizeCalculate(), 
                m_swapChainExtent, 
                m_device);

            const VkSemaphore imageAvailableSemaphore = m_imageAvailableSemaphore[frameIndex];
            uint32_t acquiredImageIndex;
            AcquireNextImage(&acquiredImageIndex, m_swapChain, imageAvailableSemaphore, m_device);

            FillSecondaryCommandBuffers(
                &m_commandBuffersSecondary[acquiredImageIndex],
                &m_commandBufferSecondaryThreads,
                &m_commandBufferThreadDoneEvents,
                &m_commandBufferThreadArguments,
                &m_descriptorSet,
                &m_swapChainFramebuffers[acquiredImageIndex],
                &m_renderPass,
                &m_swapChainExtent,
                &m_pipelineLayout,
                &m_graphicsPipeline,
                &m_vertexBuffer,
                &m_indexBuffer,
                &m_indicesSize,
                &m_objectIndices,
                NTF_OBJECTS_NUM);

            FillPrimaryCommandBuffer(
                m_commandBuffersPrimary[acquiredImageIndex],
                &m_commandBuffersSecondary[acquiredImageIndex],
                NTF_OBJECTS_NUM,
                m_swapChainFramebuffers[acquiredImageIndex],
                m_renderPass,
                m_swapChainExtent);

            DrawFrame(
                /*this,///#TODO_CALLBACK*/ 
                m_swapChain, 
                m_commandBuffersPrimary, 
                acquiredImageIndex,
                m_graphicsQueue, 
                m_presentQueue, 
                m_fence[frameIndex],
                imageAvailableSemaphore,
                m_renderFinishedSemaphore[frameIndex],
                m_device);
            frameIndex = (frameIndex + 1) % NTF_FRAMES_IN_FLIGHT_NUM;
        }

        //wait for the logical device to finish operations before exiting mainLoop and destroying the window
        vkDeviceWaitIdle(m_device);
    }

    const size_t sm_uniformBufferElementSize = sizeof(UniformBufferObject);

    GLFWwindow* m_window;
    VkInstance m_instance;
    VkDebugReportCallbackEXT m_callback;
    VkSurfaceKHR m_surface;
    QueueFamilyIndices m_queueFamilyIndices;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;//doesn't need to be deleted, since physical devices can't be created or destroyed by software
    VkDevice m_device;//interface to the physical device; must be destroyed before the physical device
    VkSwapchainKHR m_swapChain;//must be destroyed before the logical device
    VkQueue m_graphicsQueue, m_presentQueue, m_transferQueue;//queues are implicitly cleaned up with the logical device; no need to delete
    VectorSafe<const char*, NTF_DEVICE_EXTENSIONS_NUM> m_deviceExtensions;
    enum { kSwapChainImagesNumMax=8 };
    VectorSafe<VkImage, kSwapChainImagesNumMax> m_swapChainImages;//handles to images, which are created by the swapchain and will be destroyed by the swapchain.  Images are "multidimensional - up to 3 - arrays of data which can be used for various purposes (e.g. attachments, textures), by binding them to a graphics or compute pipeline via descriptor sets, or by directly specifying them as parameters to certain commands" -- https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VkImage.html
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;
    VectorSafe<VkImageView, kSwapChainImagesNumMax> m_swapChainImageViews;//defines type of image (eg color buffer with mipmaps, depth buffer, and so on)
    VectorSafe<VkFramebuffer, kSwapChainImagesNumMax> m_swapChainFramebuffers;
    VkRenderPass m_renderPass;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;
    VkCommandPool m_commandPoolPrimary, m_commandPoolTransfer;
    VectorSafe<ArraySafe<VkCommandPool, NTF_OBJECTS_NUM>, kSwapChainImagesNumMax> m_commandPoolsSecondary;
    VkImage m_depthImage;
    VkImageView m_depthImageView;
    VkImage m_textureImage;
    VkImageView m_textureImageView;
    VkSampler m_textureSampler;

    //BEG_#StreamingMemory
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    uint32_t m_indicesSize;
    //END_#StreamingMemory

    //BEG_#StreamingMemory
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexBufferMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexBufferMemory;
    VkDeviceMemory m_textureBufferMemory;
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformBufferGpuMemory;
    VkDeviceSize m_uniformBufferOffsetToGpuMemory;
    ArraySafeRef<uint8_t> m_uniformBufferCpuMemory;
    
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSet m_descriptorSet;//automatically freed when the VkDescriptorPool is destroyed
    //END_#StreamingMemory

    VkDeviceSize m_uniformBufferCpuAlignment;
    VectorSafe<VkCommandBuffer, kSwapChainImagesNumMax> m_commandBuffersPrimary;//automatically freed when VkCommandPool is destroyed
    VectorSafe<ArraySafe<VkCommandBuffer, NTF_OBJECTS_NUM>, kSwapChainImagesNumMax> m_commandBuffersSecondary;//automatically freed when VkCommandPool is destroyed ///@todo: "cannot convert argument 2 from 'ArraySafe<VectorSafe<VkCommandBuffer,8>,2>' to 'ArraySafeRef<VectorSafeRef<VkCommandBuffer>>" -- even when provided with ArraySafeRef(VectorSafe<T, kSizeMax>& vectorSafe) and VectorSafeRef(VectorSafe<T, kSizeMax>& vectorSafe) -- not sure why
    VkCommandBuffer m_commandBufferTransfer;//automatically freed when VkCommandPool is destroyed
    VkCommandBuffer m_commandBufferTransitionImage;//automatically freed when VkCommandPool is destroyed

    ArraySafe<CommandBufferSecondaryThread, NTF_OBJECTS_NUM> m_commandBufferSecondaryThreads;
    ArraySafe<HANDLE, NTF_OBJECTS_NUM> m_commandBufferThreadDoneEvents;

    ArraySafe<uint32_t, NTF_OBJECTS_NUM> m_objectIndices;
    ArraySafe<CommandBufferThreadArguments, NTF_OBJECTS_NUM> m_commandBufferThreadArguments;

    /*  fences are mainly designed to synchronize your application itself with rendering operation, whereas semaphores are 
        used to synchronize operations within or across command queues */
    int m_frameIndex=0;
    VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM> m_imageAvailableSemaphore = VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM>(NTF_FRAMES_IN_FLIGHT_NUM);///<@todo NTF: refactor so this is a ArraySafe (eg that doesn't have a m_sizeCurrentSet) rather than the current incarnation of this class, which is more like a VectorSafe
    VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM> m_renderFinishedSemaphore = VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM>(NTF_FRAMES_IN_FLIGHT_NUM);///<@todo NTF: refactor so this is a ArraySafe (eg that doesn't have a m_sizeCurrentSet) rather than the current incarnation of this class, which is more like a VectorSafe
    VkSemaphore m_transferFinishedSemaphore;
    VectorSafe<VkFence, NTF_FRAMES_IN_FLIGHT_NUM> m_fence = VectorSafe<VkFence, NTF_FRAMES_IN_FLIGHT_NUM>(NTF_FRAMES_IN_FLIGHT_NUM);///<@todo NTF: refactor so this is a true ArraySafe (eg that doesn't have a m_sizeCurrentSet) rather than the current incarnation of this class, which is more like a VectorSafe

    VulkanPagedStackAllocator m_deviceLocalMemory;
    //BEG_#StagingBuffer
    VkBuffer m_stagingBufferGpu;
    VkDeviceSize m_stagingBufferGpuAlignmentStandard;
    ArraySafe<VkBuffer, 3> m_stagingBuffersGpu;
    VkDeviceMemory m_stagingBufferGpuMemory;
    VkDeviceSize m_offsetToFirstByteOfStagingBuffer;
    //END_#StagingBuffer
    StackCpu m_stagingBufferMemoryMapCpuToGpu;
};

int main() 
{
    static VulkanRendererNTF app;
    app.run();
    return EXIT_SUCCESS;
}

#include"ntf_vulkan.h"
#include"ntf_vulkan_utility.h"

VectorSafe<uint8_t, 8192 * 8192 * 4> s_pixelBufferScratch;

glm::vec3 s_cameraTranslation = glm::vec3(2.6f,3.4f,.9f);
VectorSafe<const char*, NTF_VALIDATION_LAYERS_SIZE> s_validationLayers;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    ///@todo: timer to get framerate-independent speed
    const float cameraSpeed = .1f;
    if (action == GLFW_REPEAT || action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_W)
        {
            s_cameraTranslation.x -= cameraSpeed;
        }
        else if (key == GLFW_KEY_S)
        {
            s_cameraTranslation.x += cameraSpeed;
        }
        else if (key == GLFW_KEY_A)
        {
            s_cameraTranslation.y -= cameraSpeed;
        }
        else if (key == GLFW_KEY_D)
        {
            s_cameraTranslation.y += cameraSpeed;
        }
        else if (key == GLFW_KEY_R)
        {
            s_cameraTranslation.z += cameraSpeed;
        }
        else if (key == GLFW_KEY_F)
        {
            s_cameraTranslation.z -= cameraSpeed;
        }
    }
}

//don't complain about scanf being unsafe
#pragma warning(disable : 4996)
///@todo: figure out which libraries I'm linking that trigger LNK4098 (seems like some libraries are linking /MD and /MDd and others are linking /MT and /MTd for C-runtime) -- for now, pass /IGNORE:4098 to the linker

class VulkanRendererNTF 
{
public:
#define NTF_FRAMES_IN_FLIGHT_NUM 2//#FramesInFlight

//BEG_#StreamingMemory
#define NTF_OBJECTS_NUM 2//number of unique models
#define NTF_DRAWS_PER_OBJECT_NUM 2
#define NTF_DRAW_CALLS_TOTAL (NTF_OBJECTS_NUM*NTF_DRAWS_PER_OBJECT_NUM)
//END_#StreamingMemory

    void Run() 
	{
        WindowInitialize(&m_window);
        VulkanInitialize();

        //#SecondaryCommandBufferMultithreadingTest: see m_commandBufferSecondaryThreadsTest definition for more comments
        //CommandBufferSecondaryThreadsCreateTest(&m_commandBufferSecondaryThreadsTest, &m_commandBufferThreadDoneEventsTest, &m_commandBufferThreadArgumentsTest, NTF_OBJECTS_NUM);

        ///@todo: #StreamingMemory: generalize
        WaitForSignalWindows(m_assetLoadingThreadData.m_handles.doneEventHandle);
        MainLoop(m_window);
        Shutdown();

        int i;
#if NTF_DEBUG
        printf("s_vulkanApiCpuBytesAllocatedMax=%zu\n", GetVulkanApiCpuBytesAllocatedMax());
#endif//#if NTF_DEBUG
        printf("Enter a character and press ENTER to exit\n");
        scanf("%i", &i);
    }

    ///@todo: fix; not maintained so totally broken
    ///@todo: unit test
    //void SwapChainRecreate()
    //{
    //    vkDeviceWaitIdle(m_device);

    //    CleanupSwapChain(
    //        &m_commandBuffersPrimary,
    //        m_device,
    //        m_depthImageView,
    //        m_depthImage,
    //        m_swapChainFramebuffers,
    //        m_commandPoolPrimary,
    //        m_commandPoolsSecondary,
    //        m_graphicsPipeline,
    //        m_pipelineLayout,
    //        m_renderPass,
    //        m_swapChainImageViews,
    //        m_swapChain);

    //    m_deviceLocalMemory.Destroy(m_device);
    //    m_deviceLocalMemory.Initialize(m_device, m_physicalDevice);

    //    CreateSwapChain(
    //        m_window, 
    //        &m_swapChain, 
    //        &m_swapChainImages, 
    //        &m_swapChainImageFormat, 
    //        &m_swapChainExtent, 
    //        m_physicalDevice, 
    //        NTF_FRAMES_IN_FLIGHT_NUM, 
    //        m_surface, 
    //        m_device);
    //    CreateImageViews(&m_swapChainImageViews, m_swapChainImages, m_swapChainImageFormat, m_device);
    //    CreateRenderPass(&m_renderPass, m_swapChainImageFormat, m_device, m_physicalDevice);
    //    CreateGraphicsPipeline(
    //        &m_pipelineLayout, 
    //        &m_graphicsPipeline, 
    //        g_stbAllocator, 
    //        m_renderPass, 
    //        m_descriptorSetLayout, 
    //        m_swapChainExtent, 
    //        m_device);
    //    
    //    //#CommandPoolDuplication
    //    AllocateCommandBuffers(
    //        ArraySafeRef<VkCommandBuffer>(&m_commandBufferTransfer, 1),
    //        m_commandPoolTransfer,
    //        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    //        1,
    //        m_device);
    //    AllocateCommandBuffers(
    //        ArraySafeRef<VkCommandBuffer>(&m_commandBufferTransitionImage, 1),
    //        m_commandPoolPrimary,
    //        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    //        1,
    //        m_device);

    //    CreateDepthResources(
    //        &m_depthImage,
    //        &m_depthImageView,
    //        &m_deviceLocalMemory,
    //        m_swapChainExtent,
    //        m_commandBufferTransitionImage,
    //        m_graphicsQueue,
    //        m_device,
    //        m_physicalDevice);
    //    CreateFramebuffers(&m_swapChainFramebuffers, m_swapChainImageViews, m_renderPass, m_swapChainExtent, m_depthImageView, m_device);

    //    //#CommandPoolDuplication
    //    const uint32_t swapChainFramebuffersSize = Cast_size_t_uint32_t(m_swapChainFramebuffers.size());
    //    m_commandBuffersPrimary.size(swapChainFramebuffersSize);//bake one command buffer for every image in the swapchain so Vulkan can blast through them
    //    AllocateCommandBuffers(
    //        &m_commandBuffersPrimary,
    //        m_commandPoolPrimary,
    //        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    //        swapChainFramebuffersSize,
    //        m_device);
    //}

private:
    void WindowInitialize(GLFWwindow**const windowPtrPtr)
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
        glfwSetWindowSizeCallback(windowPtr, VulkanRendererNTF::WindowResized);
        glfwSetKeyCallback(windowPtr, key_callback);
    }

    /*  Viewport and scissor rectangle size is specified during graphics pipeline creation, so the pipeline also needs to be rebuilt when the window 
        is resized. It is possible to avoid this by using dynamic state for the viewports and scissor rectangles */
    static void WindowResized(GLFWwindow* window, const int width, const int height)
    {
        if (width == 0 || height == 0) 
		{
			return;//handle the case where the window was minimized
		}

        VulkanRendererNTF* app = reinterpret_cast<VulkanRendererNTF*>(glfwGetWindowUserPointer(window));
        //app->SwapChainRecreate();
    }

    void Shutdown()
    {
        m_streamingUnit.Free(m_device);///<@todo NTF: generalize #StreamingMemory
        m_assetLoadingThreadData.m_threadCommand = AssetLoadingArguments::ThreadCommand::kCleanupAndTerminate;
        SignalSemaphoreWindows(m_assetLoadingThreadData.m_handles.wakeEventHandle);
        WaitForSignalWindows(m_assetLoadingThreadData.m_handles.doneEventHandle);

        CleanupSwapChain(
            &m_commandBuffersPrimary,
            m_device,
            m_depthImageView,
            m_depthImage,
            m_swapChainFramebuffers,
            m_commandPoolPrimary,
            m_commandPoolsSecondary,
            m_graphicsPipeline,
            m_renderPass,
            m_swapChainImageViews,
            m_swapChain);
                
        for (size_t frameIndex = 0; frameIndex < NTF_FRAMES_IN_FLIGHT_NUM; ++frameIndex)
        {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphore[frameIndex], GetVulkanAllocationCallbacks());
            vkDestroySemaphore(m_device, m_imageAvailableSemaphore[frameIndex], GetVulkanAllocationCallbacks());
            vkDestroyFence(m_device, m_drawFrameFinishedFences[frameIndex], GetVulkanAllocationCallbacks());
        }

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

        m_deviceLocalMemory.Destroy(m_device);
        vkDestroyDevice(m_device, GetVulkanAllocationCallbacks());
        DestroyDebugReportCallbackEXT(m_instance, m_callback, GetVulkanAllocationCallbacks());
        vkDestroySurfaceKHR(m_instance, m_surface, GetVulkanAllocationCallbacks());
        vkDestroyInstance(m_instance, GetVulkanAllocationCallbacks());

        glfwDestroyWindow(m_window);

        HandleCloseWindows(&m_assetLoadingThreadData.m_handles.threadHandle);
        HandleCloseWindows(&m_assetLoadingThreadData.m_handles.doneEventHandle);
        HandleCloseWindows(&m_assetLoadingThreadData.m_handles.wakeEventHandle);

        glfwTerminate();
    }

    void VulkanInitialize()
    {
        s_validationLayers.size(0);
        s_validationLayers.Push("VK_LAYER_LUNARG_standard_validation");
#if NTF_API_DUMP_VALIDATION_LAYER_ON
        s_validationLayers.Push("VK_LAYER_LUNARG_api_dump");///<this produces "file not found" after outputting to (I believe) stdout for a short while; seems like it overruns Windows 7's file descriptor or something.  Weirdly, running from Visual Studio 2015 does not seem to have this problem, but then I'm limited to 9999 lines of the command prompt VS2015 uses for output.  Not ideal
#endif//NTF_API_DUMP_VALIDATION_LAYER_ON

        
        m_deviceExtensions = VectorSafe<const char*, NTF_DEVICE_EXTENSIONS_NUM>({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });

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
            &m_drawFrameFinishedFences,
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

        m_streamingUnit.m_uniformBufferSizeUnaligned = sizeof(UniformBufferObject)*NTF_DRAWS_PER_OBJECT_NUM*NTF_OBJECTS_NUM;///#StreamingMemory

        m_assetLoadingThreadData.m_handles.doneEventHandle = ThreadSignalingEventCreate();
        m_assetLoadingThreadData.m_handles.wakeEventHandle = ThreadSignalingEventCreate();
        m_assetLoadingThreadData.m_threadCommand = AssetLoadingArguments::ThreadCommand::kLoadStreamingUnit;

        m_assetLoadingArguments.m_commandBufferTransfer = &m_commandBufferTransfer;
        m_assetLoadingArguments.m_commandBufferTransitionImage = &m_commandBufferTransitionImage;
        m_assetLoadingArguments.m_device = &m_device;
        m_assetLoadingArguments.m_deviceLocalMemory = &m_deviceLocalMemory;
        m_assetLoadingArguments.m_graphicsQueue = &m_graphicsQueue;
        m_assetLoadingArguments.m_graphicsPipeline = &m_graphicsPipeline;
        m_assetLoadingArguments.m_physicalDevice = &m_physicalDevice;
        m_assetLoadingArguments.m_queueFamilyIndices = &m_queueFamilyIndices;
        m_assetLoadingArguments.m_streamingUnit = &m_streamingUnit;
        m_assetLoadingArguments.m_streamingUnitFilenameNoExtension = m_streamingUnitTestNameNoExtension;
        m_assetLoadingArguments.m_threadCommand = &m_assetLoadingThreadData.m_threadCommand;
        m_assetLoadingArguments.m_threadDone = &m_assetLoadingThreadData.m_handles.doneEventHandle;
        m_assetLoadingArguments.m_threadWake = &m_assetLoadingThreadData.m_handles.wakeEventHandle;
        m_assetLoadingArguments.m_transferQueue = &m_transferQueue;

        m_assetLoadingArguments.m_renderPass = &m_renderPass;
        m_assetLoadingArguments.m_swapChainExtent = &m_swapChainExtent;
        
        m_assetLoadingArguments.AssertValid();

        ///@todo: THREAD_MODE_BACKGROUND_BEGIN or THREAD_PRIORITY_BELOW_NORMAL and SetThreadPriority
        m_assetLoadingThreadData.m_handles.threadHandle = CreateThreadWindows(AssetLoadingThread, &m_assetLoadingArguments);

        SignalSemaphoreWindows(m_assetLoadingThreadData.m_handles.wakeEventHandle);

        //#CommandPoolDuplication
        m_commandBuffersPrimary.size(swapChainFramebuffersSize);//bake one command buffer for every image in the swapchain so Vulkan can blast through them
        AllocateCommandBuffers(
            &m_commandBuffersPrimary,
            m_commandPoolPrimary,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            swapChainFramebuffersSize,
            m_device);
    }

    void MainLoop(GLFWwindow* window) 
    {
        assert(window);
        size_t frameIndex = 0;
        while (!glfwWindowShouldClose(window)) 
        {
            glfwPollEvents();

           //#StreamingMemory: update uniforms per streaming unit
            UpdateUniformBuffer(
                m_streamingUnit.m_uniformBufferCpuMemory,
                s_cameraTranslation,
                m_streamingUnit.m_uniformBufferGpuMemory,
                m_streamingUnit.m_uniformBufferOffsetToGpuMemory,
                NTF_DRAW_CALLS_TOTAL,
                m_streamingUnit.m_uniformBufferSizeAligned,
                m_swapChainExtent,
                m_device);

            const VkSemaphore imageAvailableSemaphore = m_imageAvailableSemaphore[frameIndex];
            uint32_t acquiredImageIndex;
            AcquireNextImage(&acquiredImageIndex, m_swapChain, imageAvailableSemaphore, m_device);

            //BEG_#SecondaryCommandBufferMultithreadingTest: see m_commandBufferSecondaryThreadsTest definition for more comments
            //FillSecondaryCommandBuffersTest(
            //    &m_commandBuffersSecondaryTest[acquiredImageIndex],
            //    &m_commandBufferSecondaryThreadsTest,
            //    &m_commandBufferThreadDoneEventsTest,
            //    &m_commandBufferThreadArgumentsTest,
            //    &m_texturedGeometries[0].descriptorSet,
            //    &m_swapChainFramebuffers[acquiredImageIndex],
            //    &m_renderPass,
            //    &m_swapChainExtent,
            //    &m_pipelineLayout,
            //    &m_graphicsPipeline,
            //    &m_texturedGeometries[0].vertexBuffer,
            //    &m_texturedGeometries[0].indexBuffer,
            //    &m_texturedGeometries[0].indicesSize,
            //    &m_objectIndicesSecondaryCommandBuffersTest,
            //    NTF_OBJECTS_NUM);

            //FillCommandBufferPrimary(
            //    m_commandBuffersPrimary[acquiredImageIndex],
            //    &m_commandBuffersSecondaryTest[acquiredImageIndex],
            //    NTF_OBJECTS_NUM,
            //    m_swapChainFramebuffers[acquiredImageIndex],
            //    m_renderPass,
            //    m_swapChainExtent);
            //END_#SecondaryCommandBufferMultithreadingTest

            FillCommandBufferPrimary(
                m_commandBuffersPrimary[acquiredImageIndex],
                &m_streamingUnit.m_texturedGeometries,
                m_streamingUnit.m_descriptorSet,
                NTF_OBJECTS_NUM,
                NTF_DRAWS_PER_OBJECT_NUM,
                m_swapChainFramebuffers[acquiredImageIndex],
                m_renderPass,
                m_swapChainExtent,
                m_streamingUnit.m_pipelineLayout,
                m_graphicsPipeline,
                m_device);

            DrawFrame(
                /*this,///#TODO_CALLBACK*/ 
                m_swapChain, 
                m_commandBuffersPrimary, 
                acquiredImageIndex,
                m_graphicsQueue, 
                m_presentQueue, 
                m_drawFrameFinishedFences[frameIndex],
                imageAvailableSemaphore,
                m_renderFinishedSemaphore[frameIndex],
                m_device);
            frameIndex = (frameIndex + 1) % NTF_FRAMES_IN_FLIGHT_NUM;
        }

        //wait for the logical device to finish operations before exiting MainLoop and destroying the window
        vkDeviceWaitIdle(m_device);
    }
    ///@todo: consider removing
    //void CopyBufferToGpuPrepareOnTransferQueue(
    //    VkBuffer*const gpuBufferPtr,
    //    VkDeviceMemory*const gpuBufferMemoryPtr,
    //    StackNTF<VkDeviceSize>*const stagingBufferGpuStackPtr,
    //    const VkDeviceSize offsetToFirstByteOfStagingBuffer,
    //    const void*const cpuBufferSource,
    //    const VkDeviceSize bufferSize,
    //    const VkMemoryPropertyFlags& memoryPropertyFlags,
    //    const bool residentForever = false)
    //{
    //    NTF_REF(gpuBufferPtr, gpuBuffer);
    //    NTF_REF(gpuBufferMemoryPtr, gpuBufferMemory);
    //    NTF_REF(stagingBufferGpuStackPtr, stagingBufferGpuStack);

    //    CopyBufferToGpuPrepare(
    //        &m_deviceLocalMemory,
    //        &gpuBuffer,
    //        &gpuBufferMemory,
    //        &m_stagingBuffersGpu,
    //        &stagingBufferGpuAllocateIndex,
    //        &stagingBufferMemoryMapCpuToGpu,
    //        &stagingBufferGpuStack,
    //        stagingBufferGpuMemory,
    //        stagingBufferGpuAlignmentStandard,
    //        offsetToFirstByteOfStagingBuffer,
    //        cpuBufferSource,
    //        bufferSize,
    //        memoryPropertyFlags,
    //        residentForever,
    //        m_commandBufferTransfer,
    //        m_device,
    //        m_physicalDevice);
    //}


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
    VkPipeline m_graphicsPipeline;
    VkCommandPool m_commandPoolPrimary, m_commandPoolTransfer;
    VectorSafe<ArraySafe<VkCommandPool, NTF_OBJECTS_NUM>, kSwapChainImagesNumMax> m_commandPoolsSecondary;
    VkImage m_depthImage;
    VkImageView m_depthImageView;

    glm::vec3 m_cameraTranslation;

    const char* m_streamingUnitTestNameNoExtension = "unitTest0";
    StreamingUnitRuntime m_streamingUnit;
    VectorSafe<VkCommandBuffer, kSwapChainImagesNumMax> m_commandBuffersPrimary;//automatically freed when VkCommandPool is destroyed
    
    //#SecondaryCommandBufferMultithreadingTest: see m_commandBufferSecondaryThreadsTest definition for more comments
    //VectorSafe<ArraySafe<VkCommandBuffer, NTF_OBJECTS_NUM>, kSwapChainImagesNumMax> m_commandBuffersSecondaryTest;//automatically freed when VkCommandPool is destroyed ///@todo: "cannot convert argument 2 from 'ArraySafe<VectorSafe<VkCommandBuffer,8>,2>' to 'ArraySafeRef<VectorSafeRef<VkCommandBuffer>>" -- even when provided with ArraySafeRef(VectorSafe<T, kElementsMax>& vectorSafe) and VectorSafeRef(VectorSafe<T, kElementsMax>& vectorSafe) -- not sure why
    
    VkCommandBuffer m_commandBufferTransfer;//automatically freed when VkCommandPool is destroyed
    VkCommandBuffer m_commandBufferTransitionImage;//automatically freed when VkCommandPool is destroyed

    //BEG_#SecondaryCommandBufferMultithreadingTest
    //this prototype worked as expected; but of course one secondary buffer per draw call is ridiculous, so this is removed, but commented out for reference in case command buffer construction becomes a bottleneck.  See October 7, 12:40:28, 2018 for last commit that had this code working
    //ArraySafe<ThreadHandles, NTF_OBJECTS_NUM> m_commandBufferSecondaryThreadsTest;
    //ArraySafe<HANDLE, NTF_OBJECTS_NUM> m_commandBufferThreadDoneEventsTest;

    //ArraySafe<uint32_t, NTF_OBJECTS_NUM> m_objectIndicesSecondaryCommandBuffersTest;
    //ArraySafe<CommandBufferThreadArgumentsTest, NTF_OBJECTS_NUM> m_commandBufferThreadArgumentsTest;
    //END_#SecondaryCommandBufferMultithreadingTest

    /*  fences are mainly designed to synchronize your application itself with rendering operation, whereas semaphores are 
        used to synchronize operations within or across command queues */
    int m_frameIndex=0;
    VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM> m_imageAvailableSemaphore = VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM>(NTF_FRAMES_IN_FLIGHT_NUM);///<@todo NTF: refactor use ArraySafe
    VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM> m_renderFinishedSemaphore = VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM>(NTF_FRAMES_IN_FLIGHT_NUM);///<@todo NTF: refactor use ArraySafe
        
    VectorSafe<VkFence, NTF_FRAMES_IN_FLIGHT_NUM> m_drawFrameFinishedFences = VectorSafe<VkFence, NTF_FRAMES_IN_FLIGHT_NUM>(NTF_FRAMES_IN_FLIGHT_NUM);///<@todo NTF: refactor use ArraySafe

    VulkanPagedStackAllocator m_deviceLocalMemory;
    AssetLoadingThreadData m_assetLoadingThreadData;
    AssetLoadingArguments m_assetLoadingArguments;
};

int main() 
{
    static VulkanRendererNTF app;
    app.Run();
    return EXIT_SUCCESS;
}

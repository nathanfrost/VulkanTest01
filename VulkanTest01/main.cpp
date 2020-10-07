#include"ntf_vulkan.h"
#include"ntf_vulkan_utility.h"
#include"StreamingUnitManager.h"
#include"StreamingUnitTest.h"
#include"WindowsUtil.h"

#if NTF_DEBUG
bool s_allowedToIssueStreamingCommands=false;
#endif//#if NTF_DEBUG

#if NTF_WIN_TIMER
extern FILE* s_winTimer;
#endif//NTF_WIN_TIMER

//LARGE_INTEGER g_queryPerformanceFrequency;

VectorSafe<uint8_t, 8192 * 8192 * 4> s_pixelBufferScratch;

WIN_TIMER_DEF(s_frameTimer);
glm::vec3 s_cameraTranslation = glm::vec3(0.f,0.f,0.f);
VectorSafe<const char*, NTF_VALIDATION_LAYERS_SIZE> s_validationLayers;

#define NTF_FRAMES_IN_FLIGHT_NUM 2//#FramesInFlight

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    ///@todo: timer to get framerate-independent speed
    const float cameraSpeed = .1f;
    if (action == GLFW_REPEAT || action == GLFW_PRESS)
    {
        //#WorldBasisVectors
        if (key == GLFW_KEY_W)//into screen
        {
            s_cameraTranslation.z += cameraSpeed;
        }
        else if (key == GLFW_KEY_S)//out of screen
        {
            s_cameraTranslation.z -= cameraSpeed;
        }
        else if (key == GLFW_KEY_A)//left
        {
            s_cameraTranslation.x -= cameraSpeed;
        }
        else if (key == GLFW_KEY_D)//right
        {
            s_cameraTranslation.x += cameraSpeed;
        }
        else if (key == GLFW_KEY_E)//up
        {
            s_cameraTranslation.y -= cameraSpeed;
        }
        else if (key == GLFW_KEY_Q)//down
        {
            s_cameraTranslation.y += cameraSpeed;
        }
    }
}

//don't complain about scanf being unsafe
#pragma warning(disable : 4996)

static void UnloadStreamingUnitsIfGpuDone(
    VectorSafeRef<StreamingUnitRuntime *> streamingUnitsToUnload, 
    VectorSafeRef<StreamingUnitRuntime *> streamingUnitsRenderable, 
    ArraySafeRef<bool> deviceLocalMemoryStreamingUnitsAllocated,
    RTL_CRITICAL_SECTION*const deviceLocalMemoryCriticalSectionPtr,
    const ConstVectorSafeRef<VulkanPagedStackAllocator>& deviceLocalMemoryStreamingUnits,
    const StreamingUnitRuntime::FrameNumber lastCpuFrameCompleted, 
    const VkDevice& device)
{
    NTF_REF(deviceLocalMemoryCriticalSectionPtr, deviceLocalMemoryCriticalSection);

    VectorSafe<StreamingUnitRuntime*, kStreamingUnitCommandsNum> streamingUnitsToUnloadRemaining;
    streamingUnitsToUnloadRemaining.Append(streamingUnitsToUnload);//assume no streaming units can be unloaded

    for (auto& streamingUnitToUnloadPtr : streamingUnitsToUnload)
    {
        NTF_REF(streamingUnitToUnloadPtr, streamingUnitToUnload);

        const StreamingUnitRuntime::State streamingUnitToUnloadState = streamingUnitToUnload.StateCriticalSection();
        switch (streamingUnitToUnloadState)
        {
            case StreamingUnitRuntime::State::kUnloaded:
            {
                //can't unload something that's not loaded; assert on the error but otherwise do nothing
                assert(false);
                streamingUnitsToUnloadRemaining.Remove(&streamingUnitToUnload);
                break;
            }
            case StreamingUnitRuntime::State::kLoading:
            {
                //don't remove streaming unit from streamingUnitsToUnloadRemaining so it will unload shortly after as it's loaded
                break;
            }
            case StreamingUnitRuntime::State::kLoaded:
            {
                streamingUnitsRenderable.Remove(&streamingUnitToUnload);//if an unload command is issued shortly (but not immediately) after load command, then when the unload command gets processed the streaming unit will be on the renderable list, and will need to be removed so that draw calls can stop being issued and the streaming unit can be unloaded
                assert(streamingUnitsRenderable.Find(&streamingUnitToUnload) < 0);

                const size_t frameNumberBits = sizeof(lastCpuFrameCompleted) << 3;
                const StreamingUnitRuntime::FrameNumber halfRange = CastWithAssert<size_t, StreamingUnitRuntime::FrameNumber>(1 << (frameNumberBits - 1));//one half of the range of an unsigned type

                NTF_LOG_STREAMING(  "%s:%i:%s.m_state=%i,%s.m_lastSubmittedCpuFrame=%u,lastCpuFrameCompleted=%u\n",
                                    __FILE__, __LINE__, streamingUnitToUnload.m_filenameNoExtension.data(), streamingUnitToUnload.m_state, streamingUnitToUnload.m_filenameNoExtension.data(), static_cast<unsigned int>(streamingUnitToUnload.m_lastSubmittedCpuFrame), lastCpuFrameCompleted);

                /*  Gpu is done with this streaming unit -- eg last submitted frame is in the present or the past, and last submitted cpu frame 
                    did not wrap around without last completed frame wrapping around as well */
                if (streamingUnitToUnload.m_lastSubmittedCpuFrame - lastCpuFrameCompleted > -halfRange && 
                    streamingUnitToUnload.m_lastSubmittedCpuFrame <= lastCpuFrameCompleted)                                                                               
                {
                    NTF_LOG_STREAMING(  "%s:%i:%s={m_lastSubmittedCpuFrame=%i,m_lastCpuFrameCompleted=%i}\n",
                                        __FILE__, __LINE__, streamingUnitToUnload.m_filenameNoExtension.data(), streamingUnitToUnload.m_lastSubmittedCpuFrame, lastCpuFrameCompleted);
                    streamingUnitsToUnloadRemaining.Remove(&streamingUnitToUnload);
                    streamingUnitToUnload.Free(
                        &deviceLocalMemoryStreamingUnitsAllocated,
                        &deviceLocalMemoryCriticalSection,
                        deviceLocalMemoryStreamingUnits,
                        false,
                        device);
                }//if (streamingUnitToUnload.m_lastSubmittedCpuFrame - lastCpuFrameCompleted > -halfRange && streamingUnitToUnload.m_lastSubmittedCpuFrame <= lastCpuFrameCompleted)
                break;
            }//case StreamingUnitRuntime::kLoaded
            default:
            {
                assert(false);
                break;
            }
        }
    }//for (auto& streamingUnitToUnloadPtr : streamingUnitsToUnload)

    streamingUnitsToUnload.size(0);
    streamingUnitsToUnload.Append(streamingUnitsToUnloadRemaining);
}

class VulkanRendererNTF 
{
public:

//BEG_#StreamingMemoryBasicModel
#define NTF_OBJECTS_NUM 2//number of unique models
#define NTF_DRAWS_PER_OBJECT_NUM 2
#define NTF_DRAW_CALLS_TOTAL (NTF_OBJECTS_NUM*NTF_DRAWS_PER_OBJECT_NUM)
//END_#StreamingMemoryBasicModel

    VulkanRendererNTF()
    {
        m_frameNumberCurrentCpu = 0;
    }

    void Run() 
	{
        WindowInitialize(&m_window);
        VulkanInitialize();
        MainLoop(m_window);
        Shutdown();
        printf("SHUTDOWN COMPLETE\n");
#if NTF_DEBUG
        printf("s_vulkanApiCpuBytesAllocatedMax=%zu\n", GetVulkanApiCpuBytesAllocatedMax());
#endif//#if NTF_DEBUG

#if !NTF_NO_KEYSTROKE_TO_END_PROCESS
        int i;
        printf("Enter a character and press ENTER to exit\n");
        scanf("%i", &i);
#endif//#if !NTF_NO_KEYSTROKE_TO_END_PROCESS
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
        m_assetLoadingThreadData.m_threadCommand = AssetLoadingArgumentsThreadCommand::kCleanupAndTerminate;
        SignalSemaphoreWindows(m_assetLoadingThreadData.m_handles.wakeEventHandle);
        WaitForSignalWindows(m_assetLoadingThreadData.m_handles.doneEventHandle);//no need to wait on any other critical sections; if this handle signals, the asset thread is finished and will not block any critical sections

#if NTF_DEBUG
        s_allowedToIssueStreamingCommands = true;
#endif//#if NTF_DEBUG

        //unload all loaded streaming units -- Gpu is assumed to be done with them -- eg post-vkDeviceWaitIdle(m_device)
        m_streamingUnitsToUnload.Append(m_streamingUnitsRenderable);
        m_streamingUnitsToUnload.Append(m_streamingUnitsToAddToRenderable);

        NTF_LOG_STREAMING(  "%s:%i:Shutdown():m_streamingUnitsToAddToRenderable.size()=%i,m_streamingUnitsRenderable.size()=%i,m_streamingUnitsToUnload.size()=%i\n", 
                            __FILE__, __LINE__, m_streamingUnitsToAddToRenderable.size(), m_streamingUnitsRenderable.size(), m_streamingUnitsToUnload.size());

        m_streamingUnitsToAddToRenderable.size(0);
        m_streamingUnitsRenderable.size(0);

        for(auto& streamingUnitToUnloadPtr : m_streamingUnitsToUnload)
        {
            NTF_REF(streamingUnitToUnloadPtr, streamingUnitToUnload);
            streamingUnitToUnload.Free(
                &m_deviceLocalMemoryStreamingUnitsAllocated, 
                &m_deviceLocalMemoryCriticalSection,
                m_deviceLocalMemoryStreamingUnits, 
                false, 
                m_device);
        }
        m_streamingUnitsToUnload.size(0);

		assert(m_streamingUnitsToUnload.size() == 0);
        assert(m_streamingUnitsRenderable.size() == 0);
		assert(m_streamingUnitsToAddToRenderable.size() == 0);
		//assert(m_streamingUnitsAddToLoad.size() == 0);//irrelevant; shutting down means load requests will not be executed

#if NTF_DEBUG
        s_allowedToIssueStreamingCommands = false;
#endif//#if NTF_DEBUG

        for (auto& streamingUnit : m_streamingUnits)
        {
            streamingUnit.Destroy(m_device);
        }

        CleanupSwapChain(
            m_commandBuffersPrimary,
            m_device,
            m_depthImageView,
            m_depthImage,
            m_swapChainFramebuffers,
            m_commandPoolPrimary,
            m_commandPoolsSecondary,
            m_renderPass,
            m_swapChainImageViews,
            m_swapChain);
                
        for (size_t frameIndex = 0; frameIndex < NTF_FRAMES_IN_FLIGHT_NUM; ++frameIndex)
        {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphore[frameIndex], GetVulkanAllocationCallbacks());
            vkDestroySemaphore(m_device, m_imageAvailableSemaphore[frameIndex], GetVulkanAllocationCallbacks());
            vkDestroyFence(m_device, m_drawFrameFinishedFences[frameIndex].m_fence, GetVulkanAllocationCallbacks());
        }

        vkFreeCommandBuffers(m_device, m_commandPoolTransitionImage, 1, &m_commandBufferTransitionImage);
        vkDestroyCommandPool(m_device, m_commandPoolTransitionImage, GetVulkanAllocationCallbacks());

        vkFreeCommandBuffers(m_device, m_commandPoolTransfer, 1, &m_commandBufferTransfer);
        vkDestroyCommandPool(m_device, m_commandPoolTransfer, GetVulkanAllocationCallbacks());
        vkDestroyCommandPool(m_device, m_commandPoolPrimary, GetVulkanAllocationCallbacks());
        for (auto& commandPoolSecondaryArray : m_commandPoolsSecondary)
        {
            for (auto& commandPoolSecondary : commandPoolSecondaryArray)
            {
                vkDestroyCommandPool(m_device, commandPoolSecondary, GetVulkanAllocationCallbacks());
            }
        }

        //free all streaming unit memory
        m_deviceLocalMemoryStreamingUnitsAllocated.MemsetEntireArray(0);
        for (auto& deviceLocalMemory : m_deviceLocalMemoryStreamingUnits)
        {
            deviceLocalMemory.Destroy(m_device);
        }
        m_deviceLocalMemoryPersistent.Destroy(m_device);

        vkDestroyDevice(m_device, GetVulkanAllocationCallbacks());
        DestroyDebugReportCallbackEXT(m_instance, m_callback, GetVulkanAllocationCallbacks());
        vkDestroySurfaceKHR(m_instance, m_surface, GetVulkanAllocationCallbacks());
        vkDestroyInstance(m_instance, GetVulkanAllocationCallbacks());

        glfwDestroyWindow(m_window);

        CriticalSectionDelete(&m_deviceLocalMemoryCriticalSection);
        CriticalSectionDelete(&m_graphicsQueueCriticalSection);
#if NTF_ASSET_LOADING_MULTITHREADED
        HandleCloseWindows(&m_assetLoadingThreadData.m_handles.threadHandle);
#endif//#if NTF_ASSET_LOADING_MULTITHREADED
        HandleCloseWindows(&m_assetLoadingThreadData.m_handles.doneEventHandle);
        HandleCloseWindows(&m_assetLoadingThreadData.m_handles.wakeEventHandle);
        CriticalSectionDelete(&m_streamingUnitsAddToLoadCriticalSection);
        CriticalSectionDelete(&m_streamingUnitsAddToRenderableCriticalSection);

        glfwTerminate();
    }

    void VulkanInitialize()
    {
#if NTF_UNIT_TEST_STREAMING_LOG
        CriticalSectionCreate(&s_streamingDebugCriticalSection);
        CriticalSectionEnter(&s_streamingDebugCriticalSection);
        Fopen(&s_streamingDebug, "StreamingDebug.txt", "w+");
        CriticalSectionLeave(&s_streamingDebugCriticalSection);
#endif//#if NTF_UNIT_TEST_STREAMING_LOG
        
        //QueryPerformanceFrequency(&g_queryPerformanceFrequency);

        s_validationLayers.size(0);
        s_validationLayers.Push("VK_LAYER_LUNARG_standard_validation");
#if NTF_API_DUMP_VALIDATION_LAYER_ON
        s_validationLayers.Push("VK_LAYER_LUNARG_api_dump");///<this produces "file not found" after outputting to (I believe) stdout for a short while; seems like it overruns Windows 7's file descriptor or something.  Weirdly, running from Visual Studio 2015 does not seem to have this problem, but then I'm limited to 9999 lines of the command prompt VS2015 uses for output.  Not ideal
#endif//NTF_API_DUMP_VALIDATION_LAYER_ON
        m_deviceExtensions = VectorSafe<const char*, NTF_DEVICE_EXTENSIONS_NUM>({VK_KHR_SWAPCHAIN_EXTENSION_NAME});

        m_instance = CreateInstance(s_validationLayers);
        m_callback = SetupDebugCallback(m_instance);
        CreateSurface(&m_surface, m_window, m_instance);//window surface needs to be created right before physical device creation, because it influences physical device selection
        PickPhysicalDevice(&m_physicalDevice, m_surface, m_deviceExtensions, m_instance);

        //if this is an Nvidia card, use diagnostic checkpoints in case of VK_ERROR_DEVICE_LOST
        VectorSafe<const char*, 1> deviceDiagnosticCheckpoints({ "VK_NV_device_diagnostic_checkpoints" });
        if (CheckDeviceExtensionSupport(m_physicalDevice, deviceDiagnosticCheckpoints))
        {
            m_deviceExtensions.Push(deviceDiagnosticCheckpoints[0]);//device diagnostic checkpoints are supported, so use them
            g_deviceDiagnosticCheckpointsSupported = true;//global variable used to avoid passing another boolean to practically every Vulkan function
        }

        NTFVulkanInitialize(m_physicalDevice);
        FindQueueFamilies(&m_queueFamilyIndices, m_physicalDevice, m_surface);
        CriticalSectionCreate(&m_graphicsQueueCriticalSection);
		CriticalSectionCreate(&m_streamingUnitsAddToLoadCriticalSection);
		CriticalSectionCreate(&m_streamingUnitsAddToRenderableCriticalSection);
        CriticalSectionCreate(&m_deviceLocalMemoryCriticalSection);
        CreateLogicalDevice(
            &m_device, 
            &m_graphicsQueue, 
            &m_presentQueue, 
            &m_transferQueue, 
            m_deviceExtensions, 
            s_validationLayers, 
            m_queueFamilyIndices, 
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
        
        CreateCommandPool(&m_commandPoolPrimary, m_queueFamilyIndices.index[QueueFamilyIndices::Type::kGraphicsQueue], m_device, m_physicalDevice);
        CreateCommandPool(&m_commandPoolTransitionImage, m_queueFamilyIndices.index[QueueFamilyIndices::Type::kGraphicsQueue], m_device, m_physicalDevice);
        CreateCommandPool(&m_commandPoolTransfer, m_queueFamilyIndices.index[QueueFamilyIndices::Type::kTransferQueue], m_device, m_physicalDevice);

        m_deviceLocalMemoryPersistent.Initialize(m_device, m_physicalDevice);
        for (auto& vulkanPagedStackAllocator : m_deviceLocalMemoryStreamingUnits)
        {
            vulkanPagedStackAllocator.Initialize(m_device, m_physicalDevice);
        }
        m_deviceLocalMemoryStreamingUnitsAllocated.MemsetEntireArray(0);
         
        //#CommandPoolDuplication
        AllocateCommandBuffers(
            ArraySafeRef<VkCommandBuffer>(&m_commandBufferTransfer, 1),
            m_commandPoolTransfer,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1,
            m_device);
        AllocateCommandBuffers(
			ArraySafeRef<VkCommandBuffer>(&m_commandBufferTransitionImage, 1),
            m_commandPoolTransitionImage,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1,
            m_device);

        VkFence initializationDone;
        FenceCreate(&initializationDone, static_cast<VkFenceCreateFlagBits>(0), m_device);
        BeginCommandBuffer(m_commandBufferTransitionImage, m_device);
        CreateDepthResources(
            &m_depthImage,
            &m_depthImageView,
            &m_deviceLocalMemoryPersistent,
            m_swapChainExtent,
            m_commandBufferTransitionImage,
            m_device,
            m_physicalDevice, 
            m_instance);
        EndCommandBuffer(m_commandBufferTransitionImage);
        SubmitCommandBuffer(
            nullptr,//no need to critical section, since currently only the main thread is running and we guard against launching the asset loading thread until this command buffer completes
            ConstVectorSafeRef<VkSemaphore>(),
            ConstVectorSafeRef<VkSemaphore>(),
            ConstArraySafeRef<VkPipelineStageFlags>(),
            m_commandBufferTransitionImage,
            m_graphicsQueue,
            initializationDone,
            m_instance);

        CreateFrameSyncPrimitives(
            &m_imageAvailableSemaphore,
            &m_renderFinishedSemaphore,
            &m_drawFrameFinishedFences,
            NTF_FRAMES_IN_FLIGHT_NUM,
            m_device);

        m_streamingUnits.size(3);
        for (auto& streamingUnit : m_streamingUnits)
        {
            streamingUnit.Initialize(m_device);
            streamingUnit.m_uniformBufferSizeUnaligned = sizeof(UniformBufferObject)*NTF_DRAWS_PER_OBJECT_NUM*NTF_OBJECTS_NUM;///#StreamingMemoryBasicModel
        }
        m_streamingUnits[0].m_filenameNoExtension = ConstStringSafe(g_streamingUnitName_UnitTest0);
        m_streamingUnits[1].m_filenameNoExtension = ConstStringSafe(g_streamingUnitName_UnitTest1);
        m_streamingUnits[2].m_filenameNoExtension = ConstStringSafe(g_streamingUnitName_UnitTest2);

        //m_streamingUnits[3].m_filenameNoExtension = ConstStringSafe(g_streamingUnitName_TriangleCounterClockwise);
        //m_streamingUnits[3].m_filenameNoExtension = ConstStringSafe(g_streamingUnitName_TriangleClockwise);

        CreateFramebuffers(&m_swapChainFramebuffers, m_swapChainImageViews, m_renderPass, m_swapChainExtent, m_depthImageView, m_device);
        
        const uint32_t swapChainFramebuffersSize = CastWithAssert<size_t,uint32_t>(m_swapChainFramebuffers.size());
        m_commandPoolsSecondary.size(swapChainFramebuffersSize);
        for (auto& commandPoolSecondaryArray : m_commandPoolsSecondary)
        {
            for (auto& commandPoolSecondary : commandPoolSecondaryArray)
            {
                CreateCommandPool(&commandPoolSecondary, m_queueFamilyIndices.index[QueueFamilyIndices::Type::kGraphicsQueue], m_device, m_physicalDevice);
            }
        }

        m_assetLoadingThreadData.m_handles.doneEventHandle = ThreadSignalingEventCreate();
        m_assetLoadingThreadData.m_handles.wakeEventHandle = ThreadSignalingEventCreate();
        
        m_assetLoadingThreadData.m_threadCommand = AssetLoadingArgumentsThreadCommand::kProcessStreamingUnits;

        m_assetLoadingArguments.m_assetLoadingThreadIdle = &m_assetLoadingThreadIdle;
        m_assetLoadingArguments.m_deviceLocalMemoryPersistent = &m_deviceLocalMemoryPersistent;
        m_assetLoadingArguments.m_deviceLocalMemoryStreamingUnits = &m_deviceLocalMemoryStreamingUnits;
        m_assetLoadingArguments.m_deviceLocalMemoryStreamingUnitsAllocated = &m_deviceLocalMemoryStreamingUnitsAllocated;
		m_assetLoadingArguments.m_streamingUnitsToAddToLoad = &m_streamingUnitsToAddToLoad;
		m_assetLoadingArguments.m_streamingUnitsToAddToRenderable = &m_streamingUnitsToAddToRenderable;
		m_assetLoadingArguments.m_threadCommand = &m_assetLoadingThreadData.m_threadCommand;

        m_assetLoadingArguments.m_commandBufferTransfer = &m_commandBufferTransfer;
        m_assetLoadingArguments.m_commandBufferTransitionImage = &m_commandBufferTransitionImage;
        m_assetLoadingArguments.m_device = &m_device;
        m_assetLoadingArguments.m_deviceLocalMemoryCriticalSection = &m_deviceLocalMemoryCriticalSection;
        m_assetLoadingArguments.m_graphicsQueue = &m_graphicsQueue;
        m_assetLoadingArguments.m_graphicsQueueCriticalSection = &m_graphicsQueueCriticalSection;
        m_assetLoadingArguments.m_instance = &m_instance;
        m_assetLoadingArguments.m_physicalDevice = &m_physicalDevice;
		m_assetLoadingArguments.m_queueFamilyIndices = &m_queueFamilyIndices;
		m_assetLoadingArguments.m_renderPass = &m_renderPass;
		m_assetLoadingArguments.m_streamingUnitsToAddToLoadCriticalSection = &m_streamingUnitsAddToLoadCriticalSection;
		m_assetLoadingArguments.m_streamingUnitsToAddToRenderableCriticalSection = &m_streamingUnitsAddToRenderableCriticalSection;
        m_assetLoadingArguments.m_swapChainExtent = &m_swapChainExtent;
		m_assetLoadingArguments.m_threadDone = &m_assetLoadingThreadData.m_handles.doneEventHandle;
		m_assetLoadingArguments.m_threadWake = &m_assetLoadingThreadData.m_handles.wakeEventHandle;
		m_assetLoadingArguments.m_transferQueue = &m_transferQueue;

        m_assetLoadingArguments.AssertValid();

#if NTF_ASSET_LOADING_MULTITHREADED
        m_assetLoadingThreadData.m_handles.threadHandle = CreateThreadWindows(AssetLoadingThread, &m_assetLoadingArguments);
#else
        AssetLoadingThreadPersistentResourcesCreate(&m_assetLoadingPersistentResources, &m_deviceLocalMemoryPersistent, m_physicalDevice, m_device);
#endif//#if NTF_ASSET_LOADING_MULTITHREADED

		//BEG_#StreamingTest
        //load first streaming unit
		//StreamingUnitAddToLoadCriticalSection(
		//	&m_streamingUnits[0], 
		//	&m_streamingUnitsAddToLoad, 
		//	&m_streamingUnitsRenderable, 
		//	m_streamingUnitsAddToLoadListCriticalSection);
		//
		//SignalSemaphoreWindows(m_assetLoadingThreadData.m_handles.wakeEventHandle);
		//END_#StreamingTest

        //finish initialization before launching asset loading thread
        FenceWaitUntilSignalled(initializationDone, m_device);
        vkDestroyFence(m_device, initializationDone, GetVulkanAllocationCallbacks());
        
        //#CommandPoolDuplication
        m_commandBuffersPrimary.size(swapChainFramebuffersSize);//bake one command buffer for every image in the swapchain so Vulkan can blast through them
        AllocateCommandBuffers(
            &m_commandBuffersPrimary,
            m_commandPoolPrimary,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            swapChainFramebuffersSize,
            m_device);
    }

    static void IfLargerDistanceThenUpdateFrameNumber(
        StreamingUnitRuntime::FrameNumberSigned*const biggestDistancePtr, 
        StreamingUnitRuntime::FrameNumber*const distanceFrameNumberCpuSubmittedPtr,
        const StreamingUnitRuntime::FrameNumber distance,
        const StreamingUnitRuntime::FrameNumber frameNumberCpuSubmitted)
    {
        NTF_REF(biggestDistancePtr, biggestDistance);
        NTF_REF(distanceFrameNumberCpuSubmittedPtr, distanceFrameNumberCpuSubmitted);

        if (distance > biggestDistance)
        {
            biggestDistance = distance;
            distanceFrameNumberCpuSubmitted = frameNumberCpuSubmitted;
        }
    }
    static void LastFrameGpuCompletedDetermine(
        ArraySafeRef<DrawFrameFinishedFence> drawFrameFinishedFences,
        StreamingUnitRuntime::FrameNumber*const lastCpuFrameCompletedPtr,
        const VkDevice& device)
    {
        NTF_REF(lastCpuFrameCompletedPtr, lastCpuFrameCompleted);

        const StreamingUnitRuntime::FrameNumberSigned biggestPositiveDistanceInitial = 0;
        const StreamingUnitRuntime::FrameNumberSigned biggestNegativeDistanceInitial = StreamingUnitRuntime::kFrameNumberSignedMinimum;
        StreamingUnitRuntime::FrameNumberSigned biggestPositiveDistance = biggestPositiveDistanceInitial, biggestNegativeDistance = biggestNegativeDistanceInitial;//negative distance handles the wraparound case
        StreamingUnitRuntime::FrameNumber positiveDistanceFrameNumberCpuSubmitted = 0, negativeDistanceFrameNumberCpuSubmitted = 0;
        for (int i = 0; i < NTF_FRAMES_IN_FLIGHT_NUM; ++i)
        {
            auto& drawFrameFinishedFenceToCheck = drawFrameFinishedFences[i];
            const StreamingUnitRuntime::FrameNumber frameNumberCpuSubmitted = drawFrameFinishedFenceToCheck.m_frameNumberCpuSubmitted;
            if (!drawFrameFinishedFenceToCheck.m_frameNumberCpuCompletedByGpu &&
                vkGetFenceStatus(device, drawFrameFinishedFenceToCheck.m_fence) == VK_SUCCESS)//Gpu has reported a new frame completed since last Cpu checked
            {
                drawFrameFinishedFenceToCheck.m_frameNumberCpuCompletedByGpu = true;//we are about to process this frame number as completed
                const StreamingUnitRuntime::FrameNumberSigned distance = frameNumberCpuSubmitted - lastCpuFrameCompleted;
                if (distance < 0)
                {
                    //wraparound case; the most recent frames are the largest negative distance
                    IfLargerDistanceThenUpdateFrameNumber(
                        &biggestNegativeDistance,
                        &negativeDistanceFrameNumberCpuSubmitted,
                        distance,
                        frameNumberCpuSubmitted);
                }
                else
                {
                    //regular case; the most recent frames are the largest
                    IfLargerDistanceThenUpdateFrameNumber(
                        &biggestPositiveDistance,
                        &positiveDistanceFrameNumberCpuSubmitted,
                        distance,
                        frameNumberCpuSubmitted);
                }
            }
        }
        if (biggestPositiveDistance > biggestPositiveDistanceInitial || biggestNegativeDistance > biggestNegativeDistanceInitial)
        {
            //Gpu processed a new frame
            lastCpuFrameCompleted = biggestNegativeDistance > StreamingUnitRuntime::kFrameNumberSignedMinimum ?
                                    negativeDistanceFrameNumberCpuSubmitted : positiveDistanceFrameNumberCpuSubmitted;
        }
    }
    void MainLoop(GLFWwindow* window)
    {
        assert(window);

        while (!glfwWindowShouldClose(window)) 
        {
            glfwPollEvents();

            bool streamingUnitRenderedThisFrame = false;
            uint32_t acquiredImageIndex;
            VkSemaphore imageAvailableSemaphore;
            VkCommandBuffer commandBufferPrimary;
            VkFramebuffer swapChainFramebuffer;

			//add newly loaded streaming units to the renderable list
            CriticalSectionEnter(&m_streamingUnitsAddToRenderableCriticalSection);
            
#if NTF_DEBUG
            for (auto& streamingUnitPtrToAdd : m_streamingUnitsToAddToRenderable)
            {
                for (auto& streamingUnitPtrAlreadyPresent : m_streamingUnitsRenderable)
                {
                    assert(streamingUnitPtrToAdd != streamingUnitPtrAlreadyPresent);
                }
            }
#endif//#if NTF_DEBUG

			m_streamingUnitsRenderable.Append(m_streamingUnitsToAddToRenderable);
            for (auto& streamingUnitPtr : m_streamingUnitsToAddToRenderable)
            {
                NTF_REF(streamingUnitPtr, streamingUnit);

                CriticalSectionEnter(&streamingUnit.m_stateCriticalSection);
                streamingUnit.m_state = StreamingUnitRuntime::State::kLoaded;//must happen here, because now the streaming unit is guaranteed to be rendered at least once (correctly defining its cpu frame submitted number), which will allow it to be unloaded correctly -- all provided the app doesn't shut down first
                NTF_LOG_STREAMING("%s:%i:%s.m_state=%zu\n",
                    __FILE__, __LINE__, streamingUnit.m_filenameNoExtension.data(), streamingUnit.m_state);
                CriticalSectionLeave(&streamingUnit.m_stateCriticalSection);
            }

			m_streamingUnitsToAddToRenderable.size(0);
			CriticalSectionLeave(&m_streamingUnitsAddToRenderableCriticalSection);

            //fill primary command buffers with loaded streaming units
            const size_t streamingUnitsToRenderNum = m_streamingUnitsRenderable.size();
            auto& drawFrameFinishedFence = m_drawFrameFinishedFences[m_frameResourceIndex];
            if (streamingUnitsToRenderNum)
            {
                streamingUnitRenderedThisFrame = true;
                imageAvailableSemaphore = m_imageAvailableSemaphore[m_frameResourceIndex];
                AcquireNextImage(&acquiredImageIndex, m_swapChain, imageAvailableSemaphore, m_device);
                commandBufferPrimary = m_commandBuffersPrimary[acquiredImageIndex];
                swapChainFramebuffer = m_swapChainFramebuffers[acquiredImageIndex];

                BeginCommandBuffer(commandBufferPrimary, m_device);

                VkRenderPassBeginInfo renderPassInfo = {};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = m_renderPass;
                renderPassInfo.framebuffer = swapChainFramebuffer;

                //any pixels outside of the area defined here have undefined values; we don't want that
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = m_swapChainExtent;

                const size_t kClearValueNum = 2;
                VectorSafe<VkClearValue, kClearValueNum> clearValues(kClearValueNum);
                clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
                clearValues[1].depthStencil = { 0.f, 0 };

                renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
                renderPassInfo.pClearValues = clearValues.data();

                CmdSetCheckpointNV(commandBufferPrimary, &s_cmdSetCheckpointData[static_cast<size_t>(CmdSetCheckpointValues::vkCmdBeginRenderPass_kBefore)], m_instance);
                vkCmdBeginRenderPass(commandBufferPrimary, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE/**<no secondary buffers will be executed; VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS = secondary command buffers will execute these commands*/);
                CmdSetCheckpointNV(commandBufferPrimary, &s_cmdSetCheckpointData[static_cast<size_t>(CmdSetCheckpointValues::vkCmdBeginRenderPass_kAfter)], m_instance);

                for (auto& streamingUnitPtr : m_streamingUnitsRenderable)
                {
                    NTF_REF(streamingUnitPtr, streamingUnit);
                    assert(streamingUnit.m_state == StreamingUnitRuntime::State::kLoaded);
                    NTF_LOG_STREAMING("%s:%i:MainLoop():%s.m_state=%zu\n", __FILE__, __LINE__, streamingUnit.m_filenameNoExtension.data(), streamingUnit.m_state);

                    UpdateUniformBuffer(
                        streamingUnit.m_uniformBufferCpuMemory,
                        s_cameraTranslation,
                        streamingUnit.m_uniformBufferGpuMemory,
                        streamingUnit.m_uniformBufferOffsetToGpuMemory,
                        NTF_DRAW_CALLS_TOTAL,
                        streamingUnit.m_uniformBufferSizeAligned,
                        m_swapChainExtent,
                        m_device);

                    NTF_LOG_STREAMING(  "%s:%i:About to call FillCommandBufferPrimary():%s.m_lastSubmittedCpuFrame=%i\n",
                                        __FILE__, __LINE__, streamingUnit.m_filenameNoExtension.data(), streamingUnit.m_lastSubmittedCpuFrame);
                    FillCommandBufferPrimary(
                        &streamingUnit.m_lastSubmittedCpuFrame,
                        m_frameNumberCurrentCpu,
                        commandBufferPrimary,
                        streamingUnit.m_texturedGeometries,
                        streamingUnit.m_descriptorSet,
                        NTF_OBJECTS_NUM,
                        NTF_DRAWS_PER_OBJECT_NUM,
                        streamingUnit.m_pipelineLayout,
                        streamingUnit.m_graphicsPipeline,
                        m_instance);
                    NTF_LOG_STREAMING(  "%s:%i:Completed FillCommandBufferPrimary():%s.m_lastSubmittedCpuFrame=%i\n",
                                        __FILE__, __LINE__, streamingUnit.m_filenameNoExtension.data(), streamingUnit.m_lastSubmittedCpuFrame);
                }

                CmdSetCheckpointNV(commandBufferPrimary, &s_cmdSetCheckpointData[static_cast<size_t>(CmdSetCheckpointValues::vkCmdEndRenderPass_kAfter)], m_instance);
                vkCmdEndRenderPass(commandBufferPrimary);
                CmdSetCheckpointNV(commandBufferPrimary, &s_cmdSetCheckpointData[static_cast<size_t>(CmdSetCheckpointValues::vkCmdEndRenderPass_kAfter)], m_instance);
                EndCommandBuffer(commandBufferPrimary);

                //if we're Gpu-bound, wait until the Gpu is ready for another frame
                if (!drawFrameFinishedFence.m_frameNumberCpuCompletedByGpu)//only check fence that has been submitted to Gpu but has not yet signaled (eg command buffer submitted with this fence has not completed yet)
                {
                    WIN_TIMER_DEF_START(waitForFences);
                    FenceWaitUntilSignalled(drawFrameFinishedFence.m_fence, m_device);
                    WIN_TIMER_STOP(waitForFences);
                }
#if NTF_WIN_TIMER
                WIN_TIMER_STOP(s_frameTimer);
                FwriteSprintf(s_winTimer, "s_frameTimer:%fms\n", WIN_TIMER_ELAPSED_MILLISECONDS(s_frameTimer));
                WIN_TIMER_START(s_frameTimer);
#endif//#if NTF_WIN_TIMER
            }//if (streamingUnitsToRenderNum)

            //determine last Cpu frame that the Gpu finished with -- needs to be done here, immediately before the fence from the most recently completed Gpu frame is reset (since this logic depends on seeing the signaled state of this fence)
            LastFrameGpuCompletedDetermine(&m_drawFrameFinishedFences, &m_lastCpuFrameCompleted, m_device);

            if (streamingUnitsToRenderNum)
            {
                FenceReset(drawFrameFinishedFence.m_fence, m_device);//this fence has signaled, showing that the Gpu is ready for a new frame submission

                //theoretically the implementation can already start executing our vertex shader and such while the image is not
                //available yet. Each entry in the waitStages array corresponds to the semaphore with the same index in pWaitSemaphores
                VectorSafe<VkSemaphore, 4> signalSemaphores({ m_renderFinishedSemaphore[m_frameResourceIndex] });
                VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                SubmitCommandBuffer(
                    &m_graphicsQueueCriticalSection,
                    ConstVectorSafeRef<VkSemaphore>(&imageAvailableSemaphore, 1),
                    signalSemaphores,
                    ConstArraySafeRef<VkPipelineStageFlags>(&waitStages, 1),
                    commandBufferPrimary,
                    m_graphicsQueue,
                    drawFrameFinishedFence.m_fence,
                    m_instance);

                drawFrameFinishedFence.m_frameNumberCpuSubmitted = m_frameNumberCurrentCpu;//so we know when it's safe to unload streaming unit's assets
                drawFrameFinishedFence.m_frameNumberCpuCompletedByGpu = false;//this frame has not been completed by the Gpu until this fence signals

                VkPresentInfoKHR presentInfo = {};
                presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                presentInfo.waitSemaphoreCount = 1;
                presentInfo.pWaitSemaphores = signalSemaphores.data();

                VkSwapchainKHR swapChains[] = { m_swapChain };
                presentInfo.swapchainCount = 1;
                presentInfo.pSwapchains = swapChains;
                presentInfo.pResults = nullptr; // allows you to specify an array of VkResult values to check for every individual swap chain if presentation was successful
                presentInfo.pImageIndices = &acquiredImageIndex;

                const bool unifiedGraphicsAndPresentQueue = m_presentQueue == m_graphicsQueue;
                if (unifiedGraphicsAndPresentQueue)
                {
                    CriticalSectionEnter(&m_graphicsQueueCriticalSection);
                }
                const VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
                if (unifiedGraphicsAndPresentQueue)
                {
                    CriticalSectionLeave(&m_graphicsQueueCriticalSection);
                }
                if (result == VK_ERROR_OUT_OF_DATE_KHR/*swap chain can no longer be used for rendering*/ ||
                    result == VK_SUBOPTIMAL_KHR/*swap chain can still present image, but surface properties don't entirely match; for example, during resizing*/)
                {
                    ///#TODO_CALLBACK
                    //hackToRecreateSwapChainIfNecessary.SwapChainRecreate();//haven't seen this get hit yet, even when minimizing and resizing the window
                    printf("FAIL: vkQueuePresentKHR() returned %i", result);
                }
                NTF_VK_ASSERT_SUCCESS(result);

                m_frameResourceIndex = (m_frameResourceIndex + 1) % NTF_FRAMES_IN_FLIGHT_NUM;
            }//if (streamingUnitsToRenderNum)
            UnloadStreamingUnitsIfGpuDone(
                &m_streamingUnitsToUnload, 
                &m_streamingUnitsRenderable,
                &m_deviceLocalMemoryStreamingUnitsAllocated,
                &m_deviceLocalMemoryCriticalSection,
                m_deviceLocalMemoryStreamingUnits,
                m_lastCpuFrameCompleted, 
                m_device);

#if !NTF_ASSET_LOADING_MULTITHREADED
            StreamingCommandsProcess(&m_assetLoadingArguments, &m_assetLoadingPersistentResources);
#endif//!NTF_ASSET_LOADING_MULTITHREADED

#if NTF_DEBUG
            s_allowedToIssueStreamingCommands = true;
#endif//#if NTF_DEBUG

            //BEG_HAC
            //static bool s_loadedSimpleTriangle;
            //if (!s_loadedSimpleTriangle)
            //{
            //    StreamingUnitAddToLoadCriticalSection(&m_streamingUnits[3], &m_streamingUnitsToAddToLoad, &m_streamingUnitsAddToLoadCriticalSection);///@todo_NTF: should pair stream load queue with critical section to reduce error proneness
            //    AssetLoadingThreadExecuteLoad(&m_assetLoadingThreadData.m_threadCommand, m_assetLoadingThreadData.m_handles.wakeEventHandle);
            //    s_loadedSimpleTriangle = true;
            //}
            //END_HAC

            StreamingUnitTestTick(
                &m_streamingUnits[0],
                &m_streamingUnits[1],
                &m_streamingUnits[2],
                &m_streamingUnitsToAddToLoad,
                &m_streamingUnitsRenderable,
                &m_streamingUnitsToUnload,
                &m_assetLoadingThreadData.m_threadCommand,
                &m_streamingUnitsAddToLoadCriticalSection,
                &m_assetLoadingThreadIdle,
                m_assetLoadingThreadData.m_handles.wakeEventHandle,
                m_frameNumberCurrentCpu,
                24);
#if NTF_DEBUG
            s_allowedToIssueStreamingCommands = false;
#endif//#if NTF_DEBUG

            ++m_frameNumberCurrentCpu;
        }//while (!glfwWindowShouldClose(window))

#if !NTF_ASSET_LOADING_MULTITHREADED
        AssetLoadingPersistentResourcesDestroy(&m_assetLoadingPersistentResources, m_assetLoadingThreadData.m_handles.doneEventHandle, m_device);
#endif//!NTF_ASSET_LOADING_MULTITHREADED
        //wait for the logical device to finish operations before exiting MainLoop and destroying the window
        vkDeviceWaitIdle(m_device);
    }

    GLFWwindow* m_window;
    VkInstance m_instance;//need not be synchronized until, of course, vkDestroyInstance()
    VkDebugReportCallbackEXT m_callback;
    VkSurfaceKHR m_surface;
    QueueFamilyIndices m_queueFamilyIndices;//needs no synchronization; queried during creation-time and then immutable from that point forward
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;//doesn't need to be deleted, since physical devices can't be created or destroyed by software.  Needs no synchronization
    VkDevice m_device;//interface to the physical device; must be destroyed before the physical device.  Need not be synchronized until, of course, vkDestroyDevice()
    VkSwapchainKHR m_swapChain;//must be destroyed before the logical device
    VkQueue m_graphicsQueue, m_presentQueue, m_transferQueue;//queues are implicitly cleaned up with the logical device; no need to delete
    RTL_CRITICAL_SECTION m_graphicsQueueCriticalSection;//synchronizes one-and-only graphics queue between asset loading thread (which uses it to load textures) and main thread.  On devices that don't have separate transfer or present queues, this mutex synchronizes these queue accesses as well
    VectorSafe<const char*, NTF_DEVICE_EXTENSIONS_NUM> m_deviceExtensions;
    enum { kSwapChainImagesNumMax=8 };
    VectorSafe<VkImage, kSwapChainImagesNumMax> m_swapChainImages;//handles to images, which are created by the swapchain and will be destroyed by the swapchain.  Images are "multidimensional - up to 3 - arrays of data which can be used for various purposes (e.g. attachments, textures), by binding them to a graphics or compute pipeline via descriptor sets, or by directly specifying them as parameters to certain commands" -- https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VkImage.html
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;//needs no synchronization; queried during creation-time and then immutable from that point forward
    VectorSafe<VkImageView, kSwapChainImagesNumMax> m_swapChainImageViews;//defines type of image (eg color buffer with mipmaps, depth buffer, and so on)
    VectorSafe<VkFramebuffer, kSwapChainImagesNumMax> m_swapChainFramebuffers;
    VkRenderPass m_renderPass;//needs no synchronization
    VectorSafe<ArraySafe<VkCommandPool, NTF_OBJECTS_NUM>, kSwapChainImagesNumMax> m_commandPoolsSecondary;
    VkImage m_depthImage;
    VkImageView m_depthImageView;

    glm::vec3 m_cameraTranslation;

    VkCommandPool m_commandPoolPrimary, m_commandPoolTransfer, m_commandPoolTransitionImage;

    enum { kStreamingUnitsNum = 6 };
    VectorSafe<StreamingUnitRuntime, kStreamingUnitsNum> m_streamingUnits;
	VectorSafe<StreamingUnitRuntime*, kStreamingUnitCommandsNum> m_streamingUnitsToUnload;
	VectorSafe<StreamingUnitRuntime*, kStreamingUnitsRenderableNum> m_streamingUnitsRenderable;

    RTL_CRITICAL_SECTION m_streamingUnitsAddToRenderableCriticalSection;
	VectorSafe<StreamingUnitRuntime*, kStreamingUnitCommandsNum> m_streamingUnitsToAddToRenderable;

    RTL_CRITICAL_SECTION m_streamingUnitsAddToLoadCriticalSection;
	VectorSafe<StreamingUnitRuntime*, kStreamingUnitCommandsNum> m_streamingUnitsToAddToLoad;

    VectorSafe<VkCommandBuffer, kSwapChainImagesNumMax> m_commandBuffersPrimary;//automatically freed when VkCommandPool is destroyed
        
    VkCommandBuffer m_commandBufferTransfer;//automatically freed when VkCommandPool is destroyed
    VkCommandBuffer m_commandBufferTransitionImage;//automatically freed when VkCommandPool is destroyed

    /*  fences are mainly designed to synchronize your application itself with rendering operation, whereas semaphores are 
        used to synchronize operations within or across command queues */
    size_t m_frameResourceIndex = 0;
    ArraySafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM> m_imageAvailableSemaphore = VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM>(NTF_FRAMES_IN_FLIGHT_NUM);
    ArraySafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM> m_renderFinishedSemaphore = VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM>(NTF_FRAMES_IN_FLIGHT_NUM);
    ArraySafe<DrawFrameFinishedFence, NTF_FRAMES_IN_FLIGHT_NUM> m_drawFrameFinishedFences;

    RTL_CRITICAL_SECTION m_deviceLocalMemoryCriticalSection;
    VulkanPagedStackAllocator m_deviceLocalMemoryPersistent;
    VectorSafe<VulkanPagedStackAllocator, kStreamingUnitsNum> m_deviceLocalMemoryStreamingUnits = 
        VectorSafe<VulkanPagedStackAllocator, kStreamingUnitsNum>(kStreamingUnitsNum);
    ArraySafe<bool, kStreamingUnitsNum> m_deviceLocalMemoryStreamingUnitsAllocated;

	AssetLoadingThreadData m_assetLoadingThreadData;
    AssetLoadingArguments m_assetLoadingArguments;
    bool m_assetLoadingThreadIdle;///<no critical section, because only the asset thread writes to it
#if !NTF_ASSET_LOADING_MULTITHREADED
    AssetLoadingPersistentResources m_assetLoadingPersistentResources;
#endif//#if NTF_ASSET_LOADING_MULTITHREADED

    StreamingUnitRuntime::FrameNumber m_frameNumberCurrentCpu;
    StreamingUnitRuntime::FrameNumber m_lastCpuFrameCompleted = 0;
};

int main() 
{
    static VulkanRendererNTF app;
    app.Run();
    return EXIT_SUCCESS;
}

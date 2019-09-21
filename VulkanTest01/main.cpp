#include"ntf_vulkan.h"
#include"ntf_vulkan_utility.h"
#include"StreamingUnitManager.h"

#if NTF_WIN_TIMER
extern FILE* s_winTimer;
#endif//NTF_WIN_TIMER

//LARGE_INTEGER g_queryPerformanceFrequency;

VectorSafe<uint8_t, 8192 * 8192 * 4> s_pixelBufferScratch;

WIN_TIMER_DEF(s_frameTimer);
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

static void UnitTest(
    StreamingUnitRuntime*const streamingUnit0,
    StreamingUnitRuntime*const streamingUnit1,
    StreamingUnitRuntime*const streamingUnit2,
    StreamingCommandQueueManager*const streamingUnitRuntimeQueueManager,
    const HANDLE assetLoadingThreadWakeHandle)
{
    static enum class UnitTestState:size_t
    {
        k0_LoadIndexZero=0, 
        k1_LoadIndexOne,
        k2_LoadIndexTwo,
        k3_UnloadIndexZero,
        k4_UnloadIndexTwo,
        k5_UnloadIndexOne_And_LoadIndexTwo,
        k6_UnloadIndexTwo,
        k7_LoadIndexZero_And_LoadIndexOne,
        k8_UnloadIndexZero_And_UnloadIndexOne,
        k9_DoNothing,
        kNum
    } s_state;
    assert(streamingUnit0);
    assert(streamingUnit1);
    assert(streamingUnit2);
    assert(streamingUnitRuntimeQueueManager);

    VectorSafe<StreamingCommand, 2> streamingCommands;
    switch (s_state)
    {
    case UnitTestState::k0_LoadIndexZero:
        {
            streamingCommands.Push(StreamingCommand(StreamingCommand::kLoad, streamingUnit0));
            StreamingCommandsStart(&streamingCommands, streamingUnitRuntimeQueueManager, assetLoadingThreadWakeHandle);
            break;
        }
        case UnitTestState::k1_LoadIndexOne:
        {
            streamingCommands.Push(StreamingCommand(StreamingCommand::kLoad, streamingUnit1));
            StreamingCommandsStart(&streamingCommands, streamingUnitRuntimeQueueManager, assetLoadingThreadWakeHandle);
            break;
        }
        case UnitTestState::k2_LoadIndexTwo:
        {
            streamingCommands.Push(StreamingCommand(StreamingCommand::kLoad, streamingUnit2));
            StreamingCommandsStart(&streamingCommands, streamingUnitRuntimeQueueManager, assetLoadingThreadWakeHandle);
            break;
        }
        case UnitTestState::k3_UnloadIndexZero:
        {
            streamingUnit0->m_unloadAsSoonAsGpuIsDoneRendering = true;
            break;
        }
        case UnitTestState::k4_UnloadIndexTwo:
        {
            streamingUnit2->m_unloadAsSoonAsGpuIsDoneRendering = true;
            break;
        }
        case UnitTestState::k5_UnloadIndexOne_And_LoadIndexTwo:
        {
            streamingUnit1->m_unloadAsSoonAsGpuIsDoneRendering = true;
            streamingCommands.Push(StreamingCommand(StreamingCommand::kLoad, streamingUnit2));
            StreamingCommandsStart(&streamingCommands, streamingUnitRuntimeQueueManager, assetLoadingThreadWakeHandle);
            break;
        }
        case UnitTestState::k6_UnloadIndexTwo:
        {
            streamingUnit2->m_unloadAsSoonAsGpuIsDoneRendering = true;
            break;
        }
        case UnitTestState::k7_LoadIndexZero_And_LoadIndexOne:
        {
            streamingCommands.Push(StreamingCommand(StreamingCommand::kLoad, streamingUnit0));
            streamingCommands.Push(StreamingCommand(StreamingCommand::kLoad, streamingUnit1));
            StreamingCommandsStart(&streamingCommands, streamingUnitRuntimeQueueManager, assetLoadingThreadWakeHandle);
            break;
        }
        case UnitTestState::k8_UnloadIndexZero_And_UnloadIndexOne:
        {
            streamingUnit0->m_unloadAsSoonAsGpuIsDoneRendering = true;
            streamingUnit1->m_unloadAsSoonAsGpuIsDoneRendering = true;
            break;
        }
        case UnitTestState::k9_DoNothing:
        {
            break;
        }
        default:
        {
            assert(false);
            break;
        }
    }

    s_state = static_cast<UnitTestState>(static_cast<size_t>(s_state) + 1);
    if (s_state >= UnitTestState::kNum)
    {
        s_state = UnitTestState::k0_LoadIndexZero;
    }
}

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
        m_streamingCommandQueueManager.Destroy();

        m_assetLoadingThreadData.m_threadCommand = AssetLoadingArguments::ThreadCommand::kCleanupAndTerminate;
        SignalSemaphoreWindows(m_assetLoadingThreadData.m_handles.wakeEventHandle);
        WaitForSignalWindows(m_assetLoadingThreadData.m_handles.doneEventHandle);

        for (auto& streamingUnit : m_streamingUnits)
        {
            const StreamingUnitRuntime::State state = streamingUnit.StateMutexed();
            if (state == StreamingUnitRuntime::kReady || state == StreamingUnitRuntime::kUnloading)
            {
                streamingUnit.StateMutexed(StreamingUnitRuntime::kUnloading);//we are shutting down, and will not be issuing any more draw calls
                streamingUnit.Free(
                    &m_deviceLocalMemoryStreamingUnitsAllocated, 
                    m_deviceLocalMemoryStreamingUnits, 
                    m_deviceLocalMemoryMutex, 
                    true, 
                    m_device);
            }
            streamingUnit.Destroy(m_device);
        }

        CleanupSwapChain(
            &m_commandBuffersPrimary,
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

        HandleCloseWindows(&m_deviceLocalMemoryMutex);
        HandleCloseWindows(&m_graphicsQueueMutex);
#if NTF_ASSET_LOADING_MULTITHREADED
        HandleCloseWindows(&m_assetLoadingThreadData.m_handles.threadHandle);
#endif//#if NTF_ASSET_LOADING_MULTITHREADED
        HandleCloseWindows(&m_assetLoadingThreadData.m_handles.doneEventHandle);
        HandleCloseWindows(&m_assetLoadingThreadData.m_handles.wakeEventHandle);

        glfwTerminate();
    }

    void VulkanInitialize()
    {
        //QueryPerformanceFrequency(&g_queryPerformanceFrequency);

        s_validationLayers.size(0);
        s_validationLayers.Push("VK_LAYER_LUNARG_standard_validation");
#if NTF_API_DUMP_VALIDATION_LAYER_ON
        s_validationLayers.Push("VK_LAYER_LUNARG_api_dump");///<this produces "file not found" after outputting to (I believe) stdout for a short while; seems like it overruns Windows 7's file descriptor or something.  Weirdly, running from Visual Studio 2015 does not seem to have this problem, but then I'm limited to 9999 lines of the command prompt VS2015 uses for output.  Not ideal
#endif//NTF_API_DUMP_VALIDATION_LAYER_ON
        m_deviceExtensions = VectorSafe<const char*, NTF_DEVICE_EXTENSIONS_NUM>({VK_KHR_SWAPCHAIN_EXTENSION_NAME});

        m_instance = CreateInstance(s_validationLayers);
        m_callback = SetupDebugCallback(m_instance);
        CreateSurface(&m_surface, m_window, m_instance);//window surface needs to be created right before physical device creation, because it can actually influence the physical device selection: TODO: learn more about this influence
        PickPhysicalDevice(&m_physicalDevice, m_surface, m_deviceExtensions, m_instance);

        //if this is an Nvidia card, use diagnostic checkpoints in case of VK_DEVICE_LOST
        VectorSafe<const char*, 1> deviceDiagnosticCheckpoints({ "VK_NV_device_diagnostic_checkpoints" });
        if (CheckDeviceExtensionSupport(m_physicalDevice, deviceDiagnosticCheckpoints))
        {
            m_deviceExtensions.Push(deviceDiagnosticCheckpoints[0]);//device diagnostic checkpoints are supported, so use them
            g_deviceDiagnosticCheckpointsSupported = true;//global variable used to avoid passing another boolean to practically every Vulkan function
        }

        NTFVulkanInitialize(m_physicalDevice);
        m_queueFamilyIndices = FindQueueFamilies(m_physicalDevice, m_surface);
        m_graphicsQueueMutex = MutexCreate();
        m_deviceLocalMemoryMutex = MutexCreate();
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
        
        CreateCommandPool(&m_commandPoolPrimary, m_queueFamilyIndices.graphicsFamily, m_device, m_physicalDevice);
        CreateCommandPool(&m_commandPoolTransitionImage, m_queueFamilyIndices.graphicsFamily, m_device, m_physicalDevice);
        CreateCommandPool(&m_commandPoolTransfer, m_queueFamilyIndices.transferFamily, m_device, m_physicalDevice);

        m_deviceLocalMemoryPersistent.Initialize(m_device, m_physicalDevice);
        for (auto& vulkanPagedStackAllocator : m_deviceLocalMemoryStreamingUnits)
        {
            vulkanPagedStackAllocator.Initialize(m_device, m_physicalDevice);
        }
        m_deviceLocalMemoryStreamingUnitsAllocated.MemsetEntireArray(0);
         
        //#CommandPoolDuplication
        AllocateCommandBuffers(
            &ArraySafeRef<VkCommandBuffer>(&m_commandBufferTransfer, 1),
            m_commandPoolTransfer,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1,
            m_device);
        AllocateCommandBuffers(
			&ArraySafeRef<VkCommandBuffer>(&m_commandBufferTransitionImage, 1),
            m_commandPoolTransitionImage,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1,
            m_device);

        VkFence initializationDone;
        FenceCreate(&initializationDone, static_cast<VkFenceCreateFlagBits>(0), m_device);
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
            ConstVectorSafeRef<VkSemaphore>(),
            ConstVectorSafeRef<VkSemaphore>(),
            ArraySafeRef<VkPipelineStageFlags>(),
            m_commandBufferTransitionImage,
            m_graphicsQueue,
            nullptr,//no need to mutex, since currently only the main thread is running and we guard against launching the asset loading thread until this command buffer completes
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
            streamingUnit.m_uniformBufferSizeUnaligned = sizeof(UniformBufferObject)*NTF_DRAWS_PER_OBJECT_NUM*NTF_OBJECTS_NUM;///#StreamingMemory
        }
        m_streamingUnits[0].m_filenameNoExtension.Snprintf(g_streamingUnitName_UnitTest0);
        m_streamingUnits[1].m_filenameNoExtension.Snprintf(g_streamingUnitName_UnitTest1);
        m_streamingUnits[2].m_filenameNoExtension.Snprintf(g_streamingUnitName_UnitTest2);

        CreateFramebuffers(&m_swapChainFramebuffers, m_swapChainImageViews, m_renderPass, m_swapChainExtent, m_depthImageView, m_device);
        
        const uint32_t swapChainFramebuffersSize = Cast_size_t_uint32_t(m_swapChainFramebuffers.size());
        m_commandPoolsSecondary.size(swapChainFramebuffersSize);
        for (auto& commandPoolSecondaryArray : m_commandPoolsSecondary)
        {
            for (auto& commandPoolSecondary : commandPoolSecondaryArray)
            {
                CreateCommandPool(&commandPoolSecondary, m_queueFamilyIndices.graphicsFamily, m_device, m_physicalDevice);
            }
        }

        m_assetLoadingThreadData.m_handles.doneEventHandle = ThreadSignalingEventCreate();
        m_assetLoadingThreadData.m_handles.wakeEventHandle = ThreadSignalingEventCreate();
        
        m_assetLoadingThreadData.m_threadCommand = AssetLoadingArguments::ThreadCommand::kLoadStreamingUnit;

        m_assetLoadingArguments.m_deviceLocalMemoryPersistent = &m_deviceLocalMemoryPersistent;
        m_assetLoadingArguments.m_deviceLocalMemoryStreamingUnits = &m_deviceLocalMemoryStreamingUnits;
        m_assetLoadingArguments.m_deviceLocalMemoryStreamingUnitsAllocated = &m_deviceLocalMemoryStreamingUnitsAllocated;///<@todo: try to write an operator==() for ArraySafe/VectorSafe so you can assert on forgetting to set it

        m_assetLoadingArguments.m_commandBufferTransfer = &m_commandBufferTransfer;
        m_assetLoadingArguments.m_commandBufferTransitionImage = &m_commandBufferTransitionImage;
        m_assetLoadingArguments.m_device = &m_device;
        m_assetLoadingArguments.m_deviceLocalMemoryMutex = &m_deviceLocalMemoryMutex;
        m_assetLoadingArguments.m_graphicsQueue = &m_graphicsQueue;
        m_assetLoadingArguments.m_graphicsQueueMutex = &m_graphicsQueueMutex;
        m_assetLoadingArguments.m_instance = &m_instance;
        m_assetLoadingArguments.m_physicalDevice = &m_physicalDevice;
        m_assetLoadingArguments.m_queueFamilyIndices = &m_queueFamilyIndices;
        m_assetLoadingArguments.m_streamingCommandQueueManager = &m_streamingCommandQueueManager;
        m_assetLoadingArguments.m_threadCommand = &m_assetLoadingThreadData.m_threadCommand;
        m_assetLoadingArguments.m_threadDone = &m_assetLoadingThreadData.m_handles.doneEventHandle;
        m_assetLoadingArguments.m_threadWake = &m_assetLoadingThreadData.m_handles.wakeEventHandle;
        m_assetLoadingArguments.m_transferQueue = &m_transferQueue;

        m_assetLoadingArguments.m_renderPass = &m_renderPass;
        m_assetLoadingArguments.m_swapChainExtent = &m_swapChainExtent;
        
        m_assetLoadingArguments.AssertValid();

#if NTF_ASSET_LOADING_MULTITHREADED
        ///@todo: THREAD_MODE_BACKGROUND_BEGIN or THREAD_PRIORITY_BELOW_NORMAL and SetThreadPriority
        m_assetLoadingThreadData.m_handles.threadHandle = CreateThreadWindows(AssetLoadingThread, &m_assetLoadingArguments);
#else
        AssetLoadingThreadPersistentResourcesCreate(&m_assetLoadingPersistentResources, &m_deviceLocalMemoryPersistent, m_physicalDevice, m_device);
#endif//#if NTF_ASSET_LOADING_MULTITHREADED

		//BEG_#StreamingTest
        //load first streaming unit
        VectorSafe<StreamingCommand, 1> streamingCommands;
		streamingCommands.Push(StreamingCommand(StreamingCommand::kLoad, &m_streamingUnits[0]));
		StreamingCommandsStart(&streamingCommands, &m_streamingCommandQueueManager, m_assetLoadingThreadData.m_handles.wakeEventHandle);
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
    void MainLoop(GLFWwindow* window)
    {
        assert(window);

        while (!glfwWindowShouldClose(window)) 
        {
            //BEG_#StreamingTest
            //const StreamingUnitRuntime::FrameNumber frameToSwapState = 2400;
            //if (m_frameNumberCurrentCpu % frameToSwapState == frameToSwapState - 1)
            //{
            //    UnitTest(
            //        &m_streamingUnits[0], 
            //        &m_streamingUnits[1], 
            //        &m_streamingUnits[2],
            //        &m_streamingCommandQueueManager, 
            //        m_assetLoadingThreadData.m_handles.wakeEventHandle);
            //}
            //END_#StreamingTest

            glfwPollEvents();

            bool streamingUnitRendered = false;
            uint32_t acquiredImageIndex;
            VkSemaphore imageAvailableSemaphore;
            VkCommandBuffer commandBufferPrimary;
            VkFramebuffer swapChainFramebuffer;
            const size_t frameIndex = m_frameNumberCurrentCpu % NTF_FRAMES_IN_FLIGHT_NUM;
            
            //fill primary command buffers with loaded streaming units
            VectorSafe<StreamingCommand, 16> streamingCommandsToUnload;
            for (auto& streamingUnit : m_streamingUnits)///<@todo: make this a list of "renderable" streaming units rather than having ready/not-loaded states
            {
                if (!streamingUnit.m_unloadAsSoonAsGpuIsDoneRendering)
                {
                    if (streamingUnit.StateMutexed() == StreamingUnitRuntime::kReady)
                    {
                        if (!streamingUnitRendered)
                        {
                            streamingUnitRendered = true;
                            imageAvailableSemaphore = m_imageAvailableSemaphore[frameIndex];
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
                            clearValues[1].depthStencil = { 1.0f, 0 };

                            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
                            renderPassInfo.pClearValues = clearValues.data();

                            CmdSetCheckpointNV(commandBufferPrimary, &s_cmdSetCheckpointData[static_cast<size_t>(CmdSetCheckpointValues::vkCmdBeginRenderPass_kBefore)], m_instance);
                            vkCmdBeginRenderPass(commandBufferPrimary, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE/**<no secondary buffers will be executed; VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS = secondary command buffers will execute these commands*/);
                            CmdSetCheckpointNV(commandBufferPrimary, &s_cmdSetCheckpointData[static_cast<size_t>(CmdSetCheckpointValues::vkCmdBeginRenderPass_kAfter)], m_instance);
                        }

                        //printf("FillCommandBufferPrimary(%s)\n", streamingUnit.m_filenameNoExtension.data());//#LogStreaming

                        UpdateUniformBuffer(
                            streamingUnit.m_uniformBufferCpuMemory,
                            s_cameraTranslation,
                            streamingUnit.m_uniformBufferGpuMemory,
                            streamingUnit.m_uniformBufferOffsetToGpuMemory,
                            NTF_DRAW_CALLS_TOTAL,
                            streamingUnit.m_uniformBufferSizeAligned,
                            m_swapChainExtent,
                            m_device);

                        FillCommandBufferPrimary(
                            &streamingUnit.m_lastSubmittedCpuFrame,
                            &streamingUnit.m_renderedOnceSinceLastLoad,
                            m_frameNumberCurrentCpu,
                            commandBufferPrimary,
                            &streamingUnit.m_texturedGeometries,
                            streamingUnit.m_descriptorSet,
                            NTF_OBJECTS_NUM,
                            NTF_DRAWS_PER_OBJECT_NUM,
                            streamingUnit.m_pipelineLayout,
                            streamingUnit.m_graphicsPipeline,
                            m_instance);
                    }
                }//if (!streamingUnit.m_unloadAsSoonAsGpuIsDoneRendering)
                else//streaming unit should be unloaded as soon as the Gpu is done with it
                {
                    //LARGE_INTEGER perfCount;
                    //QueryPerformanceCounter(&perfCount);
                    //printf("MAIN THREAD: streamingUnit.m_lastSubmittedCpuFrame=%d m_lastCpuFrameCompleted=%d time=%f\n", streamingUnit.m_lastSubmittedCpuFrame, m_lastCpuFrameCompleted, static_cast<double>(perfCount.QuadPart)/ static_cast<double>(g_queryPerformanceFrequency.QuadPart));

                    //printf("MAIN THREAD: streamingUnit.m_lastSubmittedCpuFrame=%d m_lastCpuFrameCompleted=%d\n", streamingUnit.m_lastSubmittedCpuFrame, m_lastCpuFrameCompleted);

                    if (streamingUnit.StateMutexed() == StreamingUnitRuntime::kReady)//streaming unit must be loaded if it's to be unloaded
                    {
                        ///@todo: NTF_STATIC_ASSERT that streamingUnit.m_lastSubmittedCpuFrame is the same type as m_lastCpuFrameCompleted
                        const StreamingUnitRuntime::FrameNumber halfRange = 2 << (sizeof(m_lastCpuFrameCompleted) - 1);//one half of the range of an unsigned type
                        if (!streamingUnit.m_renderedOnceSinceLastLoad ||                                   //Gpu never rendered this streaming unit
                            (streamingUnit.m_lastSubmittedCpuFrame <= m_lastCpuFrameCompleted) ||           //Gpu is done with this streaming unit
                            (streamingUnit.m_lastSubmittedCpuFrame - m_lastCpuFrameCompleted < halfRange))  //Gpu is done with this streaming unit -- wraparound case where the last submitted cpu frame is near the end of the frame number range and the last cpu frame completed is near the beginning
                        {
                            //BEG_HAC -- ensure wraparound case triggers appropriately
                            if (streamingUnit.m_lastSubmittedCpuFrame - m_lastCpuFrameCompleted < halfRange)
                            {
                                int hack = 0;
                                ++hack;
                            }
                            //END_HAC
                            NTF_LOG_STREAMING("%i:streamingUnit=%p->m_lastSubmittedCpuFrame=%i<=m_lastCpuFrameCompleted=%i -- Push(kUnload)\n", GetCurrentThreadId(), &streamingUnit, streamingUnit.m_lastSubmittedCpuFrame, m_lastCpuFrameCompleted);
                            //printf("MAIN THREAD: m_streamingUnit.Free(); time=%f\n", static_cast<double>(perfCount.QuadPart)/ static_cast<double>(g_queryPerformanceFrequency.QuadPart));

                            /*  for multithreaded correctness, this should be the one and only one place where a streaming unit is marked "unloading" and
								the corresponding command is issued */
                            streamingUnit.m_unloadAsSoonAsGpuIsDoneRendering = false;//command about to be issued
                            streamingUnit.StateMutexed(StreamingUnitRuntime::kUnloading);
                            streamingCommandsToUnload.Push(StreamingCommand(StreamingCommand::kUnload, &streamingUnit));
                        }
                    }
                }
            }//for (auto& streamingUnit : m_streamingUnits)
            if (streamingCommandsToUnload.size())
            {
                StreamingCommandsStart(&streamingCommandsToUnload, &m_streamingCommandQueueManager, m_assetLoadingThreadData.m_handles.wakeEventHandle);
            }
            if(streamingUnitRendered)
            {
                CmdSetCheckpointNV(commandBufferPrimary, &s_cmdSetCheckpointData[static_cast<size_t>(CmdSetCheckpointValues::vkCmdEndRenderPass_kAfter)], m_instance);
                vkCmdEndRenderPass(commandBufferPrimary);
                CmdSetCheckpointNV(commandBufferPrimary, &s_cmdSetCheckpointData[static_cast<size_t>(CmdSetCheckpointValues::vkCmdEndRenderPass_kAfter)], m_instance);
                EndCommandBuffer(commandBufferPrimary);
#if NTF_WIN_TIMER
                WIN_TIMER_STOP(s_frameTimer);
                ArraySafe<char, 256> string;
                string.Snprintf("s_frameTimer:%fms\n", WIN_TIMER_ELAPSED_MILLISECONDS(s_frameTimer));
                string.Fwrite(s_winTimer, strlen(string.begin()));
                WIN_TIMER_START(s_frameTimer);
#endif//#if NTF_WIN_TIMER

                auto& drawFrameFinishedFence = m_drawFrameFinishedFences[frameIndex];
                WIN_TIMER_DEF_START(waitForFences);
                FenceWaitUntilSignalled(drawFrameFinishedFence.m_fence, m_device);
                WIN_TIMER_STOP(waitForFences);
                NTF_LOG_STREAMING("%i:FenceWaitUntilSignalled(drawFrameFinishedFence.m_fence=%zu)\n", GetCurrentThreadId(), (size_t)drawFrameFinishedFence.m_fence);
                //const int maxLen = 256;
                //char buf[maxLen];
                //snprintf(&buf[0], maxLen, "waitForFences:%fms\n", WIN_TIMER_ELAPSED_MILLISECONDS(waitForFences));
                //fwrite(&buf[0], sizeof(buf[0]), strlen(&buf[0]), s_winTimer);

                //determine last Cpu frame that the Gpu finished with -- needs to be done here, immediately before the fence from the most recently completed Gpu frame is reset (since this logic depends on seeing the signaled state of this fence)
                const StreamingUnitRuntime::FrameNumberSigned biggestPositiveDistanceInitial = 0;
                const StreamingUnitRuntime::FrameNumberSigned biggestNegativeDistanceInitial = StreamingUnitRuntime::kFrameNumberSignedMinimum;
                StreamingUnitRuntime::FrameNumberSigned biggestPositiveDistance = biggestPositiveDistanceInitial, biggestNegativeDistance = biggestNegativeDistanceInitial;//negative distance handles the wraparound case
                StreamingUnitRuntime::FrameNumber positiveDistanceFrameNumberCpuSubmitted = 0, negativeDistanceFrameNumberCpuSubmitted = 0;
                for (int i = 0; i < NTF_FRAMES_IN_FLIGHT_NUM; ++i)
                {
                    auto& drawFrameFinishedFence = m_drawFrameFinishedFences[i];
                    const StreamingUnitRuntime::FrameNumber frameNumberCpuSubmitted = drawFrameFinishedFence.m_frameNumberCpuSubmitted;

                    //printf("MAIN THREAD: m_drawFrameFinishedFences[%i]: m_frameNumberCpuSubmitted=%d -- drawFrameFinishedFence.m_frameNumberCpuRecordedCompleted=%i \n",
                    //    i, frameNumberCpuSubmitted, drawFrameFinishedFence.m_frameNumberCpuRecordedCompleted);

                    if (!drawFrameFinishedFence.m_frameNumberCpuRecordedCompleted &&
                        vkGetFenceStatus(m_device, drawFrameFinishedFence.m_fence) == VK_SUCCESS)//Gpu has reported a new frame completed since last Cpu checked
                    {
                        drawFrameFinishedFence.m_frameNumberCpuRecordedCompleted = true;//we are about to process this frame number as completed

                        const StreamingUnitRuntime::FrameNumberSigned distance = frameNumberCpuSubmitted - m_lastCpuFrameCompleted;
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
                    m_lastCpuFrameCompleted =   biggestNegativeDistance > StreamingUnitRuntime::kFrameNumberSignedMinimum ?
                                                negativeDistanceFrameNumberCpuSubmitted : positiveDistanceFrameNumberCpuSubmitted;

                    //printf("MAIN THREAD: streamingUnit.m_lastCpuFrameCompleted=%d\n", m_lastCpuFrameCompleted);
                }
                FenceReset(drawFrameFinishedFence.m_fence, m_device);//queue has completed on the GPU and is ready to be prepared on the CPU

                //theoretically the implementation can already start executing our vertex shader and such while the image is not
                //available yet. Each entry in the waitStages array corresponds to the semaphore with the same index in pWaitSemaphores
                VectorSafe<VkSemaphore, 4> signalSemaphores({ m_renderFinishedSemaphore[frameIndex] });
                VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                //BEG_HAC
                //printf( "vkGetFenceStatus(m_device, drawFrameFinishedFence.m_fence=%zx)=%i\n", 
                //        (size_t)drawFrameFinishedFence.m_fence, vkGetFenceStatus(m_device, drawFrameFinishedFence.m_fence));
                //END_HAC
                SubmitCommandBuffer(
                    signalSemaphores,
                    ConstVectorSafeRef<VkSemaphore>(&imageAvailableSemaphore, 1),
                    ArraySafeRef<VkPipelineStageFlags>(&waitStages, 1),///<@todo: ArraySafeRefConst
                    m_commandBuffersPrimary[acquiredImageIndex],
                    m_graphicsQueue,
                    &m_graphicsQueueMutex,
                    drawFrameFinishedFence.m_fence,
                    m_instance);

                drawFrameFinishedFence.m_frameNumberCpuSubmitted = m_frameNumberCurrentCpu;//so we know when it's safe to unload streaming unit's assets
                drawFrameFinishedFence.m_frameNumberCpuRecordedCompleted = false;//this frame has not been completed by the Gpu until this fence signals

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
                    WaitForSignalWindows(m_graphicsQueueMutex);
                }
                const VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
                NTF_LOG_STREAMING("%i:vkQueuePresentKHR(m_presentQueue=%zu,signalSemaphores.size()=%zu; &signalSemaphores[0]=%p)\n", 
                    GetCurrentThreadId(), (size_t)m_presentQueue, signalSemaphores.size(), &signalSemaphores[0]);
                if (unifiedGraphicsAndPresentQueue)
                {
                    ReleaseMutex(m_graphicsQueueMutex);
                }
                if (result == VK_ERROR_OUT_OF_DATE_KHR/*swap chain can no longer be used for rendering*/ ||
                    result == VK_SUBOPTIMAL_KHR/*swap chain can still present image, but surface properties don't entirely match; for example, during resizing*/)
                {
                    ///#TODO_CALLBACK
                    //hackToRecreateSwapChainIfNecessary.SwapChainRecreate();//haven't seen this get hit yet, even when minimizing and resizing the window
                }
                NTF_VK_ASSERT_SUCCESS(result);

            }
#if !NTF_ASSET_LOADING_MULTITHREADED
            StreamingCommandsProcess(&m_assetLoadingArguments, &m_assetLoadingPersistentResources);
#endif//!NTF_ASSET_LOADING_MULTITHREADED

            ++m_frameNumberCurrentCpu;
        }

#if !NTF_ASSET_LOADING_MULTITHREADED
        AssetLoadingPersistentResourcesDestroy(&m_assetLoadingPersistentResources, m_assetLoadingThreadData.m_handles.doneEventHandle, m_device);
#endif//!NTF_ASSET_LOADING_MULTITHREADED
        //wait for the logical device to finish operations before exiting MainLoop and destroying the window
        vkDeviceWaitIdle(m_device);
    }

    GLFWwindow* m_window;
    VkInstance m_instance;
    VkDebugReportCallbackEXT m_callback;
    VkSurfaceKHR m_surface;
    QueueFamilyIndices m_queueFamilyIndices;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;//doesn't need to be deleted, since physical devices can't be created or destroyed by software
    VkDevice m_device;//interface to the physical device; must be destroyed before the physical device
    VkSwapchainKHR m_swapChain;//must be destroyed before the logical device
    VkQueue m_graphicsQueue, m_presentQueue, m_transferQueue;//queues are implicitly cleaned up with the logical device; no need to delete
    HANDLE m_graphicsQueueMutex;//synchronizes one-and-only graphics queue between asset loading thread (which uses it to load textures) and main thread
    VectorSafe<const char*, NTF_DEVICE_EXTENSIONS_NUM> m_deviceExtensions;
    enum { kSwapChainImagesNumMax=8 };
    VectorSafe<VkImage, kSwapChainImagesNumMax> m_swapChainImages;//handles to images, which are created by the swapchain and will be destroyed by the swapchain.  Images are "multidimensional - up to 3 - arrays of data which can be used for various purposes (e.g. attachments, textures), by binding them to a graphics or compute pipeline via descriptor sets, or by directly specifying them as parameters to certain commands" -- https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VkImage.html
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;
    VectorSafe<VkImageView, kSwapChainImagesNumMax> m_swapChainImageViews;//defines type of image (eg color buffer with mipmaps, depth buffer, and so on)
    VectorSafe<VkFramebuffer, kSwapChainImagesNumMax> m_swapChainFramebuffers;
    VkRenderPass m_renderPass;
    VectorSafe<ArraySafe<VkCommandPool, NTF_OBJECTS_NUM>, kSwapChainImagesNumMax> m_commandPoolsSecondary;
    VkImage m_depthImage;
    VkImageView m_depthImageView;

    glm::vec3 m_cameraTranslation;

    VkCommandPool m_commandPoolPrimary, m_commandPoolTransfer, m_commandPoolTransitionImage;

    enum { kStreamingUnitsNum = 3 };
    VectorSafe<StreamingUnitRuntime, kStreamingUnitsNum> m_streamingUnits;
    StreamingCommandQueueManager m_streamingCommandQueueManager;
    VectorSafe<VkCommandBuffer, kSwapChainImagesNumMax> m_commandBuffersPrimary;//automatically freed when VkCommandPool is destroyed
        
    VkCommandBuffer m_commandBufferTransfer;//automatically freed when VkCommandPool is destroyed
    VkCommandBuffer m_commandBufferTransitionImage;//automatically freed when VkCommandPool is destroyed

    /*  fences are mainly designed to synchronize your application itself with rendering operation, whereas semaphores are 
        used to synchronize operations within or across command queues */
    int m_frameIndex=0;
    VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM> m_imageAvailableSemaphore = VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM>(NTF_FRAMES_IN_FLIGHT_NUM);///<@todo NTF: refactor use ArraySafe
    VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM> m_renderFinishedSemaphore = VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM>(NTF_FRAMES_IN_FLIGHT_NUM);///<@todo NTF: refactor use ArraySafe
    ArraySafe<DrawFrameFinishedFence, NTF_FRAMES_IN_FLIGHT_NUM> m_drawFrameFinishedFences;

    HANDLE m_deviceLocalMemoryMutex;
    VulkanPagedStackAllocator m_deviceLocalMemoryPersistent;
    VectorSafe<VulkanPagedStackAllocator, kStreamingUnitsNum> m_deviceLocalMemoryStreamingUnits = 
        VectorSafe<VulkanPagedStackAllocator, kStreamingUnitsNum>(kStreamingUnitsNum);
    ArraySafe<bool, kStreamingUnitsNum> m_deviceLocalMemoryStreamingUnitsAllocated;

    AssetLoadingThreadData m_assetLoadingThreadData;
    AssetLoadingArguments m_assetLoadingArguments;
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

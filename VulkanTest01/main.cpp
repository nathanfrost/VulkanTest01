//#Threading
#include <thread>
//#Threading
#include"ntf_vulkan.h"

VectorSafe<const char*, NTF_VALIDATION_LAYERS_SIZE> s_validationLayers;

//don't complain about scanf being unsafe
#pragma warning(disable : 4996)
///@todo: figure out which libraries I'm linking that trigger LNK4098 (seems like some libraries are linking /MD and /MDd and others are linking /MT and /MTd for C-runtime) -- for now, pass /IGNORE:4098 to the linker

//#Threading
struct CommandBufferThreadArguments
{
    VkCommandBuffer* commandBuffer;
    VkDescriptorSet* descriptorSet;
    VkRenderPass* renderPass;
    VkExtent2D* swapChainExtent;
    VkPipelineLayout* pipelineLayout;
    VkBuffer* vertexBuffer;
    VkBuffer* indexBuffer;
    VkFramebuffer* swapChainFramebuffer;
    VkPipeline* graphicsPipeline;
    uint32_t* objectIndex;
    uint32_t* indicesNum;
    HANDLE* commandBufferThreadDone;
    HANDLE* commandBufferThreadWake;
};

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
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
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
//#Threading


class VulkanRendererNTF 
{
public:
#define NTF_FRAMES_IN_FLIGHT_NUM 2//#FramesInFlight
#define NTF_OBJECTS_NUM 2//number of models to draw

    void run() 
	{
        initWindow(&m_window);
        initVulkan();

        //#Threading
        const unsigned int threadsHardwareNum = std::thread::hardware_concurrency();
        assert(threadsHardwareNum > 0);
        //BEG_THREADING_HACK
        ///@todo: cleanly handle any number of nonzero threads
        //const unsigned int commandBufferThreadsNum = min(min(threadsHardwareNum, NTF_OBJECTS_NUM), kSwapChainImagesNumMax);
        const size_t threadsNum = NTF_OBJECTS_NUM;
        assert(threadsHardwareNum >= threadsNum);
        //END_THREADING_HACK
        
        for (size_t threadIndex = 0; threadIndex < threadsNum; ++threadIndex)
        {
            auto& threadHandle = m_commandBufferThreadHandles[threadIndex];
            auto& commandBufferThreadArguments = m_commandBufferThreadArguments[threadIndex];

            auto& commandBufferThreadWake = m_commandBufferThreadWake[threadIndex];
            commandBufferThreadWake = CreateEvent(///<@todo NTF: wrap this Windows function and unduplicate below
                NULL,               // default security attributes
                FALSE,              // auto-reset; after signaling immediately set to nonsignaled
                FALSE,              // initial state is nonsignaled
                NULL                // no name -- if you have two events with the same name, the more recent one stomps the less recent one
                );
            commandBufferThreadArguments.commandBufferThreadWake = &commandBufferThreadWake;

            auto& commandBufferThreadDone = m_commandBufferThreadsDone[threadIndex];
            commandBufferThreadDone = CreateEvent(
                NULL,               // default security attributes
                FALSE,              // auto-reset; after signaling immediately set to nonsignaled
                FALSE,              // initial state is nonsignaled
                NULL                // no name -- if you have two events with the same name, the more recent one stomps the less recent one
                );
            commandBufferThreadArguments.commandBufferThreadDone = &commandBufferThreadDone;

            threadHandle = CreateThread(
                nullptr,                                        //child processes irrelevant; no suspending or resuming privileges
                0,                                              //default stack size
                CommandBufferThread,                            //starting address to execute
                &commandBufferThreadArguments,                  //argument
                0,                                              //run immediately; "commit" (eg map) stack memory for immediate use
                nullptr);                                       //ignore thread id
            assert(threadHandle);///@todo: investigate SetThreadPriority() if default priority (THREAD_PRIORITY_NORMAL) seems inefficient
        }
        ///@todo: CloseHandle() cleanup
        //#Threading

        mainLoop(m_window);
        cleanup();

        int i;
        printf("Enter a character and press ENTER to exit\n");
        scanf("%i", &i);
    }

    void recreateSwapChain()
    {
        vkDeviceWaitIdle(m_device);

        CleanupSwapChain(
            &m_commandBuffersPrimary,
            &m_commandBuffersSecondary,
            m_device,
            m_depthImageView,
            m_depthImage,
            m_depthImageMemory,
            m_swapChainFramebuffers,
            m_commandPoolPrimary,
            m_commandPoolsSecondary,
            m_graphicsPipeline,
            m_pipelineLayout,
            m_renderPass,
            m_swapChainImageViews,
            m_swapChain);

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
        CreateGraphicsPipeline(&m_pipelineLayout, &m_graphicsPipeline, m_renderPass, m_descriptorSetLayout, m_swapChainExtent, m_device);
        CreateDepthResources(
            &m_depthImage,
            &m_depthImageMemory,
            &m_depthImageView,
            m_swapChainExtent,
            m_commandPoolPrimary,
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
    VkDeviceSize UniformBufferSizeCalculate()
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
        CleanupSwapChain(
            &m_commandBuffersPrimary,
            &m_commandBuffersSecondary,
            m_device,
            m_depthImageView,
            m_depthImage,
            m_depthImageMemory,
            m_swapChainFramebuffers,
            m_commandPoolPrimary,
            m_commandPoolsSecondary,
            m_graphicsPipeline,
            m_pipelineLayout,
            m_renderPass,
            m_swapChainImageViews,
            m_swapChain);
        
        vkDestroySampler(m_device, m_textureSampler, nullptr);
        vkDestroyImageView(m_device, m_textureImageView, nullptr);

        vkDestroyImage(m_device, m_textureImage, nullptr);
        vkFreeMemory(m_device, m_textureImageMemory, nullptr);

        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        
        DestroyUniformBuffer(m_uniformBufferCpuMemory, m_uniformBufferGpuMemory, m_uniformBuffer, m_device);

        vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
        vkFreeMemory(m_device, m_indexBufferMemory, nullptr);

        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);


        for (size_t frameIndex = 0; frameIndex < NTF_FRAMES_IN_FLIGHT_NUM; ++frameIndex)
        {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphore[frameIndex], nullptr);
            vkDestroySemaphore(m_device, m_imageAvailableSemaphore[frameIndex], nullptr);
            vkDestroyFence(m_device, m_fence[frameIndex], nullptr);
        }

        vkDestroyCommandPool(m_device, m_commandPoolPrimary, nullptr);
        for (auto& commandPoolSecondaryArray : m_commandPoolsSecondary)
        {
            for (auto& commandPoolSecondary : commandPoolSecondaryArray)
            {
                vkDestroyCommandPool(m_device, commandPoolSecondary, nullptr);
            }
        }

        vkDestroyDevice(m_device, nullptr);
        DestroyDebugReportCallbackEXT(m_instance, m_callback, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance, nullptr);

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

        m_instance = CreateInstance(s_validationLayers);
        m_callback = SetupDebugCallback(m_instance);
        CreateSurface(&m_surface, m_window, m_instance);//window surface needs to be created right before physical device creation, because it can actually influence the physical device selection: TODO: learn more about this influence
        PickPhysicalDevice(&m_physicalDevice, m_surface, m_deviceExtensions, m_instance);
        CreateLogicalDevice(&m_device, &m_graphicsQueue, &m_presentQueue, m_deviceExtensions, s_validationLayers, m_surface, m_physicalDevice);
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
        CreateGraphicsPipeline(&m_pipelineLayout, &m_graphicsPipeline, m_renderPass, m_descriptorSetLayout, m_swapChainExtent, m_device);
        CreateCommandPool(&m_commandPoolPrimary, m_surface, m_device, m_physicalDevice);
        CreateDepthResources(
            &m_depthImage, 
            &m_depthImageMemory, 
            &m_depthImageView, 
            m_swapChainExtent, 
            m_commandPoolPrimary, 
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
                CreateCommandPool(&commandPoolSecondary, m_surface, m_device, m_physicalDevice);
            }
        }

        CreateTextureImage(&m_textureImage, &m_textureImageMemory, m_commandPoolPrimary, m_graphicsQueue, m_device, m_physicalDevice);
        CreateTextureImageView(&m_textureImageView, m_textureImage, m_device);
        CreateTextureSampler(&m_textureSampler, m_device);
        LoadModel(&m_vertices, &m_indices);
        m_indicesSize = Cast_size_t_uint32_t(m_indices.size());//store since we need secondary buffers to point to this
        CreateVertexBuffer(&m_vertexBuffer, &m_vertexBufferMemory, m_vertices, m_commandPoolPrimary, m_graphicsQueue, m_device, m_physicalDevice);
        CreateIndexBuffer(&m_indexBuffer, &m_indexBufferMemory, m_indices, m_commandPoolPrimary, m_graphicsQueue, m_device, m_physicalDevice);
        
        m_uniformBufferCpuAlignment = UniformBufferCpuAlignmentCalculate(sm_uniformBufferElementSize, m_physicalDevice);
        CreateUniformBuffer(
            &m_uniformBufferCpuMemory,
            &m_uniformBufferGpuMemory,
            &m_uniformBuffer, 
            UniformBufferSizeCalculate(),
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
        CreateFrameSyncPrimitives(&m_imageAvailableSemaphore, &m_renderFinishedSemaphore, &m_fence, NTF_FRAMES_IN_FLIGHT_NUM, m_device);
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
                NTF_OBJECTS_NUM,
                UniformBufferSizeCalculate(), 
                m_swapChainExtent, 
                m_device);

            const VkSemaphore imageAvailableSemaphore = m_imageAvailableSemaphore[frameIndex];
            uint32_t acquiredImageIndex;
            AcquireNextImage(&acquiredImageIndex, m_swapChain, imageAvailableSemaphore, m_device);

            //#Threading
            const size_t threadNum = NTF_OBJECTS_NUM;
            for (size_t threadIndex = 0; threadIndex < threadNum; ++threadIndex)
            {
                auto& commandBufferThreadArguments = m_commandBufferThreadArguments[threadIndex];
                commandBufferThreadArguments.commandBuffer = &m_commandBuffersSecondary[acquiredImageIndex][threadIndex];
                commandBufferThreadArguments.descriptorSet = &m_descriptorSet;
                commandBufferThreadArguments.graphicsPipeline = &m_graphicsPipeline;
                commandBufferThreadArguments.indexBuffer = &m_indexBuffer;
                commandBufferThreadArguments.indicesNum = &m_indicesSize;

                m_threadIndex[threadIndex] = Cast_size_t_uint32_t(threadIndex);
                commandBufferThreadArguments.objectIndex = &m_threadIndex[threadIndex];

                commandBufferThreadArguments.pipelineLayout = &m_pipelineLayout;
                commandBufferThreadArguments.renderPass = &m_renderPass;
                commandBufferThreadArguments.swapChainExtent = &m_swapChainExtent;
                commandBufferThreadArguments.swapChainFramebuffer = &m_swapChainFramebuffers[acquiredImageIndex];
                commandBufferThreadArguments.vertexBuffer = &m_vertexBuffer;

                //#Wait
                //WakeByAddressSingle(commandBufferThreadArguments.signalMemory);//#SynchronizationWindows8+Only
                const BOOL setEventResult = SetEvent(m_commandBufferThreadWake[threadIndex]);
                assert(setEventResult);
            }
            WaitForMultipleObjects(threadNum, m_commandBufferThreadsDone.begin(), TRUE, INFINITE);
            //#Threading

            FillCommandBuffer(
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
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;//doesn't need to be deleted, since physical devices can't be created or destroyed by software
    VkDevice m_device;//interface to the physical device; must be destroyed before the physical device
    VkSwapchainKHR m_swapChain;//must be destroyed before the logical device
    VkQueue m_graphicsQueue;//queues are implicitly cleaned up with the logical device; no need to delete
    VkQueue m_presentQueue;//queues are implicitly cleaned up with the logical device; no need to delete
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
    VkCommandPool m_commandPoolPrimary;
    VectorSafe<ArraySafe<VkCommandPool, NTF_OBJECTS_NUM>, kSwapChainImagesNumMax> m_commandPoolsSecondary;
    VkImage m_depthImage;
    VkDeviceMemory m_depthImageMemory;
    VkImageView m_depthImageView;
    VkImage m_textureImage;
    VkDeviceMemory m_textureImageMemory;
    VkImageView m_textureImageView;
    VkSampler m_textureSampler;
    std::vector<Vertex> m_vertices;///<@todo: streaming memory management
    std::vector<uint32_t> m_indices;///<@todo: streaming memory management
    uint32_t m_indicesSize;///<@todo: streaming memory management
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexBufferMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexBufferMemory;
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformBufferGpuMemory;
    ArraySafeRef<uint8_t> m_uniformBufferCpuMemory;
    VkDeviceSize m_uniformBufferCpuAlignment;
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSet m_descriptorSet;//automatically freed when the VkDescriptorPool is destroyed
    VectorSafe<VkCommandBuffer, kSwapChainImagesNumMax> m_commandBuffersPrimary;//automatically freed when VkCommandPool is destroyed
    VectorSafe<ArraySafe<VkCommandBuffer, NTF_OBJECTS_NUM>, kSwapChainImagesNumMax> m_commandBuffersSecondary;//automatically freed when VkCommandPool is destroyed ///@todo: "cannot convert argument 2 from 'ArraySafe<VectorSafe<VkCommandBuffer,8>,2>' to 'ArraySafeRef<VectorSafeRef<VkCommandBuffer>>" -- even when provided with ArraySafeRef(VectorSafe<T, kSizeMax>& vectorSafe) and VectorSafeRef(VectorSafe<T, kSizeMax>& vectorSafe) -- not sure why

    //#Threading
    ///@todo: only figure out how many secondary buffer threads could be active at max, only make that many threads and command pools
    ///@todo: collapse SoA into AoS
    ArraySafe<HANDLE, NTF_OBJECTS_NUM> m_commandBufferThreadsDone;
    ArraySafe<HANDLE, NTF_OBJECTS_NUM> m_commandBufferThreadHandles;
    ArraySafe<CommandBufferThreadArguments, NTF_OBJECTS_NUM> m_commandBufferThreadArguments;
    ArraySafe<HANDLE, NTF_OBJECTS_NUM> m_commandBufferThreadWake;
    ArraySafe<uint32_t, NTF_OBJECTS_NUM> m_threadIndex;
    //#Threading

    /*  fences are mainly designed to synchronize your application itself with rendering operation, whereas semaphores are 
        used to synchronize operations within or across command queues */
    int m_frameIndex=0;
    VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM> m_imageAvailableSemaphore = VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM>(NTF_FRAMES_IN_FLIGHT_NUM);///<@todo NTF: refactor so this is a ArraySafe (eg that doesn't have a m_sizeCurrentSet) rather than the current incarnation of this class, which is more like a VectorSafe
    VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM> m_renderFinishedSemaphore = VectorSafe<VkSemaphore, NTF_FRAMES_IN_FLIGHT_NUM>(NTF_FRAMES_IN_FLIGHT_NUM);///<@todo NTF: refactor so this is a ArraySafe (eg that doesn't have a m_sizeCurrentSet) rather than the current incarnation of this class, which is more like a VectorSafe
    VectorSafe<VkFence, NTF_FRAMES_IN_FLIGHT_NUM> m_fence = VectorSafe<VkFence, NTF_FRAMES_IN_FLIGHT_NUM>(NTF_FRAMES_IN_FLIGHT_NUM);///<@todo NTF: refactor so this is a true ArraySafe (eg that doesn't have a m_sizeCurrentSet) rather than the current incarnation of this class, which is more like a VectorSafe
};

int main() 
{
    VulkanRendererNTF app;
    app.run();
    return EXIT_SUCCESS;
}

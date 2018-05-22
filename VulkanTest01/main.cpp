#include"ntf_vulkan.h"


VectorSafe<const char*, NTF_VALIDATION_LAYERS_SIZE> s_validationLayers;

//don't complain about scanf being unsafe
#pragma warning(disable : 4996)
///@todo: figure out which libraries I'm linking that trigger LNK4098 (seems like some libraries are linking /MD and /MDd and others are linking /MT and /MTd for C-runtime) -- for now, pass /IGNORE:4098 to the linker


class VulkanRendererNTF 
{
public:
#define NTF_FRAMES_IN_FLIGHT_NUM 2//#FramesInFlight

    void run() 
	{
        initWindow(&m_window);
        initVulkan();
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
            &m_commandBuffers,
            m_device,
            m_depthImageView,
            m_depthImage,
            m_depthImageMemory,
            m_swapChainFramebuffers,
            m_commandPool,
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
            m_commandPool,
            m_graphicsQueue,
            m_device,
            m_physicalDevice);
        CreateFramebuffers(&m_swapChainFramebuffers, m_swapChainImageViews, m_renderPass, m_swapChainExtent, m_depthImageView, m_device);
        AllocateCommandBuffers(
            &m_commandBuffers,
            m_commandPool,
            static_cast<uint32_t>(m_swapChainFramebuffers.size()),
            m_device);
    }

private:
    VkDeviceSize UniformBufferSizeCalculate()
    {
        return sm_objectNum*m_uniformBufferCpuAlignment;
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
            &m_commandBuffers,
            m_device,
            m_depthImageView,
            m_depthImage,
            m_depthImageMemory,
            m_swapChainFramebuffers,
            m_commandPool,
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

        vkDestroyCommandPool(m_device, m_commandPool, nullptr);

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
        
        const VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        CreateDescriptorSetLayout(&m_descriptorSetLayout, descriptorType, m_device);
        CreateGraphicsPipeline(&m_pipelineLayout, &m_graphicsPipeline, m_renderPass, m_descriptorSetLayout, m_swapChainExtent, m_device);
        CreateCommandPool(&m_commandPool, m_surface, m_device, m_physicalDevice);
        CreateDepthResources(
            &m_depthImage, 
            &m_depthImageMemory, 
            &m_depthImageView, 
            m_swapChainExtent, 
            m_commandPool, 
            m_graphicsQueue, 
            m_device,
            m_physicalDevice);
        CreateFramebuffers(&m_swapChainFramebuffers, m_swapChainImageViews, m_renderPass, m_swapChainExtent, m_depthImageView, m_device);
        CreateTextureImage(&m_textureImage, &m_textureImageMemory, m_commandPool, m_graphicsQueue, m_device, m_physicalDevice);
        CreateTextureImageView(&m_textureImageView, m_textureImage, m_device);
        CreateTextureSampler(&m_textureSampler, m_device);
        LoadModel(&m_vertices, &m_indices);
        CreateVertexBuffer(&m_vertexBuffer, &m_vertexBufferMemory, m_vertices, m_commandPool, m_graphicsQueue, m_device, m_physicalDevice);
        CreateIndexBuffer(&m_indexBuffer, &m_indexBufferMemory, m_indices, m_commandPool, m_graphicsQueue, m_device, m_physicalDevice);
        
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
        AllocateCommandBuffers(
            &m_commandBuffers,
            m_commandPool,
            static_cast<uint32_t>(m_swapChainFramebuffers.size()),
            m_device);
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
                m_uniformBufferCpuAlignment, 
                m_uniformBufferGpuMemory, 
                sm_objectNum,
                UniformBufferSizeCalculate(), 
                m_swapChainExtent, 
                m_device);
            
            const VkSemaphore imageAvailableSemaphore = m_imageAvailableSemaphore[frameIndex];
            uint32_t acquiredImageIndex;
            AcquireNextImage(&acquiredImageIndex, m_swapChain, imageAvailableSemaphore, m_device);

            FillCommandBuffer(
                m_commandBuffers[acquiredImageIndex],
                m_descriptorSet,
                m_uniformBufferCpuAlignment,
                sm_objectNum,
                m_swapChainFramebuffers[acquiredImageIndex],
                m_renderPass,
                m_swapChainExtent,
                m_pipelineLayout,
                m_graphicsPipeline,
                m_vertexBuffer,
                m_indexBuffer,
                static_cast<uint32_t>(m_indices.size()),
                m_device);
            DrawFrame(
                /*this,///#TODO_CALLBACK*/ 
                m_swapChain, 
                m_commandBuffers, 
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


    const size_t sm_objectNum = 2;//number of models to draw
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
    VkCommandPool m_commandPool;
    VkImage m_depthImage;
    VkDeviceMemory m_depthImageMemory;
    VkImageView m_depthImageView;
    VkImage m_textureImage;
    VkDeviceMemory m_textureImageMemory;
    VkImageView m_textureImageView;
    VkSampler m_textureSampler;
    std::vector<Vertex> m_vertices;///<@todo: streaming memory management
    std::vector<uint32_t> m_indices;///<@todo: streaming memory management
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
    VectorSafe<VkCommandBuffer, kSwapChainImagesNumMax> m_commandBuffers;//automatically freed when VkCommandPool is destroyed

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


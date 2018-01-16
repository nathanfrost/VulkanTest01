#include"ntf_vulkan.h"


ArrayFixed<const char*, NTF_VALIDATION_LAYERS_SIZE> s_validationLayers;

//don't complain about scanf being unsafe
#pragma warning(disable : 4996)
///@todo: figure out which libraries I'm linking that trigger LNK4098 (seems like some libraries are linking /MD and /MDd and others are linking /MT and /MTd for C-runtime) -- for now, pass /IGNORE:4098 to the linker


class HelloTriangleApplication 
{
public:

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

        cleanupSwapChain(
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

        createSwapChain(m_window, &m_swapChain, &m_swapChainImages, &m_swapChainImageFormat, &m_swapChainExtent, m_physicalDevice, m_surface, m_device);
        createImageViews(&m_swapChainImageViews, m_swapChainImages, m_swapChainImageFormat, m_device);
        createRenderPass(&m_renderPass, m_swapChainImageFormat, m_device, m_physicalDevice);
        createGraphicsPipeline(&m_pipelineLayout, &m_graphicsPipeline, m_renderPass, m_descriptorSetLayout, m_swapChainExtent, m_device);
        createDepthResources(
            &m_depthImage,
            &m_depthImageMemory,
            &m_depthImageView,
            m_swapChainExtent,
            m_commandPool,
            m_graphicsQueue,
            m_device,
            m_physicalDevice);
        createFramebuffers(&m_swapChainFramebuffers, m_swapChainImageViews, m_renderPass, m_swapChainExtent, m_depthImageView, m_device);
        createCommandBuffers(
            &m_commandBuffers,
            m_commandPool,
            m_descriptorSet,
            m_swapChainFramebuffers,
            m_renderPass,
            m_swapChainExtent,
            m_pipelineLayout,
            m_graphicsPipeline,
            m_vertexBuffer,
            m_indexBuffer,
            static_cast<uint32_t>(m_indices.size()),
            m_device);
    }

private:
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
        glfwSetWindowSizeCallback(windowPtr, HelloTriangleApplication::onWindowResized);
    }

    /*  Viewport and scissor rectangle size is specified during graphics pipeline creation, so the pipeline also needs to be rebuilt when the window 
        is resized. It is possible to avoid this by using dynamic state for the viewports and scissor rectangles */
    static void onWindowResized(GLFWwindow* window, const int width, const int height)
    {
        if (width == 0 || height == 0) 
		{
			return;//handle the case where the window was minimized
		}

        HelloTriangleApplication* app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->recreateSwapChain();
    }

    void cleanup()
    {
        cleanupSwapChain(
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
        vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
        vkFreeMemory(m_device, m_uniformBufferMemory, nullptr);

        vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
        vkFreeMemory(m_device, m_indexBufferMemory, nullptr);

        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);

        vkDestroySemaphore(m_device, m_renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr);

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

        m_deviceExtensions = ArrayFixed<const char*, NTF_DEVICE_EXTENSIONS_NUM>({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });

        m_instance = createInstance(s_validationLayers);
        m_callback = setupDebugCallback(m_instance);
        createSurface(&m_surface, m_window, m_instance);//window surface needs to be created right before physical device creation, because it can actually influence the physical device selection: TODO: learn more about this influence
        pickPhysicalDevice(&m_physicalDevice, m_surface, m_deviceExtensions, m_instance);
        createLogicalDevice(&m_device, &m_graphicsQueue, &m_presentQueue, m_deviceExtensions, s_validationLayers, m_surface, m_physicalDevice);
        createSwapChain(m_window, &m_swapChain, &m_swapChainImages, &m_swapChainImageFormat, &m_swapChainExtent, m_physicalDevice, m_surface, m_device);
        createImageViews(&m_swapChainImageViews, m_swapChainImages, m_swapChainImageFormat, m_device);
        createRenderPass(&m_renderPass, m_swapChainImageFormat, m_device, m_physicalDevice);
        createDescriptorSetLayout(&m_descriptorSetLayout, m_device);
        createGraphicsPipeline(&m_pipelineLayout, &m_graphicsPipeline, m_renderPass, m_descriptorSetLayout, m_swapChainExtent, m_device);
        createCommandPool(&m_commandPool, m_surface, m_device, m_physicalDevice);
        createDepthResources(
            &m_depthImage, 
            &m_depthImageMemory, 
            &m_depthImageView, 
            m_swapChainExtent, 
            m_commandPool, 
            m_graphicsQueue, 
            m_device,
            m_physicalDevice);
        createFramebuffers(&m_swapChainFramebuffers, m_swapChainImageViews, m_renderPass, m_swapChainExtent, m_depthImageView, m_device);
        createTextureImage(&m_textureImage, &m_textureImageMemory, m_commandPool, m_graphicsQueue, m_device, m_physicalDevice);
        createTextureImageView(&m_textureImageView, m_textureImage, m_device);
        createTextureSampler(&m_textureSampler, m_device);
        loadModel(&m_vertices, &m_indices);
        createVertexBuffer(&m_vertexBuffer, &m_vertexBufferMemory, m_vertices, m_commandPool, m_graphicsQueue, m_device, m_physicalDevice);
        createIndexBuffer(&m_indexBuffer, &m_indexBufferMemory, m_indices, m_commandPool, m_graphicsQueue, m_device, m_physicalDevice);
        createUniformBuffer(&m_uniformBuffer, &m_uniformBufferMemory, m_device, m_physicalDevice);
        createDescriptorPool(&m_descriptorPool, m_device);
        createDescriptorSet(&m_descriptorSet, m_descriptorSetLayout, m_descriptorPool, m_uniformBuffer, m_textureImageView, m_textureSampler, m_device);
        createCommandBuffers(
            &m_commandBuffers, 
            m_commandPool,
            m_descriptorSet, 
            m_swapChainFramebuffers, 
            m_renderPass, 
            m_swapChainExtent, 
            m_pipelineLayout,
            m_graphicsPipeline, 
            m_vertexBuffer, 
            m_indexBuffer, 
            static_cast<uint32_t>(m_indices.size()),
            m_device);
        createSemaphores(&m_imageAvailableSemaphore, &m_renderFinishedSemaphore, m_device);
    }

    void mainLoop(GLFWwindow* window) 
    {
        assert(window);
        while (!glfwWindowShouldClose(window)) 
        {
            glfwPollEvents();

            updateUniformBuffer(m_uniformBufferMemory, m_swapChainExtent, m_device);
            drawFrame(/*this,///#TODO_CALLBACK*/ m_swapChain, m_commandBuffers, m_graphicsQueue, m_presentQueue, m_imageAvailableSemaphore, m_renderFinishedSemaphore, m_device);
        }

        //wait for the logical device to finish operations before exiting mainLoop and destroying the window
        vkDeviceWaitIdle(m_device);
    }


    GLFWwindow* m_window;
    VkInstance m_instance;
    VkDebugReportCallbackEXT m_callback;
    VkSurfaceKHR m_surface;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;//doesn't need to be deleted, since physical devices can't be created or destroyed by software
    VkDevice m_device;//interface to the physical device; must be destroyed before the physical device
    VkSwapchainKHR m_swapChain;//must be destroyed before the logical device
    VkQueue m_graphicsQueue;//queues are implicitly cleaned up with the logical device; no need to delete
    VkQueue m_presentQueue;//queues are implicitly cleaned up with the logical device; no need to delete
    ArrayFixed<const char*, NTF_DEVICE_EXTENSIONS_NUM> m_deviceExtensions;
    enum { kSwapChainImagesNumMax=8 };
    ArrayFixed<VkImage, kSwapChainImagesNumMax> m_swapChainImages;//handles to images, which are created by the swapchain and will be destroyed by the swapchain.  Images are "multidimensional - up to 3 - arrays of data which can be used for various purposes (e.g. attachments, textures), by binding them to a graphics or compute pipeline via descriptor sets, or by directly specifying them as parameters to certain commands" -- https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VkImage.html
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;
    ArrayFixed<VkImageView, kSwapChainImagesNumMax> m_swapChainImageViews;//defines type of image (eg color buffer with mipmaps, depth buffer, and so on)
    ArrayFixed<VkFramebuffer, kSwapChainImagesNumMax> m_swapChainFramebuffers;
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
    VkDeviceMemory m_uniformBufferMemory;
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSet m_descriptorSet;//automatically freed when the VkDescriptorPool is destroyed
    ArrayFixed<VkCommandBuffer, kSwapChainImagesNumMax> m_commandBuffers;//automatically freed when VkCommandPool is destroyed

    /*  fences are mainly designed to synchronize your application itself with rendering operation, whereas semaphores are 
        used to synchronize operations within or across command queues */
    VkSemaphore m_imageAvailableSemaphore;
    VkSemaphore m_renderFinishedSemaphore;
};

int main() 
{
    HelloTriangleApplication app;
    app.run();
    return EXIT_SUCCESS;
}


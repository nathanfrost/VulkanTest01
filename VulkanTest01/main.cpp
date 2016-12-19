//originally from https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Base_code
//TODO: Next:
//https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include<iostream>
#include<fstream>
#include<stdexcept>
#include<functional>
#include<vector>
#include<assert.h>
#include<set>
#include<algorithm>

#include"VDeleter.h"


static std::vector<char> readFile(const std::string& filename) 
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) 
    {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char* layerPrefix,
    const char* msg,
    void* userData) {

    std::cerr << "validation layer: " << msg << std::endl;

    return VK_FALSE;
}


VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) 
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

class HelloTriangleApplication {
public:

    void run() {
        initWindow();
        initVulkan();
        mainLoop();
    }

private:
    const uint32_t kWidth = 800; 
    const uint32_t kHeight = 600;

    const std::vector<const char*> m_validationLayers = 
    {
        "VK_LAYER_LUNARG_standard_validation"
    };

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    void initWindow() 
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API/**<hard constrain API...*/, GLFW_NO_API/**<...to Vulkan, which does not use an API*/);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        /*
        takes values set by glfwWindowHint()
        width and height may vary as they're soft constraints
        */
        m_window = glfwCreateWindow(
            kWidth,
            kHeight,
            "Vulkan window",
            nullptr/*windowed mode, not full-screen monitor*/,
            nullptr/*no sharing objects with another window; that's OpenGL, not Vulkan anyway*/);
    }

    std::vector<const char*> getRequiredExtensions() 
    {
        std::vector<const char*> extensions;

        unsigned int glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        for (unsigned int i = 0; i < glfwExtensionCount; i++) 
        {
            extensions.push_back(glfwExtensions[i]);
        }

        if (enableValidationLayers) 
        {
            extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);//VulkanSDK\VERSION_NUMBER\Config\vk_layer_settings.txt sets many options about layer strictness (warning,performance,error) and action taken (callback, log, breakpoint, Visual Studio output, nothing), as well as dump behavior (level of detail, output to file vs stdout, I/O flush behavior)
        }

        return extensions;
    }

    void createInstance()
    {
        if (enableValidationLayers && !checkValidationLayerSupport()) 
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = extensions.size();
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = m_validationLayers.size();
            createInfo.ppEnabledLayerNames = m_validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateInstance(&createInfo, nullptr, m_instance.replace()) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create instance!");
        }
    }

    bool checkValidationLayerSupport()
    {
        uint32_t layerCount;
        {
            const VkResult enumerateInstanceLayerPropertiesResult = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            assert(enumerateInstanceLayerPropertiesResult == VK_SUCCESS);
        }

        std::vector<VkLayerProperties> availableLayers(layerCount);
        {
            const VkResult enumerateInstanceLayerPropertiesResult = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
            assert( enumerateInstanceLayerPropertiesResult == VK_SUCCESS);
        }

        for (const char* layerName : m_validationLayers)
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

    void setupDebugCallback() 
    {
        if (!enableValidationLayers) return;

        VkDebugReportCallbackCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;//which events trigger the callback
        createInfo.pfnCallback = debugCallback;

        if (CreateDebugReportCallbackEXT(m_instance, &createInfo, nullptr, m_callback.replace()) != VK_SUCCESS)///@todo NTF: this callback spits out the error messages to the command window, which vanishes upon application exit.  Should really throw up a dialog or something far more noticeable and less ignorable
        {
            throw std::runtime_error("failed to set up debug callback!");
        }
    }

    struct SwapChainSupportDetails 
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) 
    {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
        if (formatCount != 0) 
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) 
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    struct QueueFamilyIndices 
    {
        int graphicsFamily = -1;
        int presentFamily = -1;

        bool isComplete() 
        {
            return graphicsFamily >= 0 && presentFamily >= 0;
        }
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) 
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) 
        {
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) 
            {
                indices.graphicsFamily = i;//queue supports rendering functionality
            }

            //TODO NTF: add logic to explicitly prefer a physical device that supports drawing and presentation in the same queue for improved performance rather than use presentFamily and graphicsFamily as separate queues
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport)
            {
                indices.presentFamily = i;//queue supports present functionality
            }

            if (indices.isComplete()) 
            {
                break;
            }

            i++;
        }

        return indices;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) 
    {
        if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) 
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

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) 
    {
        for (const auto& availablePresentMode : availablePresentModes) 
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) //instead of blocking the application when the queue is full, the images that are already queued are simply replaced with the newer ones
            {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;// display takes an image from the front of the queue on a vertical blank and the program inserts rendered images at the back of the queue. If the queue is full then the program has to wait
        /* could also return: 
            * VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from VK_PRESENT_MODE_FIFO_KHR if the application is late and the queue was 
                                                empty at the last vertical blank. Instead of waiting for the next vertical blank, the image is 
                                                transferred right away when it finally arrives. This may result in visible tearing.
            * VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away, which may result in tearing.
        */
    }

    ///choose the resolution of the render target based on surface capabilities and window resolution
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) 
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) 
        {
            return capabilities.currentExtent;
        }
        else 
        {
            VkExtent2D actualExtent = { kWidth, kHeight };

            actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }

    void createSwapChain() 
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        //implement triple-buffering by allowing one more buffer than the minimum image count required by the swap chain
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && //0 means max image count is unlimited
            imageCount > swapChainSupport.capabilities.maxImageCount) 
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;//1 for regular rendering; 2 for stereoscopic
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;//color attachment, since this is a render target

        QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
        uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };
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

        if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, m_swapChain.replace()) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create swap chain!");
        }

        //extract swap chain image handles
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
        m_swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

        m_swapChainImageFormat = surfaceFormat.format;
        m_swapChainExtent = extent;
    }

    void createImageViews()
    {
        m_swapChainImageViews.resize(m_swapChainImages.size(), VDeleter<VkImageView>{m_device, vkDestroyImageView});
        for (uint32_t i = 0; i < m_swapChainImages.size(); i++) 
        {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_swapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;//2D texture (not 1D or 3D textures, or a cubemap)
            createInfo.format = m_swapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            //no mipmapping, and one layer
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_device, &createInfo, nullptr, m_swapChainImageViews[i].replace()) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) 
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());

        for (const auto& extension : availableExtensions) 
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    bool isDeviceSuitable(VkPhysicalDevice device) 
    {        
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        const bool gpuSuitable =    deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && //dedicated GPU, not integrated
                                    deviceFeatures.geometryShader;

        QueueFamilyIndices indices = findQueueFamilies(device);
        
        bool swapChainAdequate = false;
        const bool extensionsSupported = checkDeviceExtensionSupport(device);
        if (extensionsSupported) 
        {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }
        return gpuSuitable && indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    void pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
        for (const auto& device : devices) 
        {
            if (isDeviceSuitable(device)) 
            {
                m_physicalDevice = device;
                break;
            }
        }

        if (m_physicalDevice == VK_NULL_HANDLE) 
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void createLogicalDevice() 
    {
        QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<int> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

        const float queuePriority = 1.0f;
        for (int queueFamily : uniqueQueueFamilies) 
        {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures = {};
        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = m_deviceExtensions.size();//require swapchain extension
        createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();//require swapchain extension

        if (enableValidationLayers) 
        {
            createInfo.enabledLayerCount = m_validationLayers.size();
            createInfo.ppEnabledLayerNames = m_validationLayers.data();
        }
        else 
        {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, m_device.replace()) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(m_device, indices.graphicsFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, indices.presentFamily, 0, &m_presentQueue);
    }

    void createGraphicsPipeline() 
    {
        auto vertShaderCode = readFile("shaders/vert.spv");
        auto fragShaderCode = readFile("shaders/frag.spv");

        //create wrappers around SPIR-V bytecodes
        VDeleter<VkShaderModule> vertShaderModule{ m_device, vkDestroyShaderModule };
        VDeleter<VkShaderModule> fragShaderModule{ m_device, vkDestroyShaderModule };
        createShaderModule(vertShaderCode, vertShaderModule);
        createShaderModule(fragShaderCode, fragShaderModule);

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

        ///@todo NTF: no vertex buffer for now; they're hardcoded in the vertex shader
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

        //specify triangle list
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_swapChainExtent.width;
        viewport.height = (float)m_swapChainExtent.height;
        //use entire depth range
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        //no scissor screenspace-culling
        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = m_swapChainExtent;

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
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

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
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = 0; // Optional

        if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr,m_pipelineLayout.replace()) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create pipeline layout!");
        }

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
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, m_graphicsPipeline.replace()) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
    }

    void createShaderModule(const std::vector<char>& code, VDeleter<VkShaderModule>& shaderModule)
    {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = (uint32_t*)code.data();

        if (vkCreateShaderModule(m_device, &createInfo, nullptr, shaderModule.replace()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create shader module!");
        }
    }

    void createRenderPass()
    {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = m_swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;                    //no MSAA
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;               //clear to a constant value @todo NTF: what value?; other options are VK_ATTACHMENT_LOAD_OP_LOAD: Preserve the existing contents of the attachment and VK_ATTACHMENT_LOAD_OP_DONT_CARE: Existing contents are undefined; we don't care about them
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;             //store buffer in memory for later; other option is VK_ATTACHMENT_STORE_OP_DONT_CARE: Contents of the framebuffer will be undefined after the rendering operation
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;    //not doing anything with stencil buffer
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  //not doing anything with stencil buffer
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;          //don't care about what layout the buffer was when we begin the renderpass
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;      //when the renderpass is complete the layout will be ready for presentation in the swap chain
        /*  other layouts: 
            * VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: Images used as color attachment
            * VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: Images to be presented in the swap chain
            * VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : Images to be used as destination for a memory copy operation */

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;//graphics subpass, not compute subpass
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, m_renderPass.replace()) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void initVulkan() 
    {
        createInstance();
        setupDebugCallback();
        createSurface();//window surface needs to be created right before physical device creation, because it can actually influence the physical device selection: TODO: learn more about this influence
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
    }

    void createFramebuffers() 
    {
        m_swapChainFramebuffers.resize(m_swapChainImageViews.size(), VDeleter<VkFramebuffer>{m_device, vkDestroyFramebuffer});

        for (size_t i = 0; i < m_swapChainImageViews.size(); i++)
        {
            VkImageView attachments[] =
            {
                m_swapChainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_swapChainExtent.width;
            framebufferInfo.height = m_swapChainExtent.height;
            framebufferInfo.layers = 1;//number of image arrays -- each swap chain image in pAttachments is a single image

            if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, m_swapChainFramebuffers[i].replace()) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void createSurface() 
    {
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr, m_surface.replace()) != VK_SUCCESS)//cross-platform window creation
        {
            throw std::runtime_error("failed to create window surface!");
        }
    }
    
    void mainLoop() 
    {
        while (!glfwWindowShouldClose(m_window)) 
        {
            glfwPollEvents();
        }
    }


    GLFWwindow* m_window;
    VDeleter<VkInstance> m_instance{ vkDestroyInstance };
    VDeleter<VkDebugReportCallbackEXT> m_callback{ m_instance, DestroyDebugReportCallbackEXT };
    VDeleter<VkSurfaceKHR> m_surface{ m_instance, vkDestroySurfaceKHR };
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;//doesn't need to be deleted, since physical devices can't be created or destroyed by software
    VDeleter<VkDevice> m_device{ vkDestroyDevice };//interface to the physical device; must be destroyed before the physical device (C++ guarantees order of destruction in reverse definition order)
    VDeleter<VkSwapchainKHR> m_swapChain{ m_device, vkDestroySwapchainKHR };//must be destroyed before the logical device (C++ guarantees order of destruction in reverse definition order)
    VkQueue m_graphicsQueue;//queues are implicitly cleaned up with the logical device; no need to delete
    VkQueue m_presentQueue;//queues are implicitly cleaned up with the logical device; no need to delete
    const std::vector<const char*> m_deviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    std::vector<VkImage> m_swapChainImages;//handles to images, which are created by the swapchain and will be destroyed by the swapchain.  Images are "multidimensional - up to 3 - arrays of data which can be used for various purposes (e.g. attachments, textures), by binding them to a graphics or compute pipeline via descriptor sets, or by directly specifying them as parameters to certain commands" -- https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VkImage.html
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;
    std::vector<VDeleter<VkImageView>> m_swapChainImageViews;//defines type of image (eg color buffer with mipmaps, depth buffer, and so on)
    std::vector<VDeleter<VkFramebuffer>> m_swapChainFramebuffers;
    VDeleter<VkRenderPass> m_renderPass{ m_device, vkDestroyRenderPass };
    VDeleter<VkPipelineLayout> m_pipelineLayout{ m_device, vkDestroyPipelineLayout };
    VDeleter<VkPipeline> m_graphicsPipeline{ m_device, vkDestroyPipeline };
};

int main() 
{
    HelloTriangleApplication app;

    try 
    {
        app.run();
    }
    catch (const std::runtime_error& e) 
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
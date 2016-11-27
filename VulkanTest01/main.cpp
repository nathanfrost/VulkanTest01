//originally from https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Base_code
//TODO: Next:
//https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include<iostream>
#include<stdexcept>
#include<functional>
#include<vector>
#include<assert.h>

#include"VDeleter.h"


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
    const int kWidth = 800; 
    const int kHeight = 600;

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

        if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) 
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

    struct QueueFamilyIndices {
        int graphicsFamily = -1;

        bool isComplete() 
        {
            return graphicsFamily >= 0;
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
                indices.graphicsFamily = i;//found an acceptable queue
            }

            if (indices.isComplete()) 
            {
                break;//TODO NTF: weird; this break seems to be able to go under the above if statement at present...not sure why the tutorial puts it here
            }

            i++;
        }

        return indices;
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
        return gpuSuitable && indices.isComplete();
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

        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
        queueCreateInfo.queueCount = 1;//only need one, since we can submit multiple command buffers to this single queue all at once with low overhead

        const float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;//each queue can be prioritized against other queues; here we have only one

        VkPhysicalDeviceFeatures deviceFeatures = {};
        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = 0;

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
    }

    void initVulkan() 
    {
        createInstance();
        setupDebugCallback();
        pickPhysicalDevice();
        createLogicalDevice();
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
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;//doesn't need to be deleted, since physical devices can't be created or destroyed by software
    VDeleter<VkDevice> m_device{ vkDestroyDevice };//interface to the physical device; must be destroyed before the physical device (C++ guarantees order of destruction in reverse definition order)
    VkQueue m_graphicsQueue;//queues are implicitly cleaned up with the logical device; no need to delete
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
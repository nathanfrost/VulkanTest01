#include"ntf_vulkan.h"

#define STB_IMAGE_IMPLEMENTATION
#include"stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include"tiny_obj_loader.h"

#if NTF_WIN_TIMER
FILE* s_winTimer;
#endif//NTF_WIN_TIMER

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
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device)
{
    VkCommandBuffer commandBuffer;
    BeginSingleTimeCommands(&commandBuffer, commandPool, device);

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

    EndSingleTimeCommands(commandBuffer, commandPool, graphicsQueue, device);
}

void TransitionImageLayout(
    const VkImage& image,
    const VkFormat& format,
    const VkImageLayout& oldLayout,
    const VkImageLayout& newLayout,
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device)
{
    VkCommandBuffer commandBuffer;
    BeginSingleTimeCommands(&commandBuffer, commandPool, device);

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (HasStencilComponent(format))
        {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    //not an array and has no mipmapping levels
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;//specifies write access to an image or buffer in a clear or copy operation.

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;//perform source operation immediately (and not at some later stage, like the vertex shader or fragment shader)
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        //block until source is done being written to, then block until shader is done reading from
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;//specifies write access to an image or buffer in a clear or copy operation.
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;//specifies read access to a storage buffer, uniform texel buffer, storage texel buffer, sampled image, or storage image.

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else
    {
        assert(false);//unsupported layout transition
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage,
        destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1,
        &barrier);

    EndSingleTimeCommands(commandBuffer, commandPool, graphicsQueue, device);
}

void CreateImage(
    VkImage*const imagePtr,
    VkDeviceMemory*const imageMemoryPtr,
    const uint32_t width,
    const uint32_t height,
    const VkFormat& format,
    const VkImageTiling& tiling,
    const VkImageUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    assert(imagePtr);
    auto& image = *imagePtr;

    assert(imageMemoryPtr);
    auto& imageMemory = *imageMemoryPtr;

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
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;//used by only one queue family

    const VkResult createImageResult = vkCreateImage(device, &imageInfo, nullptr, &image);
    NTF_VK_ASSERT_SUCCESS(createImageResult);

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties, physicalDevice);

    const VkResult allocateMemoryResult = vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory);
    NTF_VK_ASSERT_SUCCESS(allocateMemoryResult);

    vkBindImageMemory(device, image, imageMemory, 0);
}

void CopyBuffer(
    const VkBuffer& srcBuffer,
    const VkBuffer& dstBuffer,
    const VkDeviceSize& size,
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device)
{
    VkCommandBuffer commandBuffer;
    BeginSingleTimeCommands(&commandBuffer, commandPool, device);

    VkBufferCopy copyRegion = {};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    EndSingleTimeCommands(commandBuffer, commandPool, graphicsQueue, device);
}

//returns memoryTypeIndex that satisfies the constraints passed
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
    return 0;
}

void CreateBuffer(
    VkBuffer*const bufferPtr,
    VkDeviceMemory*const bufferMemoryPtr,
    const VkDeviceSize& size,
    const VkBufferUsageFlags& usage,
    const VkMemoryPropertyFlags& properties,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice
    )
{
    assert(bufferPtr);
    VkBuffer& buffer = *bufferPtr;

    assert(bufferMemoryPtr);
    auto& bufferMemory = *bufferMemoryPtr;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    const VkResult createBufferResult = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    NTF_VK_ASSERT_SUCCESS(createBufferResult);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties, physicalDevice);

    ///@todo: don't use vkAllocateMemory for individual buffers; instead use a custom allocator that splits up a single allocation among many different objects by using offset parameters (VulkanMemoryAllocator is an open source example).   We could also store multiple buffers, like the vertex and index buffer, into a single VkBuffer for cache.  It is even possible to reuse the same chunk of memory for multiple resources if they are not used during the same render operations, provided that their data is refreshed, of course. This is known as aliasing and some Vulkan functions have explicit flags to specify that you want to do this
    const VkResult allocateMemoryResult = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
    NTF_VK_ASSERT_SUCCESS(allocateMemoryResult);

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
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

void CreateShaderModule(VkShaderModule*const shaderModulePtr, const std::vector<char>& code, const VkDevice& device)
{
    assert(shaderModulePtr);
    VkShaderModule& shaderModule = *shaderModulePtr;

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = (uint32_t*)code.data();

    const VkResult createShaderModuleResult = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
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

    const VkResult createImageViewResult = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
    NTF_VK_ASSERT_SUCCESS(createImageViewResult);
}

///@todo: replace with proper allocation strategy for streaming
std::vector<char> ReadFile(const char*const filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    assert(file.is_open());

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);///<@todo: streaming memory management
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
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

void BeginSingleTimeCommands(VkCommandBuffer*const commandBufferPtr, const VkCommandPool& commandPool, const VkDevice& device)
{
    assert(commandBufferPtr);
    auto& commandBuffer = *commandBufferPtr;

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    ///@todo: pretty sure I should be using a pool of commmand buffers here
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

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

    return indices.isComplete() && extensionsSupported && supportedFeatures.samplerAnisotropy;
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
    ConstVectorSafeRef<const char*> deviceExtensions,
    ConstVectorSafeRef<const char*> validationLayers,
    const VkSurfaceKHR& surface,
    const VkPhysicalDevice& physicalDevice)
{
    assert(graphicsQueuePtr);
    VkQueue& graphicsQueue = *graphicsQueuePtr;

    assert(presentQueuePtr);
    VkQueue& presentQueue = *presentQueuePtr;

    assert(devicePtr);
    auto& device = *devicePtr;

    QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);

    const uint32_t queueFamiliesNum = 2;
    VectorSafe<VkDeviceQueueCreateInfo, queueFamiliesNum> queueCreateInfos(0);
    VectorSafe<int, queueFamiliesNum> uniqueQueueFamilies({ indices.graphicsFamily, indices.presentFamily });
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

    const VkResult createDeviceResult = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
    NTF_VK_ASSERT_SUCCESS(createDeviceResult);

    vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
}

void DescriptorTypeAssertOnInvalid(const VkDescriptorType descriptorType)
{
    assert(descriptorType >= VK_DESCRIPTOR_TYPE_BEGIN_RANGE && descriptorType <= VK_DESCRIPTOR_TYPE_END_RANGE);
}

void CreateDescriptorSetLayout(VkDescriptorSetLayout*const descriptorSetLayoutPtr, const VkDescriptorType descriptorType, const VkDevice& device)
{
    assert(descriptorSetLayoutPtr);
    VkDescriptorSetLayout& descriptorSetLayout = *descriptorSetLayoutPtr;

    DescriptorTypeAssertOnInvalid(descriptorType);

    VkDescriptorSetLayoutBinding uboLayoutBinding = {};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = descriptorType;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;///@todo: consider using this; immutable samplers compile sampler into shader, reducing latency in shader (on AMD the Scalar Arithmetic Logic Unit [SALU] is often underutilized, and is used to construct immutable samplers)
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VectorSafe<VkDescriptorSetLayoutBinding, 2> bindings({ uboLayoutBinding,samplerLayoutBinding });

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    const VkResult createDescriptorSetLayoutResult = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
    NTF_VK_ASSERT_SUCCESS(createDescriptorSetLayoutResult);
}

void CreateGraphicsPipeline(
    VkPipelineLayout*const pipelineLayoutPtr,
    VkPipeline*const graphicsPipelinePtr,
    const VkRenderPass& renderPass,
    const VkDescriptorSetLayout& descriptorSetLayout,
    const VkExtent2D& swapChainExtent,
    const VkDevice& device)
{
    assert(pipelineLayoutPtr);
    VkPipelineLayout& pipelineLayout = *pipelineLayoutPtr;
    assert(graphicsPipelinePtr);
    VkPipeline& graphicsPipeline = *graphicsPipelinePtr;

    auto vertShaderCode = ReadFile("shaders/vert.spv");
    auto fragShaderCode = ReadFile("shaders/frag.spv");

    //create wrappers around SPIR-V bytecodes
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    CreateShaderModule(&vertShaderModule, vertShaderCode, device);
    CreateShaderModule(&fragShaderModule, fragShaderCode, device);

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
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = 0; // Optional

    const VkResult createPipelineLayoutResult = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
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

    const VkResult createGraphicsPipelineResult = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);
    NTF_VK_ASSERT_SUCCESS(createGraphicsPipelineResult);

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
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

    const VkResult createRenderPassResult = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    NTF_VK_ASSERT_SUCCESS(createRenderPassResult);
}

void AllocateCommandBuffers(
    VectorSafeRef<VkCommandBuffer> commandBuffers,
    const VkCommandPool& commandPool,
    const int swapChainFramebuffersSize,
    const VkDevice& device)
{
    commandBuffers.size(swapChainFramebuffersSize);//bake one command buffer for every image in the swapchain so Vulkan can blast through them

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;//only value allowed
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;//primary can submit to execution queue, but not be submitted to other command buffers; secondary can't be submitted to execution queue but can be submitted to other command buffers (for example, to factor out common sequences of commands)
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    const VkResult allocateCommandBuffersResult = vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());
    NTF_VK_ASSERT_SUCCESS(allocateCommandBuffersResult);
}

void FillCommandBuffer(
    const VkCommandBuffer& vkCommandBuffer,
    const VkDescriptorSet& descriptorSet,
    const VkFramebuffer& swapChainFramebuffer,
    const VkRenderPass& renderPass,
    const VkExtent2D& swapChainExtent,
    const VkPipelineLayout& pipelineLayout,
    const VkPipeline& graphicsPipeline,
    const VkBuffer& vertexBuffer,
    const VkBuffer& indexBuffer,
    const uint32_t& indicesNum,
    const VkDevice& device)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; /* options: * VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once
                                                                                * VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be entirely within a single render pass.
                                                                                * VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT : The command buffer can be resubmitted while it is also already pending execution. */
    beginInfo.pInheritanceInfo = nullptr; //specifies what state a secondary buffer should inherit from the primary buffer
    vkBeginCommandBuffer(vkCommandBuffer, &beginInfo);  //implicitly resets the command buffer (you can't append commands to an existing buffer)

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

    vkCmdBeginRenderPass(vkCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE/**<no secondary buffers will be executed; VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS = secondary command buffers will execute these commands*/);
    vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkBuffer vertexBuffers[] = { vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(vkCommandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(vkCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    //note that with shader lines like the following, multiple descriptors can be passed such that per-object descriptors and shared descriptors can be passed in separate descriptor sets, so shared descriptors can be bound only once
    //layout(set = 0, binding = 0) uniform UniformBufferObject { ... }
    const uint32_t dynamicOffset = 0;
    vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS/*graphics not compute*/, pipelineLayout, 0, 1, &descriptorSet, 1, &dynamicOffset);

    vkCmdDrawIndexed(vkCommandBuffer, indicesNum, 1, 0, 0, 0);
    vkCmdEndRenderPass(vkCommandBuffer);

    const VkResult endCommandBufferResult = vkEndCommandBuffer(vkCommandBuffer);
    NTF_VK_ASSERT_SUCCESS(endCommandBufferResult);
}

VkDeviceSize UniformBufferCpuAlignmentCalculate(const VkDeviceSize bufferSize, const VkPhysicalDevice& physicalDevice)
{
    assert(bufferSize > 0);

    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
    const VkDeviceSize minUniformBufferOffsetAlignment = physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;

    VkDeviceSize uniformBufferAlignment = bufferSize;
    if (minUniformBufferOffsetAlignment > 0)
    {
        uniformBufferAlignment = (uniformBufferAlignment + minUniformBufferOffsetAlignment - 1) & ~(minUniformBufferOffsetAlignment - 1);
    }
    return uniformBufferAlignment;
}

void CreateUniformBuffer(
    VkBuffer*const uniformBufferPtr,
    VkDeviceMemory*const uniformBufferGpuMemoryPtr,
    ArraySafeRef<uint8_t>*const uniformBufferCpuMemoryPtr,
    const VkDeviceSize bufferSize,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    assert(uniformBufferPtr);
    auto& uniformBuffer = *uniformBufferPtr;

    assert(uniformBufferGpuMemoryPtr);
    auto& uniformBufferGpuMemory = *uniformBufferGpuMemoryPtr;

    assert(uniformBufferCpuMemoryPtr);
    auto& uniformBufferCpuMemory = *uniformBufferCpuMemoryPtr;

    assert(bufferSize > 0);

    CreateBuffer(
        &uniformBuffer,
        &uniformBufferGpuMemory,
        bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        device,
        physicalDevice);

    void* uniformBufferCpuMemoryCPtr;
    vkMapMemory(device, uniformBufferGpuMemory, 0, bufferSize, 0, &uniformBufferCpuMemoryCPtr);
    uniformBufferCpuMemory.SetArray(reinterpret_cast<uint8_t*>(uniformBufferCpuMemoryCPtr), Cast_size_t(bufferSize));
}

void CreateDescriptorPool(VkDescriptorPool*const descriptorPoolPtr, const VkDescriptorType descriptorType, const VkDevice& device)
{
    assert(descriptorPoolPtr);
    VkDescriptorPool& descriptorPool = *descriptorPoolPtr;

    DescriptorTypeAssertOnInvalid(descriptorType);

    const size_t kPoolSizesNum = 2;
    VectorSafe<VkDescriptorPoolSize, kPoolSizesNum> poolSizes(kPoolSizesNum);
    poolSizes[0].type = descriptorType;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());//number of elements in pPoolSizes
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;//max number of descriptor sets that can be allocated from the pool
    poolInfo.flags = 0;//if you allocate and free descriptors, don't use VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT here because that's abdicating memory allocation to the driver.  Instead use vkResetDescriptorPool() because it amounts to changing an offset for (de)allocation


    const VkResult createDescriptorPoolResult = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
    NTF_VK_ASSERT_SUCCESS(createDescriptorPoolResult);
}

void CreateDescriptorSet(
    VkDescriptorSet*const descriptorSetPtr,
    const VkDescriptorType descriptorType,
    const VkDescriptorSetLayout& descriptorSetLayout,
    const VkDescriptorPool& descriptorPool,
    const VkBuffer& uniformBuffer,
    const size_t uniformBufferSize,
    const VkImageView& textureImageView,
    const VkSampler& textureSampler,
    const VkDevice& device)
{
    assert(descriptorSetPtr);
    VkDescriptorSet& descriptorSet = *descriptorSetPtr;

    DescriptorTypeAssertOnInvalid(descriptorType);
    assert(uniformBufferSize > 0);

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

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureImageView;
    imageInfo.sampler = textureSampler;

    const size_t kDescriptorWritesNum = 2;
    VectorSafe<VkWriteDescriptorSet, kDescriptorWritesNum> descriptorWrites(kDescriptorWritesNum);

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;//descriptor is not an array
    descriptorWrites[0].descriptorType = descriptorType;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pNext = nullptr;//no extension

                                        //one of the following three must be non-null
    descriptorWrites[0].pBufferInfo = &bufferInfo;//if buffer data
    descriptorWrites[0].pImageInfo = nullptr; //if image data
    descriptorWrites[0].pTexelBufferView = nullptr; //if render view

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;//descriptor is not an array
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pNext = nullptr;//no extension

                                        //one of the following three must be non-null
    descriptorWrites[1].pBufferInfo = nullptr;//if buffer data
    descriptorWrites[1].pImageInfo = &imageInfo;
    descriptorWrites[1].pTexelBufferView = nullptr; //if render view

    vkUpdateDescriptorSets(
        device,
        static_cast<uint32_t>(descriptorWrites.size()),
        descriptorWrites.data()/*write to descriptor set*/,
        0, /*copy descriptor sets from one to another*/
        nullptr);
}
void LoadModel(std::vector<Vertex>*const verticesPtr, std::vector<uint32_t>*const indicesPtr)
{
    assert(verticesPtr);
    auto& vertices = *verticesPtr;

    assert(indicesPtr);
    auto& indices = *indicesPtr;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;///<@todo: streaming memory management
    std::vector<tinyobj::material_t> materials;///<@todo: streaming memory management
    std::string err;///<@todo: streaming memory management

    const bool loadObjResult = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, sk_ModelPath);
    assert(loadObjResult);

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
}

///@todo: refactor with CreateIndexBuffer()
void CreateVertexBuffer(
    VkBuffer*const vertexBufferPtr,
    VkDeviceMemory*const vertexBufferMemoryPtr,
    const std::vector<Vertex>& vertices,
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice
    )
{
    assert(vertexBufferPtr);
    auto& vertexBuffer = *vertexBufferPtr;

    assert(vertexBufferMemoryPtr);
    auto& vertexBufferMemory = *vertexBufferMemoryPtr;

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(&stagingBuffer,
        &stagingBufferMemory,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT/*writable from the CPU*/ | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,/*memory will have i/o coherency. If not set, application may need to use vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges to flush/invalidate host cache*/
        device,
        physicalDevice
        );

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    CreateBuffer(&vertexBuffer,
        &vertexBufferMemory,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT/*specifies that the buffer is suitable for passing as an element of the pBuffers array to vkCmdBindVertexBuffers*/,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,//most optimal graphics memory
        device,
        physicalDevice
        );

    CopyBuffer(stagingBuffer, vertexBuffer, bufferSize, commandPool, graphicsQueue, device);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

///@todo: refactor with CreateVertexBuffer()
void CreateIndexBuffer(
    VkBuffer*const indexBufferPtr,
    VkDeviceMemory*const indexBufferMemoryPtr,
    const std::vector<uint32_t>& indices,
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice
    )
{
    assert(indexBufferPtr);
    auto& indexBuffer = *indexBufferPtr;

    assert(indexBufferMemoryPtr);
    auto& indexBufferMemory = *indexBufferMemoryPtr;

    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(
        &stagingBuffer,
        &stagingBufferMemory,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        device,
        physicalDevice
        );

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    CreateBuffer(
        &indexBuffer,
        &indexBufferMemory,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        device,
        physicalDevice
        );

    CopyBuffer(stagingBuffer, indexBuffer, bufferSize, commandPool, graphicsQueue, device);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void EndSingleTimeCommands(const VkCommandBuffer& commandBuffer, const VkCommandPool commandPool, const VkQueue& graphicsQueue, const VkDevice& device)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);//could use a fence, which would allow you to schedule multiple transfers simultaneously and wait for all of them complete, instead of executing one at a time

                                   ///@todo: pretty sure I should be using a pool of commmand buffers here
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void CreateCommandPool(VkCommandPool*const commandPoolPtr, const VkSurfaceKHR& surface, const VkDevice& device, const VkPhysicalDevice& physicalDevice)
{
    assert(commandPoolPtr);
    auto& commandPool = *commandPoolPtr;

    QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(physicalDevice, surface);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;   //options:  VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often(may change memory allocation behavior)
                                                                        //          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
    const VkResult createCommandPoolResult = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
    NTF_VK_ASSERT_SUCCESS(createCommandPoolResult);
}

void CreateDepthResources(
    VkImage*const depthImagePtr,
    VkDeviceMemory*const depthImageMemoryPtr,
    VkImageView*const depthImageViewPtr,
    const VkExtent2D& swapChainExtent,
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    assert(depthImagePtr);
    auto& depthImage = *depthImagePtr;

    assert(depthImageMemoryPtr);
    auto& depthImageMemory = *depthImageMemoryPtr;

    assert(depthImageViewPtr);
    auto& depthImageView = *depthImageViewPtr;

    VkFormat depthFormat = FindDepthFormat(physicalDevice);

    CreateImage(
        &depthImage,
        &depthImageMemory,
        swapChainExtent.width,
        swapChainExtent.height,
        depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        device,
        physicalDevice);

    CreateImageView(&depthImageView, device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    TransitionImageLayout(
        depthImage,
        depthFormat,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        commandPool,
        graphicsQueue,
        device);
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

/* @todo:   All of the helper functions that submit commands so far have been set up to execute synchronously by
            waiting for the queue to become idle. For practical applications it is recommended to combine these
            operations in a single command buffer and execute them asynchronously for higher throughput, especially
            the transitions and copy in the CreateTextureImage function. Try to experiment with this by creating a
            setupCommandBuffer that the helper functions record commands into, and add a flushSetupCommands to
            execute the commands that have been recorded so far.*/
void CreateTextureImage(
    VkImage*const textureImagePtr,
    VkDeviceMemory*const textureImageMemoryPtr,
    const VkCommandPool& commandPool,
    const VkQueue& graphicsQueue,
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice)
{
    assert(textureImagePtr);
    auto& textureImage = *textureImagePtr;

    assert(textureImageMemoryPtr);
    auto& textureImageMemory = *textureImageMemoryPtr;

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(sk_texturePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    assert(pixels);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(
        &stagingBuffer,
        &stagingBufferMemory,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        device,
        physicalDevice
        );

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    stbi_image_free(pixels);

    CreateImage(
        &textureImage,
        &textureImageMemory,
        texWidth,
        texHeight,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL/*could also pass VK_IMAGE_TILING_LINEAR so texels are laid out in row-major order for debugging (less performant)*/,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT/*accessible by shader*/,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        device,
        physicalDevice);

    TransitionImageLayout(
        textureImage,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        commandPool,
        graphicsQueue,
        device);
    CopyBufferToImage(
        stagingBuffer,
        textureImage,
        static_cast<uint32_t>(texWidth),
        static_cast<uint32_t>(texHeight),
        commandPool,
        graphicsQueue,
        device);
    TransitionImageLayout(
        textureImage,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        commandPool,
        graphicsQueue,
        device);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
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

    const VkResult createSamplerResult = vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler);
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

        const VkResult createFramebufferResult = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]);
        NTF_VK_ASSERT_SUCCESS(createFramebufferResult);
    }
}

void CreateSurface(VkSurfaceKHR*const surfacePtr, GLFWwindow*const window, const VkInstance& instance)
{
    assert(surfacePtr);
    auto& surface = *surfacePtr;

    assert(window);

    const VkResult createWindowSurfaceResult = glfwCreateWindowSurface(instance, window, nullptr, &surface);//cross-platform window creation
    NTF_VK_ASSERT_SUCCESS(createWindowSurfaceResult);
}

void CreateFrameSyncPrimitives(
    VectorSafeRef<VkSemaphore> imageAvailable, 
    VectorSafeRef<VkSemaphore> renderFinished, 
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
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable[frameIndex]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished[frameIndex]) != VK_SUCCESS)
        {
            assert(false);//failed to create semaphores
        }
        if (vkCreateFence(device, &fenceInfo, nullptr/*no allocator specified*/, &fence[frameIndex]) != VK_SUCCESS)
        {
            assert(false);//failed to create fence
        }
    }
}

///@todo: use push constants instead, since it's more efficient
void UpdateUniformBuffer(
    ArraySafeRef<uint8_t> uniformBufferCpuMemory,
    const VkDeviceMemory& uniformBufferGpuMemory, 
    const VkDeviceSize uniformBufferSize, 
    const VkExtent2D& swapChainExtent, 
    const VkDevice& device)
{
    assert(uniformBufferSize);

    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;

    const glm::mat4 worldRotation = glm::rotate(glm::mat4(), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::mat4 worldTranslation = glm::translate(glm::mat4(), glm::vec3(-1.f, -1.f, 0.f));
    const glm::mat4 modelToWorld = worldTranslation*worldRotation;
    const glm::mat4 worldToView = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 viewToClip = glm::perspective(glm::radians(45.0f), swapChainExtent.width / static_cast<float>(swapChainExtent.height), 0.1f, 10.0f);
    viewToClip[1][1] *= -1;//OpenGL's clipspace y-axis points in opposite direction of Vulkan's y-axis; doing this requires counterclockwise vertex winding

    UniformBufferObject ubo = {};
    ubo.modelToClip = viewToClip*worldToView*modelToWorld;
    
    uniformBufferCpuMemory.MemcpyFromStart(&ubo, sizeof(ubo));

    VkMappedMemoryRange mappedMemoryRange;
    mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedMemoryRange.pNext = nullptr;
    mappedMemoryRange.memory = uniformBufferGpuMemory;
    mappedMemoryRange.offset = 0;
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
        //hackToRecreateSwapChainIfNecessary.recreateSwapChain();//haven't seen this get hit yet, even when minimizing and resizing the window
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
        //hackToRecreateSwapChainIfNecessary.recreateSwapChain();//haven't seen this get hit yet, even when minimizing and resizing the window
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
    const VkResult createInstanceResult = vkCreateInstance(&createInfo, nullptr, &instance);
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
    const VkResult createDebugReportCallbackEXTResult = CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, &callback);//@todo NTF: this callback spits out the error messages to the command window, which vanishes upon application exit.  Should really throw up a dialog or something far more noticeable and less ignorable
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
        if (queueFamilyProperties.queueCount > 0 && queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = queueFamilyIndex;//queue supports rendering functionality
        }

        //TODO NTF: add logic to explicitly prefer a physical device that supports drawing and presentation in the same queue for improved performance rather than use presentFamily and graphicsFamily as separate queues
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, queueFamilyIndex, surface, &presentSupport);
        if (queueFamilyProperties.queueCount > 0 && presentSupport)
        {
            indices.presentFamily = queueFamilyIndex;//queue supports present functionality
        }

        if (indices.isComplete())
        {
            break;
        }

        queueFamilyIndex++;
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

    const VkResult createSwapchainKHRResult = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain);
    NTF_VK_ASSERT_SUCCESS(createSwapchainKHRResult);

    //extract swap chain image handles
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImagesNum, nullptr);
    swapChainImages.AssertSufficient(swapChainImagesNum);
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImagesNum, swapChainImages.data());
    swapChainImages.size(swapChainImagesNum);

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void CleanupSwapChain(
    VectorSafeRef<VkCommandBuffer> commandBuffers,
    const VkDevice& device,
    const VkImageView& depthImageView,
    const VkImage& depthImage,
    const VkDeviceMemory& depthImageMemory,
    ConstVectorSafeRef<VkFramebuffer> swapChainFramebuffers,
    const VkCommandPool& commandPool,
    const VkPipeline& graphicsPipeline,
    const VkPipelineLayout& pipelineLayout,
    const VkRenderPass& renderPass,
    ConstVectorSafeRef<VkImageView> swapChainImageViews,
    const VkSwapchainKHR& swapChain)
{
    assert(commandBuffers.size() == swapChainFramebuffers.size());
    assert(swapChainFramebuffers.size() == swapChainImageViews.size());

    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);

    for (const VkFramebuffer vkFramebuffer : swapChainFramebuffers)
    {
        vkDestroyFramebuffer(device, vkFramebuffer, nullptr);
    }

    //return command buffers to the pool from whence they came
    vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);

    for (const VkImageView vkImageView : swapChainImageViews)
    {
        vkDestroyImageView(device, vkImageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);
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

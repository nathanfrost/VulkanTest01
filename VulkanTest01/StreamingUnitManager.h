#pragma once
#include"ntf_vulkan.h"

DWORD WINAPI AssetLoadingThread(void* arg);

struct QueueFamilyIndices;
class AssetLoadingArguments
{
public:
    VulkanPagedStackAllocator* m_deviceLocalMemory;
    StreamingUnitRuntime* m_streamingUnit;
    enum ThreadCommand {
        kFirstValidArgument, kLoadStreamingUnit = kFirstValidArgument,
        kLastValidArgument, kCleanupAndTerminate = kLastValidArgument
    } *m_threadCommand;

    const VkCommandBuffer* m_commandBufferTransfer;
    const VkCommandBuffer* m_commandBufferTransitionImage;
    const VkDevice* m_device;
    const VkQueue* m_graphicsQueue;
    const VkPhysicalDevice* m_physicalDevice;
    const QueueFamilyIndices* m_queueFamilyIndices;
    const VkRenderPass* m_renderPass;
    const char* m_streamingUnitFilenameNoExtension;
    const VkExtent2D* m_swapChainExtent;
    const HANDLE* m_threadDone;
    const HANDLE* m_threadWake;
    const VkQueue* m_transferQueue;

    inline void AssertValid()
    {
        assert(m_deviceLocalMemory);
        assert(m_streamingUnit);
        assert(m_threadCommand && *m_threadCommand >= kFirstValidArgument && *m_threadCommand <= kLastValidArgument);

        assert(m_commandBufferTransfer);
        assert(m_commandBufferTransitionImage);
        assert(m_device);
        assert(m_graphicsQueue);
        assert(m_physicalDevice);
        assert(m_queueFamilyIndices);
        assert(m_renderPass);
        assert(m_streamingUnitFilenameNoExtension);
        assert(strlen(m_streamingUnitFilenameNoExtension) > 0);
        assert(m_swapChainExtent);
        assert(m_threadDone);
        assert(m_threadWake);
        assert(m_transferQueue);
    }
};
struct AssetLoadingThreadData
{
    ThreadHandles m_handles;
    AssetLoadingArguments::ThreadCommand m_threadCommand;
};

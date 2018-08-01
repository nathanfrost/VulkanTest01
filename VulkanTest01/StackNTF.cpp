#include"StackNTF.h"
#include <vulkan/vulkan.h>
#include "assert.h"
#include"MemoryUtil.h"
#include<malloc.h>

template<class SizeT>
bool StackNTF<SizeT>::PushAlloc(SizeT* memoryOffsetPtr, const SizeT alignment, const SizeT size)
{
    assert(memoryOffsetPtr);
    auto& memoryOffset = *memoryOffsetPtr;

    assert(size > 0);

    const bool allocateResult = PushAllocInternal(&m_firstByteFree, &memoryOffset, alignment, size);
    assert(allocateResult);
    return allocateResult;
}
template bool StackNTF<VkDeviceSize>::PushAlloc(VkDeviceSize* memoryOffsetPtr, const VkDeviceSize alignment, const VkDeviceSize size);

template<class SizeT>
void StackNTF<SizeT>::Allocate(const SizeT memoryMaxBytes)
{
#if NTF_DEBUG
    assert(memoryMaxBytes > 0);
    assert(!m_allocated);
    m_allocated = true;
#endif//#if NTF_DEBUG

    m_maxOffsetPlusOne = memoryMaxBytes;
    m_firstByteFree = 0;
}
template void StackNTF<VkDeviceSize>::Allocate(const VkDeviceSize memoryMaxBytes);

template<class SizeT>
bool StackNTF<SizeT>::PushAllocInternal(SizeT*const firstByteFreePtr, SizeT*const firstByteReturnedPtr, const SizeT alignment, const SizeT size) const
{
#if NTF_DEBUG
    assert(m_allocated);
#endif//#if NTF_DEBUG

    assert(firstByteFreePtr);
    auto& firstByteFree = *firstByteFreePtr;

    assert(firstByteReturnedPtr);
    auto& firstByteReturned = *firstByteReturnedPtr;

    assert(size > 0);

    firstByteReturned = (alignment > 0) ? RoundToNearest(m_firstByteFree, alignment) : m_firstByteFree;
    if (firstByteReturned >= m_maxOffsetPlusOne)
    {
        return false;
    }

    firstByteFree = firstByteReturned + size;
    if (firstByteFree >= m_maxOffsetPlusOne)
    {
        return false;
    }

    return true;
}
template bool StackNTF<VkDeviceSize>::PushAllocInternal(VkDeviceSize*const firstByteFreePtr, VkDeviceSize*const firstByteReturnedPtr, const VkDeviceSize alignment, const VkDeviceSize size) const;

///@todo: allocating memory in Initialize() and not Alloc() is not consistent with VulkanMemoryHeapPage; consider redesign
void StackCpu::Initialize(const size_t sizeBytes)
{
#if NTF_DEBUG
    assert(!m_initialized);
    m_initialized = true;
#endif//#if NTF_DEBUG

    m_memory = reinterpret_cast<uint8_t*>(malloc(sizeBytes));
    assert(m_memory);
    m_stack.Allocate(sizeBytes);
}

bool StackCpu::PushAlloc(void**const memoryRetPtr, const size_t sizeBytes)
{
    assert(m_initialized);

    assert(memoryRetPtr);
    auto& memoryRet = *memoryRetPtr;

    assert(sizeBytes > 0);

    size_t memoryOffset;
    const bool allocResult = m_stack.PushAlloc(&memoryOffset, 0, sizeBytes);
    memoryRet = m_memory + memoryOffset;
    return allocResult;
}

void StackCpu::Destroy()
{
#if NTF_DEBUG
    assert(m_initialized);
    m_initialized = false;
#endif//#if NTF_DEBUG    

    free(m_memory);
    m_memory = nullptr; 
}
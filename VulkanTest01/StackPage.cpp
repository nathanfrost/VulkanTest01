#include"StackPage.h"
#include <vulkan/vulkan.h>
#include "assert.h"
#include"MemoryUtil.h"

template<class SizeT>
bool StackPage<SizeT>::PushAlloc(SizeT* memoryOffsetPtr, const SizeT alignment, const SizeT size)
{
    assert(memoryOffsetPtr);
    auto& memoryOffset = *memoryOffsetPtr;

    assert(alignment > 0);
    assert(size > 0);

    const bool allocateResult = PushAllocInternal(&m_firstByteFree, &memoryOffset, alignment, size);
    assert(allocateResult);
    return allocateResult;
}
template bool StackPage<VkDeviceSize>::PushAlloc(VkDeviceSize* memoryOffsetPtr, const VkDeviceSize alignment, const VkDeviceSize size);

template<class SizeT>
void StackPage<SizeT>::Allocate(const SizeT memoryMax)
{
#if NTF_DEBUG
    assert(memoryMax > 0);
    assert(!m_allocated);
    m_allocated = true;
#endif//#if NTF_DEBUG

    m_maxOffsetPlusOne = memoryMax;
    m_firstByteFree = 0;
}
template void StackPage<VkDeviceSize>::Allocate(const VkDeviceSize memoryMax);

template<class SizeT>
bool StackPage<SizeT>::PushAllocInternal(SizeT*const firstByteFreePtr, SizeT*const firstByteReturnedPtr, const SizeT alignment, const SizeT size) const
{
#if NTF_DEBUG
    assert(m_allocated);
#endif//#if NTF_DEBUG

    assert(firstByteFreePtr);
    auto& firstByteFree = *firstByteFreePtr;

    assert(firstByteReturnedPtr);
    auto& firstByteReturned = *firstByteReturnedPtr;

    assert(alignment > 0);
    assert(size > 0);

    firstByteReturned = RoundToNearest(m_firstByteFree, alignment);
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
template bool StackPage<VkDeviceSize>::PushAllocInternal(VkDeviceSize*const firstByteFreePtr, VkDeviceSize*const firstByteReturnedPtr, const VkDeviceSize alignment, const VkDeviceSize size) const;

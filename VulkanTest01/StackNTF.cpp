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
    if (firstByteFree > m_maxOffsetPlusOne)
    {
        return false;
    }

    return true;
}
template bool StackNTF<VkDeviceSize>::PushAllocInternal(VkDeviceSize*const firstByteFreePtr, VkDeviceSize*const firstByteReturnedPtr, const VkDeviceSize alignment, const VkDeviceSize size) const;

///@todo: allocating memory in Initialize() and not Alloc() is not consistent with VulkanMemoryHeapPage; consider redesign
template<class SizeT>
void StackCpu<SizeT>::Initialize(uint8_t*const p, const SizeT sizeBytes)
{
    InitializeInternal(p, sizeBytes);
}
template<class SizeT>
void StackCpu<SizeT>::InitializeInternal(uint8_t*const p, const SizeT sizeBytes)
{
#if NTF_DEBUG
    assert(!m_initialized);
    m_initialized = true;
#endif//#if NTF_DEBUG

    assert(p);
    m_memory = p;
    m_stack.Allocate(sizeBytes);
}

template<class SizeT>
bool StackCpu<SizeT>::PushAlloc(ArraySafeRef<uint8_t>*const memoryRetPtr, SizeT*const memoryOffsetPtr, const SizeT alignment, const SizeT sizeBytes)
{
    NTF_REF(memoryRetPtr, memoryRet);
    NTF_REF(memoryOffsetPtr, memoryOffset);
    assert(sizeBytes > 0);

    void* voidPtr;
    const bool ret = PushAlloc(&voidPtr, &memoryOffset, alignment, sizeBytes);
    memoryRet.SetArray(reinterpret_cast<uint8_t*>(voidPtr), CastWithAssert<SizeT,size_t>(sizeBytes));
    return ret;
}
template<class SizeT>
bool StackCpu<SizeT>::PushAlloc(ArraySafeRef<uint8_t>*const memoryRetPtr, const SizeT alignment, const SizeT sizeBytes)
{
    SizeT dummy;
    return PushAlloc(memoryRetPtr, &dummy, alignment, sizeBytes);
}
template<class SizeT>
bool StackCpu<SizeT>::MemcpyIfPushAllocSucceeds(
    ArraySafeRef<uint8_t>*const memoryRetPtr, 
    const void*const dataToMemcpy, 
    const SizeT alignment,
    const SizeT sizeBytes)
{
    NTF_REF(memoryRetPtr, memoryRet);
    assert(dataToMemcpy);
    assert(sizeBytes > 0);

    const bool hasSpace = PushAlloc(&memoryRet, alignment, sizeBytes);
    if (hasSpace)
    {
        memoryRet.MemcpyFromStart(dataToMemcpy, CastWithAssert<SizeT, size_t>(sizeBytes));
    }
    return hasSpace;
}

template<class SizeT>
bool StackCpu<SizeT>::PushAlloc(void**const memoryRetPtr, SizeT*const memoryOffsetPtr, const SizeT alignment, const SizeT sizeBytes)
{
    assert(m_initialized);

    assert(memoryRetPtr);
    auto& memoryRet = *memoryRetPtr;

    NTF_REF(memoryOffsetPtr, memoryOffset);

    assert(sizeBytes > 0);

    const bool allocResult = m_stack.PushAlloc(&memoryOffset, alignment, sizeBytes);
    memoryRet = m_memory + memoryOffset;
    return allocResult;
}
template<class SizeT>
bool StackCpu<SizeT>::PushAlloc(void**const memoryRetPtr, const SizeT alignment, const SizeT sizeBytes)
{
    SizeT dummy;
    return PushAlloc(memoryRetPtr, &dummy, alignment, sizeBytes);
}

template<class SizeT>
void StackCpu<SizeT>::Destroy()
{
#if NTF_DEBUG
    assert(m_initialized);
    m_initialized = false;
#endif//#if NTF_DEBUG    

    m_stack.Free();
    m_memory = nullptr; 
}
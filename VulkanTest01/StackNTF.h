#pragma once

#include<vulkan/vulkan_core.h>
#include"ntf_compileTimeDefines.h"
#include"stdArrayUtility.h"

template<class SizeT>
class StackNTF
{
public:

    explicit inline StackNTF()
    {
        m_allocated = false;
    }
    StackNTF(const StackNTF& other) = default;
    StackNTF& operator=(const StackNTF& other) = default;
    StackNTF(StackNTF&& other) = default;
    StackNTF& operator=(StackNTF&& other) = default;
    ~StackNTF() = default;

    void Allocate(const SizeT memoryMaxBytes);
    inline void ClearSuballocations() { assert(m_allocated); m_firstByteFree = 0; }
    inline void Free()
    {
        ClearSuballocations();

        assert(m_allocated);
        m_allocated = false;
    }

    inline SizeT GetFirstByteFree() const{ assert(m_allocated); return m_firstByteFree; }
    inline bool Allocated() const { return m_allocated; }

    bool PushAlloc(SizeT* memoryOffsetPtr, const SizeT alignment, const SizeT size);
    bool PushAllocInternal(SizeT*const firstByteFreePtr, SizeT*const firstByteReturnedPtr, SizeT const alignment, SizeT const size) const;
    inline bool IsEmptyAndAllocated() const { return m_allocated && GetFirstByteFree() == 0; }
    inline SizeT GetMaxBytes() const { return m_maxOffsetPlusOne; }

private:
    SizeT m_maxOffsetPlusOne;///<page's first invalid memory address within its allocation
    SizeT m_firstByteFree;
    bool m_allocated;
};

template<class SizeT>
class StackCpu
{
public:
    explicit StackCpu()
    {
#if NTF_DEBUG
        m_initialized = false;
#endif//#if NTF_DEBUG
    };
    StackCpu(const StackCpu& other) = default;
    StackCpu& operator=(const StackCpu& other) = default;
    StackCpu(StackCpu&& other) = default;
    StackCpu& operator=(StackCpu&& other) = default;
    ~StackCpu() = default;

    void Initialize(uint8_t*const p, const SizeT sizeBytes);
    void Destroy();

    inline void Clear() { m_stack.ClearSuballocations(); }

    bool PushAlloc(ArraySafeRef<uint8_t>*const memoryRetPtr, SizeT*const memoryOffsetPtr, const SizeT alignment, const SizeT sizeBytes);
    bool PushAlloc(ArraySafeRef<uint8_t>*const memoryRetPtr, const SizeT alignment, const SizeT sizeBytes);
    bool MemcpyIfPushAllocSucceeds(
        ArraySafeRef<uint8_t>*const memoryRetPtr,
        const void*const dataToMemcpy,
        const SizeT alignment,
        const SizeT sizeBytes);
    bool PushAlloc(void**const memoryRetPtr, SizeT*const memoryOffsetPtr, const SizeT alignment, const SizeT sizeBytes);
    bool PushAlloc(void**const memoryRetPtr, const SizeT alignment, const SizeT sizeBytes);
    inline SizeT GetFirstByteFree(){ assert(m_initialized); return m_stack.GetFirstByteFree(); }
    inline ArraySafeRef<uint8_t> GetMemory() { return ArraySafeRef<uint8_t>(m_memory, CastWithAssert<SizeT,size_t>(m_stack.GetMaxBytes())); }
    inline bool IsEmptyAndAllocated() const { return m_stack.IsEmptyAndAllocated(); }

private:
    void InitializeInternal(uint8_t*const p, const SizeT sizeBytes);
#if NTF_DEBUG
    bool m_initialized;
#endif//#if NTF_DEBUG
    uint8_t* m_memory;///<*this is not responsible for freeing the memory pointer; *this merely uses the buffer of memory it is passed
    StackNTF<SizeT> m_stack;//tracks stack allocations within memory allocation
};
#pragma warning(disable : 4661) //disable spurious "no suitable definition provided for explicit template instantiation request" error in Visual Studio 2015; all template class methods are defined and link correctly
template class StackCpu<size_t>;
template class StackCpu<VkDeviceSize>;
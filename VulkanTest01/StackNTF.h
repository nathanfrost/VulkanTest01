#pragma once

#include"ntf_compileTimeDefines.h"
#include"stdArrayUtility.h"

template<class SizeT>
class StackNTF
{
public:

    ///@todo: all explicit default C++ functions except default constructor
    inline StackNTF()
    {
        m_allocated = false;
    }

    void Allocate(const SizeT memoryMaxBytes);
    inline void ClearSuballocations() { assert(m_allocated); m_firstByteFree = 0; }
    inline void Free()
    {
        assert(m_allocated);
        m_allocated = false;
    }

    inline SizeT GetFirstByteFree(){ assert(m_allocated); return m_firstByteFree; }
    inline bool Allocated() const { return m_allocated; }

    bool PushAlloc(SizeT* memoryOffsetPtr, const SizeT alignment, const SizeT size);
    bool PushAllocInternal(SizeT*const firstByteFreePtr, SizeT*const firstByteReturnedPtr, SizeT const alignment, SizeT const size) const;

private:
    SizeT m_maxOffsetPlusOne;///<page's first invalid memory address within its allocation
    SizeT m_firstByteFree;
    bool m_allocated;
};

///@todo: consider refactoring VulkanMemoryHeap to a generalized linked list of StackNTF's (call it "StackPageList"), and then refactor this class to use StackPageList
class StackCpu
{
public:
    StackCpu()
    {
#if NTF_DEBUG
        m_initialized = false;
#endif//#if NTF_DEBUG
    };
    void Initialize(uint8_t*const p, const size_t sizeBytes);
    ///@todo: all explicit default C++ functions

    void Destroy();
    inline void Clear() { m_stack.ClearSuballocations(); }

    bool PushAlloc(ArraySafeRef<uint8_t>*const memoryRetPtr, const size_t alignment, const size_t sizeBytes);
    bool MemcpyIfPushAllocSucceeds(
        ArraySafeRef<uint8_t>*const memoryRetPtr,
        const void*const dataToMemcpy,
        const size_t alignment,
        const size_t sizeBytes);
    bool PushAlloc(void**const memoryPtr, const size_t alignment, const size_t sizeBytes);
    inline size_t GetFirstByteFree(){ assert(m_initialized); return m_stack.GetFirstByteFree(); }
    inline uint8_t*const GetMemory() { return m_memory; }

private:
    void InitializeInternal(uint8_t*const p, const size_t sizeBytes);
#if NTF_DEBUG
    bool m_initialized;
#endif//#if NTF_DEBUG
    uint8_t* m_memory;
    StackNTF<size_t> m_stack;//tracks stack allocations within memory allocation
};

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
#if NTF_DEBUG
        m_allocated = false;
#endif//#if NTF_DEBUG
    }

    void Allocate(const SizeT memoryMaxBytes);
    inline void ClearSuballocations() { m_firstByteFree = 0; }
    inline void Free()
    {
#if NTF_DEBUG
        assert(m_allocated);
        m_allocated = false;
#endif//#if NTF_DEBUG
    }

#if NTF_DEBUG
    inline bool Allocated() const { return m_allocated; }
#endif//#if NTF_DEBUG

    bool PushAlloc(SizeT* memoryOffsetPtr, const SizeT alignment, const SizeT size);
    bool PushAllocInternal(SizeT*const firstByteFreePtr, SizeT*const firstByteReturnedPtr, SizeT const alignment, SizeT const size) const;

private:
    SizeT m_maxOffsetPlusOne;///<page's first invalid memory address within its allocation
    SizeT m_firstByteFree;

#if NTF_DEBUG
    bool m_allocated;
#endif//#if NTF_DEBUG
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
    void Initialize(const size_t pageSizeBytes);
    ///@todo: all explicit default C++ functions

    void Destroy();
    inline void Clear() { m_stack.ClearSuballocations(); }

    bool PushAlloc(void**const memoryPtr, const size_t sizeBytes);

private:
#if NTF_DEBUG
    bool m_initialized;
#endif//#if NTF_DEBUG
    uint8_t* m_memory;
    StackNTF<size_t> m_stack;//tracks stack allocations within memory allocation
};

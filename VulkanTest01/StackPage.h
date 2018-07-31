#pragma once

#include"ntf_compileTimeDefines.h"

template<class SizeT>
class StackPage
{
public:

    ///@todo: all explicit default C++ functions except default constructor
    inline StackPage()
    {
#if NTF_DEBUG
        m_allocated = false;
#endif//#if NTF_DEBUG
    }

    void Allocate(const SizeT memoryMax);
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

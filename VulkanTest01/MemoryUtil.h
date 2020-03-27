#pragma once

#include<windows.h>
#include<assert.h>

///@todo: refactor to a more sensible translation unit
#define NTF_STATIC_ASSERT(expr) static_assert(expr, #expr)

#define NTF_ROUND_TO_NEAREST(i,alignment) (i + alignment - 1) & ~(alignment - 1)
template<class T>
inline T RoundToNearest(const T i, const T alignment)
{
    assert(alignment > 0);
    assert(i >= 0);
    return NTF_ROUND_TO_NEAREST(i, alignment);
}

inline void* AlignedMalloc(size_t size, size_t alignment)
{
    assert(size > 0);
    assert(alignment > 0);

    void* ret = _aligned_malloc(size, alignment);
    assert(ret);
    assert((uintptr_t)(ret) % (uintptr_t)(alignment) == 0);
    return ret;
}

inline void* AlignedRealloc(void* mem, size_t size, size_t alignment)
{
    return _aligned_realloc(mem, size, alignment);
}

inline void AlignedFree(void* mem)
{
    _aligned_free(mem);
}

template<typename OriginType, typename DestinationType>
inline DestinationType CastWithAssert(const OriginType numOrigin)
{
    const DestinationType numDestination = static_cast<DestinationType>(numOrigin);
#pragma warning(disable : 4389)//ignore signed/unsigned mismatches; this assert is meant to guard against any such mismatch that causes numOrigin's value to change as a result of being cast.  Any other mismatches are fine
    assert(numDestination == numOrigin);
#pragma warning(default : 4389)
    return numDestination;
}

#pragma once

#include<windows.h>

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

///@todo: unduplicate all below casts
///@todo: unit test
template<typename OriginType, typename DestinationType>
inline DestinationType CastWithAssert(const OriginType numOrigin)
{
    const DestinationType numDestination = static_cast<DestinationType>(numOrigin);
#pragma warning(disable : 4389)//ignore signed/unsigned mismatches; this assert is meant to guard against any such mismatch that causes numOrigin's value to change as a result of being cast.  Any other mismatches are fine
    assert(numDestination == numOrigin);
#pragma warning(default : 4389)
    return numDestination;
}

///@todo: unit test
inline size_t Cast_uint64_t_size_t(const uint64_t num)
{
    const size_t num_size_t = static_cast<size_t>(num);
    assert(num_size_t == num);
    return num_size_t;
}
///@todo: unit test
inline DWORD Cast_size_t_DWORD(const size_t num)
{
    const DWORD num_size_t = static_cast<DWORD>(num);
    assert(num_size_t == num);
    return num_size_t;
}
///@todo: unit test
inline uint8_t Cast_int_uint8_t(const int num)
{
    const uint8_t num_uint8_t = static_cast<uint8_t>(num);
    assert(num_uint8_t == num);
    return num_uint8_t;
}
///@todo: unit test
inline uint16_t Cast_int_uint16_t(const int num)
{
    const uint16_t num_uint16_t = static_cast<uint16_t>(num);
    assert(num_uint16_t == num);
    return num_uint16_t;
}
///@todo: unit test
inline uint32_t Cast_size_t_uint32_t(const size_t num)
{
    const uint32_t num_uint32_t = static_cast<uint32_t>(num);
    assert(num_uint32_t == num);
    return num_uint32_t;
}
///@todo: unit test
inline uint8_t Cast_size_t_uint8_t(const size_t num)
{
    const uint8_t num_uint8_t = static_cast<uint8_t>(num);
    assert(num_uint8_t == num);
    return num_uint8_t;
}

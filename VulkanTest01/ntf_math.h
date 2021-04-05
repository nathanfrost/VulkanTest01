#pragma once

#include<stdint.h>//for int32_t, etc
#include"stdArrayUtility.h"

#define NTF_PI      (3.141592653589793f)

///@todo: unit tests

namespace ntf
{
    template<class T>
    inline T Min(const T a, const T b)
    {
        return a < b ? a : b;
    }

    template<class T>
    inline T Max(const T a, const T b)
    {
        return a > b ? a : b;
    }

    template<class T>
    inline T Clamp(const T v, const T min, const T max)
    {
        assert(min <= max);
        return Max(min, Min(v, max));
    }
    template<class T>
    inline T ClampNormalized(const T v)
    {
        const T zero = static_cast<T>(0);
        const T one = static_cast<T>(1);
        return Clamp(v, zero, one);
    }
    template<class T>
    inline T ClampZeroTo(const T v, const T max)
    {
        const T zero = static_cast<T>(0);
        assert(max >= zero);
        return Clamp(v, zero, max);
    }


    inline void DivideByTwoIfGreaterThanOne(uint32_t*const vPtr)
    {
        NTF_REF(vPtr, v);

        if (v > 1)
        {
            v >>= 1;
        }
    }
}


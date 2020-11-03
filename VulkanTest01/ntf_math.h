#pragma once

#define NTF_PI      (3.141592653589793f)

template<class T>
inline T MaxNtf(const T a, const T b)
{
    assert(a >= 0);
    assert(b >= 0);
    return a > b ? a : b;
}
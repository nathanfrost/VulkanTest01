#pragma once

template<class type, int size>
size_t SizeOfArrayInBytes(const std::array<type, size> &a)
{
    return a.size()*sizeof(a[0]);
}
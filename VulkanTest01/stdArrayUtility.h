#pragma once

template<class type, size_t size>
size_t SizeOfArrayInBytes(const std::array<type, size> &a)
{
    return a.size()*sizeof(a[0]);
}

template<class type, size_t size>
void SortAndRemoveDuplicatesFromArray(std::array<type, size>*const a, size_t*const uniqueElementsNum)
{
    assert(a);
    assert(uniqueElementsNum);

    std::sort(a->begin(), a->end());
    RemoveDuplicatesFromSortedArray(a, uniqueElementsNum);
}

template<class type, size_t size>
void RemoveDuplicatesFromSortedArray(std::array<type, size>*const a, size_t*const uniqueElementsNum)
{
    assert(a);
    assert(uniqueElementsNum);

    std::array<type, size>& aRef = *a;

    int uniqueIndex = 0;
    for (int index = 1; index < size; ++index)
    {
        type& previousElement = aRef[index - 1];
        type& currentElement = aRef[index];
        if (previousElement != currentElement)
        {
            aRef[++uniqueIndex] = currentElement;
        }
    }
    *uniqueElementsNum = uniqueIndex + 1;
}
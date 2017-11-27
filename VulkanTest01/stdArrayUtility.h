#pragma once

#include<array>
#include<assert.h>

#if _DEBUG
#define NTF_ARRAY_FIXED_DEBUG 1
#endif//#if _DEBUG

///@todo: replace std::array<> uses with this
#define NTF_ARRAY_FIXED_PARENT std::array<T, kSizeMax>
template<class T, size_t kSizeMax>
class ArrayFixed:public NTF_ARRAY_FIXED_PARENT
{
    typedef ArrayFixed<T, kSizeMax> ThisDataType;

    size_t m_sizeCurrent;///<must be manually managed with methods
#if NTF_ARRAY_FIXED_DEBUG
    bool m_sizeCurrentSet;
#endif//#if NTF_ARRAY_FIXED_DEBUG
public:
    ArrayFixed()
    {
#if NTF_ARRAY_FIXED_DEBUG
        m_sizeCurrentSet = false;
#endif//#if NTF_ARRAY_FIXED_DEBUG
    }
    ArrayFixed(const size_t size):
    m_sizeCurrent(size)
    {
#if NTF_ARRAY_FIXED_DEBUG
        m_sizeCurrentSet = true;
#endif//#if NTF_ARRAY_FIXED_DEBUG
    }

    size_type size() const noexcept
    {
#if NTF_ARRAY_FIXED_DEBUG
        assert(m_sizeCurrentSet);
#endif//#if NTF_ARRAY_FIXED_DEBUG
        assert(m_sizeCurrent <= kSizeMax);
        return m_sizeCurrent;
    }
    void size(const size_t size)
    {
#if NTF_ARRAY_FIXED_DEBUG
        m_sizeCurrentSet = true;
#endif//#if NTF_ARRAY_FIXED_DEBUG
        m_sizeCurrent = size;

        assert(m_sizeCurrent <= kSizeMax);
        assert(m_sizeCurrent >= 0);
    }
    ///@todo: unit tests
    void sizeIncrement()
    {
#if NTF_ARRAY_FIXED_DEBUG
        assert(m_sizeCurrentSet);
#endif//#if NTF_ARRAY_FIXED_DEBUG
        ++m_sizeCurrent;
        assert(m_sizeCurrent <= kSizeMax);
        assert(m_sizeCurrent >= 0);
    }
    ///@todo: unit tests
    void sizeDecrement()
    {
#if NTF_ARRAY_FIXED_DEBUG
        assert(m_sizeCurrentSet);
#endif//#if NTF_ARRAY_FIXED_DEBUG
        --m_sizeCurrent;
        assert(m_sizeCurrent <= kSizeMax);
        assert(m_sizeCurrent >= 0);
    }
    ///@todo: unit tests
    void Push(const T& item)
    {
        size_t indexForItem = m_sizeCurrent;
        sizeIncrement();
        operator[](indexForItem) = item;
    }

    const T* GetAddressOfUnderlyingArray() const
    {
        return &NTF_ARRAY_FIXED_PARENT::operator[](0);
    }
    T* GetAddressOfUnderlyingArray()
    {
        return const_cast<T*>(static_cast<const ThisDataType*>(this)->GetAddressOfUnderlyingArray());
    }
    const_reference GetChecked(const size_type pos) const
    {
        return std::array<T, kSizeMax>::operator[](pos);
    }
    reference GetChecked(const size_type pos)
    {
#if NTF_ARRAY_FIXED_DEBUG
        assert(m_sizeCurrentSet);
#endif//#if NTF_ARRAY_FIXED_DEBUG
        assert(pos < m_sizeCurrent);
        assert(pos < kSizeMax);
        assert(m_sizeCurrent <= kSizeMax);
        assert(m_sizeCurrent >= 0);

        return const_cast<reference>(static_cast<const ThisDataType*>(this)->GetChecked(pos));
    }
    size_t GetLastValidIndex() const
    {
#if NTF_ARRAY_FIXED_DEBUG
        assert(m_sizeCurrentSet);
#endif//#if NTF_ARRAY_FIXED_DEBUG
        assert(m_sizeCurrent <= kSizeMax);
        assert(m_sizeCurrent >= 0);
        return m_sizeCurrent - 1;
    }
    size_t GetOneAfterLastValidIndex() const
    {
#if NTF_ARRAY_FIXED_DEBUG
        assert(m_sizeCurrentSet);
#endif//#if NTF_ARRAY_FIXED_DEBUG
        assert(m_sizeCurrent <= kSizeMax);
        assert(m_sizeCurrent >= 0);
        return GetLastValidIndex() + 1;
    }

    reference operator[](size_type pos) 
    {
        return GetChecked(pos);
    }
    const_reference operator[](size_type pos) const 
    {
        return GetChecked(pos);
    }

    reference at(size_type pos)  
    {
        return GetChecked(pos);
    }
    const_reference at(size_type pos) const 
    {
        return GetChecked(pos);
    }

    reference front() 
    {
        return GetChecked(0);
    }
    const_reference front() const 
    {
        return GetChecked(0);
    }

    reference back() 
    {
        return GetChecked(GetLastValidIndex());
    }
    const_reference back() const 
    {
        return GetChecked(GetLastValidIndex());
    }

    //iterator begin() purposefully omitted because underlying std::array implementation is already correct

    iterator end() noexcept 
    {
        return iterator(GetAddressOfUnderlyingArray(),GetOneAfterLastValidIndex());
    }
    const_iterator end() const noexcept 
    {
        return const_iterator(GetAddressOfUnderlyingArray(), GetOneAfterLastValidIndex());
    }
    const_iterator cend() const noexcept 
    {
        return end();
    }

    reverse_iterator rbegin() noexcept 
    {
        return reverse_iterator(end());
    }
    const_reverse_iterator rbegin() const noexcept 
    {
        return const_reverse_iterator(end());
    }
    const_reverse_iterator crbegin() const noexcept 
    {
        return rbegin();
    }

    //iterator rend() and crend() purposefully omitted because underlying std::array implementation is already correct

    bool empty() const noexcept 
    {
        return size() == 0;
    }
};

template< class T, std::size_t N >
bool operator==(const ArrayFixed<T, N>& lhs,
                const ArrayFixed<T, N>& rhs)
{
    const size_t lhsSize = lhs.size();
    if (lhsSize != rhs.size())
    {
        return false;
    }
    else
    {
        for (size_t i = 0; i < lhsSize; ++i)
        {
            if (lhs[i] != rhs[i])
            {
                return false;
            }
        }
    }

    return true;
}

template< class T, std::size_t N >
bool operator!=(const ArrayFixed<T, N>& lhs, const ArrayFixed<T, N>& rhs)
{
    return !(lhs == rhs);
}

template< class T, std::size_t N >
bool operator<(const ArrayFixed<T, N>& lhs, const ArrayFixed<T, N>& rhs)
{
    return false;
}

template< class T, std::size_t N >
bool operator<=(const ArrayFixed<T, N>& lhs, const ArrayFixed<T, N>& rhs)
{
    return false;
}

template< class T, std::size_t N >
bool operator>(const ArrayFixed<T, N>& lhs, const ArrayFixed<T, N>& rhs)
{
    return false;
}

template< class T, std::size_t N >
bool operator>=(const ArrayFixed<T, N>& lhs, const ArrayFixed<T, N>& rhs)
{
    return false;
}

template< size_t I, class T, size_t N >
T& get(ArrayFixed<T, N>& a) noexcept
{
    //if the implementation of this ever changes, update unit tests to cover the new implementation
    return a.GetChecked(I);
}

template< size_t I, class T, size_t N >
T&& get(ArrayFixed<T, N>&& a) noexcept
{
    //if the implementation of this ever changes, update unit tests to cover the new implementation
    return a.GetChecked(I);
}

template< size_t I, class T, size_t N >
const T& get(ArrayFixed<T, N>& a) noexcept
{
    //if the implementation of this ever changes, update unit tests to cover the new implementation
    return a.GetChecked(I);
}

template< size_t I, class T, size_t N >
const T&& get(ArrayFixed<T, N>&& a) noexcept
{
    //if the implementation of this ever changes, update unit tests to cover the new implementation
    return a.GetChecked(I);
}


template<class T, size_t size>
size_t ArrayInBytesMaxSize(const std::array<T, size> &a)
{
    return a.size()*sizeof(a[0]);
}

template<class T, size_t size>
void SortAndRemoveDuplicatesFromArray(std::array<T, size>*const a, size_t*const uniqueElementsNum)
{
    assert(a);
    assert(uniqueElementsNum);

    std::sort(a->begin(), a->end());
    RemoveDuplicatesFromSortedArray(a, uniqueElementsNum);
}

template<class T, size_t size>
void RemoveDuplicatesFromSortedArray(std::array<T, size>*const a, size_t*const uniqueElementsNum)
{
    assert(a);
    assert(uniqueElementsNum);

    std::array<T, size>& aRef = *a;

    int uniqueIndex = 0;
    for (int index = 1; index < size; ++index)
    {
        T& previousElement = aRef[index - 1];
        T& currentElement = aRef[index];
        if (previousElement != currentElement)
        {
            aRef[++uniqueIndex] = currentElement;
        }
    }
    *uniqueElementsNum = uniqueIndex + 1;
}
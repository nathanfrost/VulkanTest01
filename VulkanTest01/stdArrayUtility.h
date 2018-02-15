#pragma once

#include<algorithm>
#include<assert.h>
#include<initializer_list>
#include<string.h>

#if _DEBUG
#define NTF_ARRAY_FIXED_DEBUG 1
#endif//#if _DEBUG

template<class T, size_t kSizeMax>
class ArraySafe
{
    typedef ArraySafe<T, kSizeMax> ThisDataType;
    typedef T* iterator;
    typedef const T* const_iterator;
    typedef T& reference;
    typedef const T& const_reference;
    typedef size_t size_type;

    T m_array[kSizeMax];
    size_t m_sizeCurrent;///<must be manually managed with methods
#if NTF_ARRAY_FIXED_DEBUG
    bool m_sizeCurrentSet;
#endif//#if NTF_ARRAY_FIXED_DEBUG
public:
    ArraySafe()
    {
#if NTF_ARRAY_FIXED_DEBUG
        m_sizeCurrentSet = false;
#endif//#if NTF_ARRAY_FIXED_DEBUG
    }
    ArraySafe(const size_t size):
    m_sizeCurrent(size)
    {
#if NTF_ARRAY_FIXED_DEBUG
        m_sizeCurrentSet = true;
#endif//#if NTF_ARRAY_FIXED_DEBUG
    }
    ArraySafe(const std::initializer_list<T>& initializerList)
    {
#if NTF_ARRAY_FIXED_DEBUG
        m_sizeCurrentSet = true;
#endif//#if NTF_ARRAY_FIXED_DEBUG
        MemcpyFromStart(initializerList.begin(), initializerList.size()*sizeof(T));
    }
    template<size_t kSizeMaxOther>
    void Copy(const ArraySafe<T, kSizeMaxOther>& arrayFixedOther)
    {
        MemcpyFromStart(arrayFixedOther.GetAddressOfUnderlyingArray(), arrayFixedOther.SizeCurrentInBytes());
    }

    void MemcpyFromStart(const T*const input, const size_t inputBytesNum)
    {
        assert(input);
        assert(inputBytesNum > 0);
        assert(inputBytesNum <= SizeMaxInBytes());
        assert(inputBytesNum % sizeof(T) == 0);

        memcpy(GetAddressOfUnderlyingArray(), input, inputBytesNum);
        size(inputBytesNum / sizeof(T));
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
    void sizeDecrement()
    {
#if NTF_ARRAY_FIXED_DEBUG
        assert(m_sizeCurrentSet);
#endif//#if NTF_ARRAY_FIXED_DEBUG
        --m_sizeCurrent;
        assert(m_sizeCurrent <= kSizeMax);
        assert(m_sizeCurrent >= 0);
    }
    size_t SizeCurrentInBytes() const
    {
        return size()*sizeof(T);
    }
    size_t SizeMaxInBytes() const
    {
        return kSizeMax*sizeof(T);
    }
    void Push(const T& item)
    {
        size_t indexForItem = m_sizeCurrent;
        sizeIncrement();
        operator[](indexForItem) = item;
    }

    const T* data() const
    {
        return GetAddressOfUnderlyingArray();
    }
    T* data()
    {
        return GetAddressOfUnderlyingArray();
    }
    const T* GetAddressOfUnderlyingArray() const
    {
        return &m_array[0];
    }
    T* GetAddressOfUnderlyingArray()
    {
        return const_cast<T*>(static_cast<const ThisDataType*>(this)->GetAddressOfUnderlyingArray());
    }
    const_reference GetChecked(const size_type pos) const
    {
#if NTF_ARRAY_FIXED_DEBUG
        assert(m_sizeCurrentSet);
#endif//#if NTF_ARRAY_FIXED_DEBUG
        assert(pos < m_sizeCurrent);
        assert(pos < kSizeMax);
        assert(m_sizeCurrent <= kSizeMax);
        assert(m_sizeCurrent >= 0);
        return m_array[pos];
    }
    reference GetChecked(const size_type pos)
    {
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

    iterator begin() noexcept
    {
        return GetAddressOfUnderlyingArray();
    }
    const_iterator begin() const noexcept
    {
        return GetAddressOfUnderlyingArray();
    }
    const_iterator cbegin() const noexcept
    {
        return end();
    }

    iterator end() noexcept 
    {
        return const_cast<iterator>(static_cast<const ThisDataType*>(this)->end());
    }
    const_iterator end() const noexcept 
    {
        return const_iterator(GetAddressOfUnderlyingArray() + GetOneAfterLastValidIndex());
    }
    const_iterator cend() const noexcept 
    {
        return end();
    }

    //reverse_iterator rbegin() noexcept 
    //{
    //    return reverse_iterator(end());
    //}
    //const_reverse_iterator rbegin() const noexcept 
    //{
    //    return const_reverse_iterator(end());
    //}
    //const_reverse_iterator crbegin() const noexcept 
    //{
    //    return rbegin();
    //}
    //iterator rend() and crend() not implemented

    bool empty() const noexcept 
    {
        return m_sizeCurrent == 0;
    }
};

template< class T, std::size_t N >
bool operator==(const ArraySafe<T, N>& lhs,
                const ArraySafe<T, N>& rhs)
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
bool operator!=(const ArraySafe<T, N>& lhs, const ArraySafe<T, N>& rhs)
{
    return !(lhs == rhs);
}

template< class T, std::size_t N >
bool operator<(const ArraySafe<T, N>& lhs, const ArraySafe<T, N>& rhs)
{
    return false;
}

template< class T, std::size_t N >
bool operator<=(const ArraySafe<T, N>& lhs, const ArraySafe<T, N>& rhs)
{
    return false;
}

template< class T, std::size_t N >
bool operator>(const ArraySafe<T, N>& lhs, const ArraySafe<T, N>& rhs)
{
    return false;
}

template< class T, std::size_t N >
bool operator>=(const ArraySafe<T, N>& lhs, const ArraySafe<T, N>& rhs)
{
    return false;
}

template< size_t I, class T, size_t N >
T& get(ArraySafe<T, N>& a) noexcept
{
    //if the implementation of this ever changes, update unit tests to cover the new implementation
    return a.GetChecked(I);
}

template< size_t I, class T, size_t N >
T&& get(ArraySafe<T, N>&& a) noexcept
{
    //if the implementation of this ever changes, update unit tests to cover the new implementation
    return a.GetChecked(I);
}

template< size_t I, class T, size_t N >
const T& get(ArraySafe<T, N>& a) noexcept
{
    //if the implementation of this ever changes, update unit tests to cover the new implementation
    return a.GetChecked(I);
}

template< size_t I, class T, size_t N >
const T&& get(ArraySafe<T, N>&& a) noexcept
{
    //if the implementation of this ever changes, update unit tests to cover the new implementation
    return a.GetChecked(I);
}

template<class T, size_t size>
void SortAndRemoveDuplicatesFromArray(ArraySafe<T, size>*const a)
{
    assert(a);
    std::sort(a->begin(), a->end());
    RemoveDuplicatesFromSortedArray(a);
}

template<class T, size_t size>
void RemoveDuplicatesFromSortedArray(ArraySafe<T, size>*const a)
{
    assert(a);
    ArraySafe<T, size>& aRef = *a;

    int uniqueIndex = 0;
    const size_t currentSize = a->size();
    for (size_t index = 1; index < currentSize; ++index)
    {
        T& previousElement = aRef[index - 1];
        T& currentElement = aRef[index];
        if (previousElement != currentElement)
        {
            aRef[++uniqueIndex] = currentElement;
        }
    }
    a->size(uniqueIndex + 1);
}
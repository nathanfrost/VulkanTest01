#pragma once

#include<algorithm>
#include<assert.h>
#include<initializer_list>
#include<string.h>

#if !NDEBUG
#define NTF_ARRAY_SAFE_DEBUG 1 ///@todo: rename NTF_ARRAY_SAFE_DEBUG
#endif//#if _DEBUG

inline void* AlignedMalloc(size_t size, size_t alignment)
{
    assert(size > 0);
    assert(alignment > 0);

    void* ret = _aligned_malloc(size, alignment);
    assert(ret);
    assert((uintptr_t)(ret) % (uintptr_t)(alignment) == 0);
    return ret;
}

inline void AlignedFree(void* mem)
{
    assert(mem);
    _aligned_free(mem);
}

template<class T, size_t kSize>
class VectorSafe;

template<class T>
class VectorSafeRef
{
public:
    typedef VectorSafeRef<T> ThisDataType;
    typedef T* iterator;
    typedef const T* const_iterator;
    typedef T& reference;
    typedef const T& const_reference;
    typedef size_t size_type;

private:
    void SetArray(T* p)
    {
        assert(p);
        m_array = p;
#if NTF_ARRAY_SAFE_DEBUG
        m_arraySet = true;
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }
    void SetSizeCurrent(const size_t sizeCurrent)
    {
        assert(m_sizeCurrent);
        
        *m_sizeCurrent = sizeCurrent;
#if NTF_ARRAY_SAFE_DEBUG
        assert(m_sizeCurrentSet);
        *m_sizeCurrentSet = true;
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }
    void SetSizeCurrentPtr(size_t* const sizeCurrentPtr
#if NTF_ARRAY_SAFE_DEBUG
        ,bool* sizeCurrentSetPtr
#endif//#if NTF_ARRAY_SAFE_DEBUG
        )
    {
        assert(sizeCurrentPtr);
        m_sizeCurrent = sizeCurrentPtr;

#if NTF_ARRAY_SAFE_DEBUG
        assert(sizeCurrentSetPtr);
        m_sizeCurrentSet = sizeCurrentSetPtr;
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }
    void SetSizeMax(const size_t sizeMax)
    {
        assert(sizeMax > 0);
#if NTF_ARRAY_SAFE_DEBUG
        m_sizeMax = sizeMax;
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }

    ///@todo: reduce code duplication with VectorSafe
    T* m_array;
    size_t* m_sizeCurrent;
#if NTF_ARRAY_SAFE_DEBUG
    bool* m_sizeCurrentSet;
    size_t m_sizeMax;
    bool m_arraySet;
#endif//#if NTF_ARRAY_SAFE_DEBUG

public:
//this would be a non-const pointer to non-const -- this class is for const pointer to non-const
//    VectorSafeRef()
//    {
//#if NTF_ARRAY_SAFE_DEBUG
//        m_arraySet = m_sizeCurrentSet = false;
//        m_sizeMax = 0;
//#endif//#if NTF_ARRAY_SAFE_DEBUG
//    }
    ///@todo
    //VectorSafeRef(T*const pointer, const std::initializer_list<T>& initializerList, const size_t maxSize)
    //{
    //    SetArray(pointer);
    //    SetSizeMax(maxSize);
    //    MemcpyFromStart(initializerList.begin(), initializerList.size()*sizeof(T));
    //    AssertValid();
    //}
    template<size_t kSizeMax>
    VectorSafeRef(VectorSafe<T, kSizeMax>*const arraySafe)
    {
        assert(arraySafe);

        SetSizeCurrentPtr(
            &arraySafe->m_sizeCurrent
#if NTF_ARRAY_SAFE_DEBUG
            ,&arraySafe->m_sizeCurrentSet
#endif//#if NTF_ARRAY_SAFE_DEBUG
            );
        SetSizeMax(arraySafe->SizeMax());
        SetArray(arraySafe->begin());
    }

    ///@todo: unit tests
    VectorSafeRef(T*const p, const size_t sizeMax, const size_t alignment)
    {
        assert((uintptr_t)p % alignment == 0);
        assert(sizeMax % alignment == 0);

        SetSizeMax(sizeMax);
        SetArray(p);
    }

    ///@todo: unit tests
    VectorSafeRef()
    {
        Reset();
    }
    ///@todo: unit tests
    void Reset()
    {
        m_array = nullptr;
        m_sizeCurrent = 0;

#if NTF_ARRAY_SAFE_DEBUG
        m_sizeCurrentSet = nullptr;
        m_arraySet = false;
        m_sizeMax = 0;
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }

    ///@todo: AssertCurrentSufficient() //m_sizeMax - m_sizeCurrent >= elementsNum
    void AssertSufficient(const size_t elementsNum) const
    {
#if NTF_ARRAY_SAFE_DEBUG
        AssertValid();
        assert(m_sizeMax >= elementsNum);
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }

    void AssertValid() const
    {
#if NTF_ARRAY_SAFE_DEBUG
        assert(m_arraySet);
        assert(m_sizeCurrentSet);
        assert(m_sizeCurrent);
        if (*m_sizeCurrentSet)
        {
            assert(*m_sizeCurrent <= m_sizeMax);
        }
        
        assert(m_sizeMax > 0);
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }

    ///@todo: have to pass in number of bytes explicitly
    //void Copy(const VectorSafeRef<T>& vectorSafeOther)
    //{
    //    MemcpyFromStart(vectorSafeOther.GetAddressOfUnderlyingArray(), vectorSafeOther.SizeCurrentInBytes());
    //}

    void MemcpyFromStart(const T*const input, const size_t inputBytesNum)
    {
        AssertValid();
#if NTF_ARRAY_SAFE_DEBUG
        assert(inputBytesNum <= SizeMaxInBytes());
#endif//#if NTF_ARRAY_SAFE_DEBUG

        assert(input);
        assert(inputBytesNum > 0);
        assert(inputBytesNum % sizeof(T) == 0);

        memcpy(GetAddressOfUnderlyingArray(), input, inputBytesNum);
        SetSizeCurrent(inputBytesNum / sizeof(T));
        AssertValid();
    }

    size_type size() const noexcept
    {
        AssertValid();
        return *m_sizeCurrent;
    }
    void size(const size_t size)
    {
        SetSizeCurrent(size);
        AssertValid();
    }
    ///@todo: unit tests
    void sizeIncrement()
    {
        AssertValid();
        ++(*m_sizeCurrent);
        AssertValid();
    }
    void sizeDecrement()
    {
        AssertValid();
        --(*m_sizeCurrent);
        AssertValid();
    }
    size_t SizeCurrentInBytes() const
    {
        return size()*sizeof(T);
    }
    size_t SizeMaxInBytes() const
    {
        AssertValid();
        return m_sizeMax*sizeof(T);
    }
    void Push(const T& item)
    {
        AssertValid();
        const size_t indexForItem = *m_sizeCurrent;
        sizeIncrement();
        m_array[indexForItem] = item;
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
        AssertValid();
        return &m_array[0];
    }
    T* GetAddressOfUnderlyingArray()
    {
        return const_cast<T*>(static_cast<const ThisDataType*>(this)->GetAddressOfUnderlyingArray());
    }
    const_reference GetChecked(const size_type pos) const
    {
        AssertValid();
        assert(pos < *m_sizeCurrent);
        return m_array[pos];
    }
    reference GetChecked(const size_type pos)
    {
        return const_cast<reference>(static_cast<const ThisDataType*>(this)->GetChecked(pos));
    }
    size_t GetLastValidIndex() const
    {
        AssertValid();
        return *m_sizeCurrent - 1;
    }
    size_t GetOneAfterLastValidIndex() const
    {
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
        AssertValid();
        return *m_sizeCurrent == 0;
    }
};

///NOTE: the caller is responsible for freeing this memory with AlignedFree(); VectorSafeRef is only a reference
template<class T>
void AlignedMalloc(VectorSafeRef<T>*const vectorSafeRef, size_t size, size_t alignment)
{
    assert(vectorSafeRef);
    *vectorSafeRef = VectorSafeRef<T>(reinterpret_cast<T*>(AlignedMalloc(size, alignment)), size, alignment);
}

template<class T>
void AlignedFree(VectorSafeRef<T>*const vectorSafeRef)
{
    assert(vectorSafeRef);
    AlignedFree(vectorSafeRef->data());
    vectorSafeRef->Reset();
}


template<class T>
class ConstVectorSafeRef///@todo: rename VectorSafeRefConst
{
public:
    typedef ConstVectorSafeRef<T> ThisDataType;
    typedef const T* const_iterator;
    typedef const T& const_reference;
    typedef size_t size_type;

private:
    void SetArray(const T*const p)
    {
        assert(p);
        m_array = p;
    }
    void SetSizeMax(const size_t sizeMax)
    {
        assert(sizeMax > 0);
        m_sizeMax = sizeMax;
    }

    ///@todo: reduce code duplication with VectorSafe
    const T* m_array;
    size_t m_sizeMax;

public:
    ConstVectorSafeRef(const T*const pointer, const size_t sizeMax)
    {
        SetArray(pointer);
        SetSizeMax(sizeMax);
        AssertValid();
    }
    template<size_t kSizeMax>
    ConstVectorSafeRef(const VectorSafe<T, kSizeMax>& arraySafe)
    {
        SetArray(arraySafe.begin());
        SetSizeMax(arraySafe.size());
        AssertValid();
    }

    void AssertValid() const
    {
#if NTF_ARRAY_SAFE_DEBUG
        assert(m_array);
        assert(m_sizeMax > 0);
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }

    size_type size() const noexcept
    {
        AssertValid();
        return m_sizeMax;
    }
    size_t SizeMaxInBytes() const
    {
        return size()*sizeof(T);
    }
    const T* data() const
    {
        return GetAddressOfUnderlyingArray();
    }
    const T* GetAddressOfUnderlyingArray() const
    {
        AssertValid();
        return &m_array[0];
    }
    const_reference GetChecked(const size_type pos) const
    {
        AssertValid();
        assert(pos < m_sizeMax);
        return m_array[pos];
    }
    size_t GetLastValidIndex() const
    {
        return m_sizeMax - 1;
    }
    size_t GetOneAfterLastValidIndex() const
    {
        return GetLastValidIndex() + 1;
    }
    const_reference operator[](size_type pos) const
    {
        return GetChecked(pos);
    }
    const_reference at(size_type pos) const
    {
        return GetChecked(pos);
    }

    const_reference front() const
    {
        return GetChecked(0);
    }

    const_reference back() const
    {
        return GetChecked(GetLastValidIndex());
    }

    const_iterator begin() const noexcept
    {
        return GetAddressOfUnderlyingArray();
    }
    const_iterator cbegin() const noexcept
    {
        return end();
    }

    const_iterator end() const noexcept
    {
        return const_iterator(GetAddressOfUnderlyingArray() + GetOneAfterLastValidIndex());
    }
    const_iterator cend() const noexcept
    {
        return end();
    }

    //const_reverse_iterator rbegin() const noexcept 
    //{
    //    return const_reverse_iterator(end());
    //}
    //const_reverse_iterator crbegin() const noexcept 
    //{
    //    return rbegin();
    //}

    bool empty() const noexcept
    {
        AssertValid();
        return m_sizeMax == 0;
    }
};

template<class T, size_t kSizeMax>
class VectorSafe///@todo: rename VectorSafe
{
public:
    typedef VectorSafe<T, kSizeMax> ThisDataType;
    typedef T* iterator;
    typedef const T* const_iterator;
    typedef T& reference;
    typedef const T& const_reference;
    typedef size_t size_type;

private:
    T m_array[kSizeMax];
    size_t m_sizeCurrent;///<must be manually managed with methods
#if NTF_ARRAY_SAFE_DEBUG
    bool m_sizeCurrentSet;
#endif//#if NTF_ARRAY_SAFE_DEBUG

    friend class VectorSafeRef<T>;
    friend class ConstVectorSafeRef<T>;

    void AssertValid() const
    {
#if NTF_ARRAY_SAFE_DEBUG
        assert(m_sizeCurrentSet);
        assert(m_sizeCurrent <= kSizeMax);

        static_assert(kSizeMax > 0, "VectorSafe<T>::kSizeMax must be greater than 0");
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }

public:
    VectorSafe()
    {
#if NTF_ARRAY_SAFE_DEBUG
        m_sizeCurrentSet = false;
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }
    VectorSafe(const size_t sz)
    {
        size(sz);
    }
    VectorSafe(const std::initializer_list<T>& initializerList)
    {
        MemcpyFromStart(initializerList.begin(), initializerList.size()*sizeof(T));
    }
    VectorSafe(ConstVectorSafeRef<T> r)
    {
        MemcpyFromStart(r.begin(), r.size()*sizeof(T));
    }
    template<class T, size_t kSizeMax>
    operator VectorSafeRef<T>()
    {
        return VectorSafeRef(this);
    }

    template<size_t kSizeMaxOther>
    void Copy(const VectorSafe<T, kSizeMaxOther>& vectorSafeOther)
    {
        MemcpyFromStart(vectorSafeOther.GetAddressOfUnderlyingArray(), vectorSafeOther.SizeCurrentInBytes());
    }

    void MemcpyFromStart(const T*const input, const size_t inputBytesNum)///@todo: rename to make obvious that the semantics of this is to CLEAR the Array and replace with the contents of input
    {
        assert(input);
        assert(inputBytesNum > 0);
        size(inputBytesNum / sizeof(T));
        assert(inputBytesNum <= SizeMaxInBytes());
        assert(inputBytesNum % sizeof(T) == 0);

        memcpy(GetAddressOfUnderlyingArray(), input, inputBytesNum);
        AssertValid();
    }

    size_type size() const noexcept
    {
        AssertValid();
        return m_sizeCurrent;
    }
    void size(const size_t size)
    {
#if NTF_ARRAY_SAFE_DEBUG
        m_sizeCurrentSet = true;
#endif//#if NTF_ARRAY_SAFE_DEBUG
        m_sizeCurrent = size;

        AssertValid();
    }
    ///@todo: unit tests
    void sizeIncrement()
    {
        AssertValid();
        ++m_sizeCurrent;
        AssertValid();
    }
    void sizeDecrement()
    {
        AssertValid();
        --m_sizeCurrent;
        AssertValid();
    }
    size_t SizeCurrentInBytes() const
    {
        return size()*sizeof(T);
    }
    size_t SizeMax() const
    {
        return kSizeMax;
    }
    size_t SizeMaxInBytes() const
    {
        return SizeMax()*sizeof(T);
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
        AssertValid();
        assert(pos < m_sizeCurrent);
        assert(pos < kSizeMax);
        return m_array[pos];
    }
    reference GetChecked(const size_type pos)
    {
        return const_cast<reference>(static_cast<const ThisDataType*>(this)->GetChecked(pos));
    }
    size_t GetLastValidIndex() const
    {
        AssertValid();
        return m_sizeCurrent - 1;
    }
    size_t GetOneAfterLastValidIndex() const
    {
        AssertValid();
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
        AssertValid();
        return m_sizeCurrent == 0;
    }
};


template< class T, std::size_t N >
bool operator==(const VectorSafe<T, N>& lhs,
                const VectorSafe<T, N>& rhs)
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
bool operator!=(const VectorSafe<T, N>& lhs, const VectorSafe<T, N>& rhs)
{
    return !(lhs == rhs);
}

//purposefully don't overload comparison operators for VectorSafe

template< size_t I, class T, size_t N >
T& get(VectorSafe<T, N>& a) noexcept
{
    //if the implementation of this ever changes, update unit tests to cover the new implementation
    return a.GetChecked(I);
}

template< size_t I, class T, size_t N >
T&& get(VectorSafe<T, N>&& a) noexcept
{
    //if the implementation of this ever changes, update unit tests to cover the new implementation
    return a.GetChecked(I);
}

template< size_t I, class T, size_t N >
const T& get(VectorSafe<T, N>& a) noexcept
{
    //if the implementation of this ever changes, update unit tests to cover the new implementation
    return a.GetChecked(I);
}

template< size_t I, class T, size_t N >
const T&& get(VectorSafe<T, N>&& a) noexcept
{
    //if the implementation of this ever changes, update unit tests to cover the new implementation
    return a.GetChecked(I);
}

template<class T, size_t size>
void SortAndRemoveDuplicatesFromVectorSafe(VectorSafe<T, size>*const a)
{
    assert(a);
    std::sort(a->begin(), a->end());
    RemoveDuplicatesFromSortedVectorSafe(a);
}

template<class T, size_t size>
void RemoveDuplicatesFromSortedVectorSafe(VectorSafe<T, size>*const a)
{
    assert(a);
    VectorSafe<T, size>& aRef = *a;

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

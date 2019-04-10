#pragma once

#include<algorithm>
#include<assert.h>
#include"ntf_compileTimeDefines.h"
#include"MemoryUtil.h"
#include<initializer_list>
#include<string.h>
#include<windows.h>//for DWORD

#define NTF_REF(ptrIdentifier, refIdentifier) assert(ptrIdentifier); auto& refIdentifier = *ptrIdentifier

inline void Fopen(FILE**const f, const char*const filename, const char*const mode)
{
    assert(f);
    assert(filename && filename[0]);
    assert(mode && mode[0]);
    
    const errno_t fopen_sRet = fopen_s(f, filename, mode);
    
    assert(*f);
    assert(fopen_sRet == 0);
}

inline void Fclose(FILE*const f)
{
    assert(f);
    const int fcloseRet = fclose(f);
    assert(fcloseRet == 0);
}

/*  users of this translation unit should directly use this function only for non-array elements; array elements should reside in an array 
    with its own assert-safe Fwrite method*/
inline void Fwrite(FILE*const file, const void*const buf, const size_t sizeOfElement, const size_t elementsNum)
{
    assert(file);
    assert(buf);
    assert(sizeOfElement);
    assert(elementsNum);

    const size_t fwriteRet = fwrite(buf, sizeOfElement, elementsNum, file);
    assert(fwriteRet == elementsNum);
}

/*  users of this translation unit should directly use this function only for non-array elements; array elements should reside in an array 
    with its own assert-safe Fread method */
inline void Fread(FILE*const file, void*const buf, const size_t sizeOfElement, const size_t elementsNum)
{
    assert(file);
    assert(buf);
    assert(sizeOfElement);
    assert(elementsNum);

    const size_t freadRet = fread(buf, sizeOfElement, elementsNum, file);
    assert(freadRet == elementsNum);
}

template<class T, size_t kSize>
class VectorSafe;

template<class T, size_t kSize>
class ArraySafe;

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
    }
    void SetSizeCurrentPtr(size_t* const sizeCurrentPtr)
    {
        assert(sizeCurrentPtr);
        m_sizeCurrent = sizeCurrentPtr;
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
    VectorSafeRef(VectorSafe<T, kSizeMax>*const vectorSafe)
    {
        assert(vectorSafe);

        SetSizeCurrentPtr(&vectorSafe->m_sizeCurrent);
        SetSizeMax(vectorSafe->SizeMax());
        SetArray(vectorSafe->begin());
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
        assert(m_sizeCurrent);
        assert(*m_sizeCurrent <= m_sizeMax);       
        assert(m_sizeMax > 0);
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }

    ///@todo: have to pass in number of bytes explicitly
    //void Copy(const VectorSafeRef<T>& vectorSafeOther)
    //{
    //    MemcpyFromStart(vectorSafeOther.GetAddressOfUnderlyingArray(), vectorSafeOther.SizeCurrentInBytes());
    //}

    void MemcpyFromStart(const void*const input, const size_t inputBytesNum)
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
    ///@todo: unit tests
    void MemcpyFromFread(FILE*const f, const size_t elementsNum)
    {
        AssertValid();
        assert(f);
        assert(elementsNum > 0);
#if NTF_ARRAY_SAFE_DEBUG
        assert(elementsNum <= m_sizeMax);
#endif//#if NTF_ARRAY_SAFE_DEBUG
        Fread(f, &m_array[0], sizeof(T), elementsNum);
        SetSizeCurrent(elementsNum);
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


///@todo: unit tests for entire class
template<class T>
class ArraySafeRef
{
public:
    typedef ArraySafeRef<T> ThisDataType;
    typedef T* iterator;
    typedef const T* const_iterator;
    typedef T& reference;
    typedef const T& const_reference;
    typedef size_t size_type;

private:
    void SetArray(T* p)
    {
        m_array = p;
    }
    void SetSizeMax(const size_t sizeMax)
    {
#if NTF_ARRAY_SAFE_DEBUG
        m_sizeMax = sizeMax;
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }

    ///@todo: reduce code duplication with VectorSafe
    T* m_array;
#if NTF_ARRAY_SAFE_DEBUG
    size_t m_sizeMax;
#endif//#if NTF_ARRAY_SAFE_DEBUG

public:
    ///@todo
    //ArraySafeRef(T*const pointer, const std::initializer_list<T>& initializerList, const size_t maxSize)
    //{
    //    SetArray(pointer);
    //    SetSizeMax(maxSize);
    //    MemcpyFromStart(initializerList.begin(), initializerList.size()*sizeof(T));
    //    AssertValid();
    //}

    template<size_t kSizeMax>
    ArraySafeRef(VectorSafe<T, kSizeMax>*const vectorSafe)
    {
        assert(vectorSafe);
        SetSizeMax(vectorSafe->size());
        SetArray(vectorSafe->begin());
    }
    ArraySafeRef(VectorSafeRef<T>*const vectorSafe)
    {
        assert(vectorSafe);
        SetSizeMax(vectorSafe->size());
        SetArray(vectorSafe->begin());
    }

    template<size_t kSizeMax>
    ArraySafeRef(ArraySafe<T, kSizeMax>*const arraySafe)
    {
        assert(arraySafe);
        SetSizeMax(arraySafe->size());
        SetArray(arraySafe->begin());
    }

    ///@todo: unit tests
    ArraySafeRef(T*const p, const size_t sizeMax)
    {
        SetSizeMax(sizeMax);
        SetArray(p);
    }

    ///@todo: unit tests
    ArraySafeRef(T*const p, const size_t sizeMax, const size_t alignment)
    {
        assert((uintptr_t)p % alignment == 0);
        assert(sizeMax % alignment == 0);

        SetSizeMax(sizeMax);
        SetArray(p);
    }

    ///@todo: unit tests
    ArraySafeRef()
    {
        Reset();
    }
    ///@todo: unit tests
    ///<use Reset() to set this array to null, not this function
    void SetArray(T* p, const size_t sizeMax)
    {
        SetArray(p);
        SetSizeMax(sizeMax);
        AssertValid();
    }
    ///@todo: unit tests
    void SetArray(FILE*const f, const size_t elementsNum)
    {
        AssertValid();//assumes correctly initialized ArraySafe reference
        assert(f);
        assert(elementsNum > 0);
#if NTF_ARRAY_SAFE_DEBUG
        assert(elementsNum <= m_sizeMax);
#endif//#if NTF_ARRAY_SAFE_DEBUG
        Fread(f, m_array, sizeof(T), elementsNum);
    }
    ///@todo: unit tests
    void Reset()
    {
        m_array = nullptr;

#if NTF_ARRAY_SAFE_DEBUG
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
        assert(m_sizeMax > 0);
        assert(m_array);
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }

    ///@todo: have to pass in number of bytes explicitly
    //void Copy(const ArraySafeRef<T>& vectorSafeOther)
    //{
    //    MemcpyFromStart(vectorSafeOther.GetAddressOfUnderlyingArray(), vectorSafeOther.SizeCurrentInBytes());
    //}
    ///@todo: totally untested; and unit tests
    void MemcpyFromFread(FILE*const f, const size_t elementsNum)
    {
        AssertValid();
        assert(f);
        assert(elementsNum > 0);
#if NTF_ARRAY_SAFE_DEBUG
        assert(elementsNum <= m_sizeMax);
#endif//#if NTF_ARRAY_SAFE_DEBUG
        Fread(f, &m_array[0], sizeof(T), elementsNum);
    }
    ///@todo: unit tests
    void MemcpyFromStart(const void*const input, const size_t inputBytesNum)
    {
        MemcpyFromIndex(input, 0, inputBytesNum);
    }
    ///@todo: unit tests
    void MemcpyFromIndex(const void*const input, const size_t index, const size_t inputBytesNum)
    {
        AssertValid();
#if NTF_ARRAY_SAFE_DEBUG
        assert(index*sizeof(T) + inputBytesNum <= SizeMaxInBytes());
#endif//#if NTF_ARRAY_SAFE_DEBUG

        assert(input);
        assert(index >= 0);
        assert(inputBytesNum > 0);
        assert(inputBytesNum % sizeof(T) == 0);

        memcpy(&GetAddressOfUnderlyingArray()[index], input, inputBytesNum);
    }

#if NTF_ARRAY_SAFE_DEBUG
    size_type size() const noexcept
    {
        AssertValid();
        return m_sizeMax;
    }
    size_t SizeMaxInBytes() const
    {
        AssertValid();
        return size()*sizeof(T);
    }
#endif//#if NTF_ARRAY_SAFE_DEBUG
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
#if NTF_ARRAY_SAFE_DEBUG
        assert(pos < size());
#endif//#if NTF_ARRAY_SAFE_DEBUG
        return m_array[pos];
    }
    reference GetChecked(const size_type pos)
    {
        return const_cast<reference>(static_cast<const ThisDataType*>(this)->GetChecked(pos));
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
class ConstVectorSafeRef
{
public:
    typedef ConstVectorSafeRef<T> ThisDataType;
    typedef const T* const_iterator;
    typedef const T& const_reference;
    typedef size_t size_type;

private:
    void SetArray(const T*const p)
    {
        m_array = p;
    }
    void SetSizeMax(const size_t sizeMax)
    {
        m_sizeMax = sizeMax;
    }

    ///@todo: reduce code duplication with VectorSafe
    const T* m_array;
    size_t m_sizeMax;

public:
    ConstVectorSafeRef()
    {
        SetArray(nullptr);
        SetSizeMax(0);
        AssertValid();
    }

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

template<class T, size_t kSize>///<@todo: rename kElementsNum
class ArraySafe
{
public:
    typedef ArraySafe<T, kSize> ThisDataType;
    typedef T* iterator;
    typedef const T* const_iterator;
    typedef T& reference;
    typedef const T& const_reference;
    typedef size_t size_type;

private:
    T m_array[kSize];
public:
    ArraySafe()
    {
    }
    ///@todo: totally untested; use and unit test
    ArraySafe(FILE*const f, const size_t elementsNum)
    {
        assert(f);
        assert(elementsNum > 0);
        assert(elementsNum <= kSize);
        Fread(f, &m_array[0], sizeof(T), elementsNum);
    }
    ArraySafe(const std::initializer_list<T>& initializerList)
    {
        MemcpyFromStart(initializerList.begin(), initializerList.size()*sizeof(T));
    }
    ArraySafe(ConstVectorSafeRef<T> r)
    {
        MemcpyFromStart(r.begin(), r.size()*sizeof(T));
    }
    template<class T, size_t kSize>
    operator ArraySafeRef<T>()
    {
        return ArraySafeRef(this);
    }

    template<size_t kSizeOther>
    void Copy(const ArraySafe<T, kSizeOther>& arraySafeOther)
    {
        MemcpyFromStart(arraySafeOther.GetAddressOfUnderlyingArray(), arraySafeOther.SizeInBytes());
    }

    void Fwrite(FILE*const f, const size_t elementsNum)
    {
        assert(f);
        assert(elementsNum > 0);
        assert(elementsNum <= kSize);
        ::Fwrite(f, &m_array[0], sizeof(m_array[0]), elementsNum);
    }

    ///@todo NTF: eliminate code duplication with VectorSafe
    ///@todo: unit test
    void Snprintf(const char*const formatString, ...)
    {
        NTF_STATIC_ASSERT(sizeof(T) == sizeof(char));//this function is intended to be used only when *this holds ASCII characters
#if NTF_ARRAY_SAFE_DEBUG
        assert(formatString);

        const char bellAsciiKeyCode = 7;
        char*const lastElement = &m_array[GetLastValidIndex()];
        *lastElement = bellAsciiKeyCode;//no "bell key" allowed -- use it as a sentinel to guard against the possibility of vsnprintf truncation
#endif//#if NTF_ARRAY_SAFE_DEBUG
        assert(strlen(formatString) > 0);

        va_list args;
        va_start(args, formatString);
        vsnprintf(&m_array[0], kSize, formatString, args);
        va_end(args);

#if NTF_ARRAY_SAFE_DEBUG
        assert(m_array);
        assert(*lastElement == bellAsciiKeyCode);//vsnprintf may have had to truncate its result to stay within the buffer
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }

    ///@todo: unit test
    void MemcpyFromStart(const void*const input, const size_t inputBytesNum)///@todo: rename to make obvious that the semantics of this is to CLEAR the Array and replace with the contents of input
    {
        assert(input);
        assert(inputBytesNum > 0);
        assert(inputBytesNum <= SizeInBytes());
        assert(inputBytesNum % sizeof(T) == 0);

        memcpy(GetAddressOfUnderlyingArray(), input, inputBytesNum);
    }

    size_type size() const noexcept
    {
        return kSize;
    }
    size_t SizeInBytes() const
    {
        return size()*sizeof(T);
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
        assert(pos < kSize);
        return m_array[pos];
    }
    reference GetChecked(const size_type pos)
    {
        return const_cast<reference>(static_cast<const ThisDataType*>(this)->GetChecked(pos));
    }
    size_t GetLastValidIndex() const
    {
        return kSize - 1;
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
    size_t m_sizeCurrent;

    friend class VectorSafeRef<T>;
    friend class ConstVectorSafeRef<T>;

    void AssertValid() const
    {
#if NTF_ARRAY_SAFE_DEBUG
        assert(m_sizeCurrent <= kSizeMax);

        static_assert(kSizeMax > 0, "VectorSafe<T>::kSizeMax must be greater than 0");
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }

public:
    VectorSafe()
    {
        size(0);
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

    ///@todo NTF: eliminate code duplication with ArraySafe
    ///@todo: unit test
    void Snprintf(const char*const formatString, ...)
    {
        NTF_STATIC_ASSERT(sizeof(T) == sizeof(char));//this function is intended to be used only when *this holds ASCII characters
#if NTF_ARRAY_SAFE_DEBUG
        assert(formatString);

        const char bellAsciiKeyCode = 7;
        char*const lastElement = &m_array[GetLastValidIndex()];
        *lastElement = bellAsciiKeyCode;//no "bell key" allowed -- use it as a sentinel to guard against the possibility of vsnprintf truncation
#endif//#if NTF_ARRAY_SAFE_DEBUG
        assert(strlen(formatString) > 0);

        va_list args;
        va_start(args, formatString);
        vsnprintf(&m_array[0], kSize, formatString, args);
        va_end(args);

#if NTF_ARRAY_SAFE_DEBUG
        assert(m_array);
        assert(*lastElement == bellAsciiKeyCode);//vsnprintf may have had to truncate its result to stay within the buffer
#endif//#if NTF_ARRAY_SAFE_DEBUG
    }

    template<size_t kSizeMaxOther>
    void Copy(const VectorSafe<T, kSizeMaxOther>& vectorSafeOther)
    {
        MemcpyFromStart(vectorSafeOther.GetAddressOfUnderlyingArray(), vectorSafeOther.SizeCurrentInBytes());
    }

    void MemcpyFromStart(const void*const input, const size_t inputBytesNum)///@todo: rename to make obvious that the semantics of this is to CLEAR the Array and replace with the contents of input
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
        m_sizeCurrent = size;
        AssertValid();
    }
    size_t SizeCurrentInBytes()
    {
        AssertValid();
        return m_sizeCurrent*sizeof(T);
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

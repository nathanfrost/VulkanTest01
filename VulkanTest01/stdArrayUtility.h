#pragma once

#include<algorithm>
#include<assert.h>
#include"ntf_compileTimeDefines.h"
#include"MemoryUtil.h"
#include<initializer_list>
#include<string.h>
#include<windows.h>//for DWORD

#pragma warning(disable : 4996)//debug builds are responsible for detecting any buffer overruns in vsprintf()

#define NTF_REF(ptrIdentifier, refIdentifier) assert(ptrIdentifier); auto& refIdentifier = *ptrIdentifier


#define STD_ARRAY_UTILITY_CONST_TYPEDEFS    \
typedef const T* const_iterator;            \
typedef const T& const_reference;           \
typedef size_t size_type;                   \
typedef T value_type

#define STD_ARRAY_UTILITY_NONCONST_TYPEDEFS \
typedef T* iterator;                        \
typedef T& reference                        \

#define STD_ARRAY_UTILITY_NONCONST_REF_PRIVATE_METHODS  \
void SetArray(T*const p)                                \
{                                                       \
    m_array = p;                                        \
}

#define STD_ARRAY_UTILITY_CONST_REF_PRIVATE_METHODS     \
void SetArray(const T*const p)                          \
{                                                       \
    m_array = p;                                        \
}

#define STD_ARRAY_UTILITY_CONST_METHODS                                                                                                             \
const T* data() const                                                                                                                               \
{                                                                                                                                                   \
    return GetAddressOfUnderlyingArray();                                                                                                           \
}                                                                                                                                                   \
const_reference GetChecked(const size_type pos) const                                                                                               \
{                                                                                                                                                   \
    AssertValid();                                                                                                                                  \
    assert(pos < size());                                                                                                                           \
    return m_array[pos];                                                                                                                            \
}                                                                                                                                                   \
const_reference operator[](size_type pos) const                                                                                                     \
{                                                                                                                                                   \
    return GetChecked(pos);                                                                                                                         \
}                                                                                                                                                   \
const_reference at(size_type pos) const                                                                                                             \
{                                                                                                                                                   \
    return GetChecked(pos);                                                                                                                         \
}                                                                                                                                                   \
const_reference front() const                                                                                                                       \
{                                                                                                                                                   \
    return GetChecked(0);                                                                                                                           \
}                                                                                                                                                   \
const_iterator begin() const noexcept                                                                                                               \
{                                                                                                                                                   \
    return GetAddressOfUnderlyingArray();                                                                                                           \
}                                                                                                                                                   \
const_iterator cbegin() const noexcept                                                                                                              \
{                                                                                                                                                   \
    return begin();                                                                                                                                 \
}                                                                                                                                                   \
void Fwrite(FILE*const f, const size_t elementsNum)                                                                                                 \
{                                                                                                                                                   \
    AssertValid();                                                                                                                                  \
    assert(f);                                                                                                                                      \
    assert(elementsNum > 0);                                                                                                                        \
    assert(elementsNum <= size());                                                                                                                  \
    ::Fwrite(f, &m_array[0], sizeof(m_array[0]), elementsNum);                                                                                      \
}                                                                                                                                                   \
size_t SizeInBytes() const                                                                                                                          \
{                                                                                                                                                   \
    return size() * sizeof(T);                                                                                                                      \
}                                                                                                                                                   \
const T* GetAddressOfUnderlyingArray() const                                                                                                        \
{                                                                                                                                                   \
    return &m_array[0];                                                                                                                             \
}                                                                                                                                                   \
/*For *Vector* classes, this assumes any existing elements would be overwritten by the new elements; eg considers the Vector to be empty */         \
void AssertSufficient(const size_t elementsNum) const                                                                                               \
{                                                                                                                                                   \
    AssertValid();                                                                                                                                  \
    assert(SizeMax() >= elementsNum);                                                                                                               \
}

#define STD_ARRAY_UTILITY_ARRAYSAFE_CONSTARRAYSAFE_CONSTVECTORSAFE_SIZE_CONST_METHOD                                                                                                         \
size_type size() const noexcept                                                                                                                     \
{                                                                                                                                                   \
    return m_elementsNumMax;                                                                                                                        \
}


#define STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS                                                                          \
size_t Strnlen() const                                                                                                                              \
{                                                                                                                                                   \
    NTF_STATIC_ASSERT(sizeof(T) == sizeof(char));/*this function is intended to be used only when *this holds ASCII characters*/                    \
    const size_t bufferMaxSize = size();                                                                                                            \
    const size_t strnlenResult = strnlen(&m_array[0], bufferMaxSize);                                                                               \
    return strnlenResult == bufferMaxSize ? 0 : strnlenResult;/*non-null-terminated buffer defined to be empty string*/                             \
}                                                                                                                                                   \
size_t GetLastValidIndex() const                                                                                                                    \
{                                                                                                                                                   \
    AssertValid();                                                                                                                                  \
    return size() - 1;                                                                                                                              \
}                                                                                                                                                   \
size_t GetOneAfterLastValidIndex() const                                                                                                            \
{                                                                                                                                                   \
    AssertValid();                                                                                                                                  \
    return GetLastValidIndex() + 1;                                                                                                                 \
}                                                                                                                                                   \
const_reference back() const                                                                                                                        \
{                                                                                                                                                   \
    return GetChecked(GetLastValidIndex());                                                                                                         \
}                                                                                                                                                   \
const_iterator end() const noexcept                                                                                                                 \
{                                                                                                                                                   \
    return const_iterator(GetAddressOfUnderlyingArray() + GetOneAfterLastValidIndex());                                                             \
}                                                                                                                                                   \
const_iterator cend() const noexcept                                                                                                                \
{                                                                                                                                                   \
    return end();                                                                                                                                   \
}                                                                                                                                                   \
//const_reverse_iterator rbegin() const noexcept 
//{
//    return const_reverse_iterator(end());
//}
//const_reverse_iterator crbegin() const noexcept 
//{
//    return rbegin();
//}

#define STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS                                                                                                  \
int Find(const T& item) const                                                                                                                       \
{                                                                                                                                                   \
    const size_type elementsNumCurrent = size();                                                                                                    \
    for (size_type i = 0; i < elementsNumCurrent; ++i)                                                                                              \
    {                                                                                                                                               \
        if (m_array[i] == item)                                                                                                                     \
        {                                                                                                                                           \
            return CastWithAssert<size_type, int>(i);                                                                                               \
        }                                                                                                                                           \
    }                                                                                                                                               \
    return -1;                                                                                                                                      \
}                                                                                                                                                   \
bool empty() const noexcept                                                                                                                         \
{                                                                                                                                                   \
    AssertValid();                                                                                                                                  \
    return size() == 0;                                                                                                                             \
}                                                                                                                                                   \

#define STD_ARRAY_UTILITY_NON_CONST_METHODS                                                                                                         \
void MemcpyFromStart(const void*const input, const size_t inputBytesNum)                                                                            \
{                                                                                                                                                   \
    MemcpyFromIndex(input, 0, inputBytesNum);                                                                                                       \
}                                                                                                                                                   \
T* data()                                                                                                                                           \
{                                                                                                                                                   \
    return GetAddressOfUnderlyingArray();                                                                                                           \
}                                                                                                                                                   \
T* GetAddressOfUnderlyingArray()                                                                                                                    \
{                                                                                                                                                   \
    return const_cast<T*>(static_cast<const ThisDataType*>(this)->GetAddressOfUnderlyingArray());                                                   \
}                                                                                                                                                   \
reference GetChecked(const size_type pos)                                                                                                           \
{                                                                                                                                                   \
    return const_cast<reference>(static_cast<const ThisDataType*>(this)->GetChecked(pos));                                                          \
}                                                                                                                                                   \
reference operator[](size_type pos)                                                                                                                 \
{                                                                                                                                                   \
    return GetChecked(pos);                                                                                                                         \
}                                                                                                                                                   \
reference at(size_type pos)                                                                                                                         \
{                                                                                                                                                   \
    return GetChecked(pos);                                                                                                                         \
}                                                                                                                                                   \
reference front()                                                                                                                                   \
{                                                                                                                                                   \
    return GetChecked(0);                                                                                                                           \
}                                                                                                                                                   \
iterator begin() noexcept                                                                                                                           \
{                                                                                                                                                   \
    return GetAddressOfUnderlyingArray();                                                                                                           \
}                                                                                                                                                   \
void MemsetEntireArray(const int val)/*<<for VectorSafe*'s only memsets size() elements, not the max elements *this can hold*/                      \
{                                                                                                                                                   \
    AssertValid();                                                                                                                                  \
    memset(GetAddressOfUnderlyingArray(), val, SizeInBytes());                                                                                      \
}                                                                                                                                                   \
void Fread(FILE*const f, const size_t elementsNum)                                                                                                  \
{                                                                                                                                                   \
    assert(f);                                                                                                                                      \
    assert(elementsNum > 0);                                                                                                                        \
    assert(elementsNum <= SizeMax());                                                                                                               \
    ::Fread(f, m_array, sizeof(T), elementsNum);                                                                                                    \
    size(elementsNum);                                                                                                                              \
    AssertValid();                                                                                                                                  \
}                                                                                                                                                   \
void Sprintf(const char*const formatString, ...)                                                                                                    \
{                                                                                                                                                   \
    va_list args;                                                                                                                                   \
    va_start(args, formatString);                                                                                                                   \
    Sprintf_va_list(formatString, args);                                                                                                            \
    va_end(args);                                                                                                                                   \
}

#define STD_ARRAY_UTILITY_SPRINTF_VA_LIST_FUNCTION_SIGNATURE void Sprintf_va_list(const char*const formatString, va_list args)
#define STD_ARRAY_UTILITY_SPRINTF_VA_LIST_PREFACE                                                                                                   \
    assert(m_array);                                                                                                                                \
    assert(formatString);                                                                                                                           \
    assert(strlen(formatString) > 0);                                                                                                               
#define STD_ARRAY_UTILITY_SPRINTF_VA_LIST_DEBUG_BODY                                                                                                \
    const int charactersPrinted = vsnprintf(&m_array[0], SizeMax(), formatString, args);                                                            
#define STD_ARRAY_UTILITY_SPRINTF_VA_LIST_RELEASE_BODY                                                                                              \
    const int charactersPrinted = vsprintf(&m_array[0], formatString, args);                                                         
#define STD_ARRAY_UTILITY_SPRINTF_VA_LIST_POSTFACE                                                                                                  \
    assert(charactersPrinted > 0);                                                                                                                  \
    size(CastWithAssert<int, size_t>(charactersPrinted + 1));/*include null-terminator in size*/                                                    \
    AssertValid();

#define STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS                                                       \
size_t SizeMax() const                                                                                                                              \
{                                                                                                                                                   \
    return size();                                                                                                                                  \
}                                                                                                                                                   \
size_t SizeMaxInBytes() const                                                                                                                       \
{                                                                                                                                                   \
    return size() * sizeof(T);                                                                                                                      \
}

#define STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS                                                                      \
reference back()                                                                                                                                    \
{                                                                                                                                                   \
    return GetChecked(GetLastValidIndex());                                                                                                         \
}                                                                                                                                                   \
iterator end() noexcept                                                                                                                             \
{                                                                                                                                                   \
    return const_cast<iterator>(static_cast<const ThisDataType*>(this)->end());                                                                     \
}                                                                                                                                                   \
template<typename U>                                                                                                                                \
void MemcpyFromStart(const U& other)                                                                                                                \
{                                                                                                                                                   \
    MemcpyFromStart(other.GetAddressOfUnderlyingArray(), other.size()*sizeof(U::value_type));                                                       \
    AssertValid();                                                                                                                                  \
}                                                                                                                                                   \

#define STD_ARRAY_UTILITY_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS                                                                                \
void Append(const ConstVectorSafeRef<T>& vectorSafeOther)                                                                                           \
{                                                                                                                                                   \
    AssertValid();                                                                                                                                  \
    const size_type vectorSafeOtherSize = vectorSafeOther.size();                                                                                   \
    const size_type thisOriginalSize = size();                                                                                                      \
    size(thisOriginalSize + vectorSafeOtherSize);                                                                                                   \
    AssertValid();                                                                                                                                  \
                                                                                                                                                    \
    memcpy(GetAddressOfUnderlyingArray() + thisOriginalSize, vectorSafeOther.GetAddressOfUnderlyingArray(), sizeof(T)*vectorSafeOtherSize);         \
}                                                                                                                                                   \
void Push(const T& item)                                                                                                                            \
{                                                                                                                                                   \
    AssertValid();                                                                                                                                  \
    const size_t indexForItem = size();                                                                                                             \
    sizeIncrement();                                                                                                                                \
    operator[](indexForItem) = item;                                                                                                                \
}                                                                                                                                                   \
bool PushIfUnique(const T& item)                                                                                                                    \
{                                                                                                                                                   \
    AssertValid();                                                                                                                                  \
    if (Find(item) >= 0)                                                                                                                            \
    {                                                                                                                                               \
        return false;                                                                                                                               \
    }                                                                                                                                               \
    else                                                                                                                                            \
    {                                                                                                                                               \
        Push(item);                                                                                                                                 \
        return true;                                                                                                                                \
    }                                                                                                                                               \
}                                                                                                                                                   \
void Pop()                                                                                                                                          \
{                                                                                                                                                   \
    sizeDecrement();                                                                                                                                \
}                                                                                                                                                   \
void RemoveItemAtIndex(const size_type indexToRemove)                                                                                               \
{                                                                                                                                                   \
    AssertValid();                                                                                                                                  \
    const size_type lastValidIndex = GetLastValidIndex();                                                                                           \
    if (indexToRemove != lastValidIndex)                                                                                                            \
    {                                                                                                                                               \
        operator[](indexToRemove) = operator[](lastValidIndex);                                                                                     \
    }                                                                                                                                               \
    sizeDecrement();                                                                                                                                \
}                                                                                                                                                   \
bool Remove(const T& item)                                                                                                                          \
{                                                                                                                                                   \
    AssertValid();                                                                                                                                  \
    const int itemIndex = Find(item);                                                                                                               \
    if (itemIndex >= 0)                                                                                                                             \
    {                                                                                                                                               \
        RemoveItemAtIndex(itemIndex);                                                                                                               \
        return true;                                                                                                                                \
    }                                                                                                                                               \
    else                                                                                                                                            \
    {                                                                                                                                               \
        return false;                                                                                                                               \
    }                                                                                                                                               \
}                                                                                                                                                   \
void SortAndRemoveDuplicates()                                                                                                                      \
{                                                                                                                                                   \
    std::sort(begin(), end());                                                                                                                      \
                                                                                                                                                    \
    int uniqueIndex = 0;                                                                                                                            \
    const size_t currentSize = size();                                                                                                              \
    for (size_t index = 1; index < currentSize; ++index)                                                                                            \
    {                                                                                                                                               \
        auto& previousElement = GetChecked(index - 1);                                                                                              \
        auto& currentElement = GetChecked(index);                                                                                                   \
        if (previousElement != currentElement)                                                                                                      \
        {                                                                                                                                           \
            GetChecked(++uniqueIndex) = currentElement;                                                                                             \
        }                                                                                                                                           \
    }                                                                                                                                               \
    size(uniqueIndex + 1);                                                                                                                          \
}

#define STD_ARRAY_UTILITY_ARRAYSAFEREF_VECTORSAFEREF_NON_CONST_METHODS                                                                              \
void SetArray(T* p, const size_t elementsNumMax)                                                                                                    \
{                                                                                                                                                   \
    SetArray(p);                                                                                                                                    \
    SetElementsNumMax(elementsNumMax);                                                                                                              \
    AssertValid();                                                                                                                                  \
}                                                                                                                                                   

#define STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONSTVECTORSAFE_OPERATOR_EQUIVALENCE                       \
template<class U>                                                                                                       \
bool operator==(const U& rhs)                                                                                           \
{                                                                                                                       \
    if (this->size() != rhs.size())                                                                                     \
    {                                                                                                                   \
        return false;                                                                                                   \
    }                                                                                                                   \
    else                                                                                                                \
    {                                                                                                                   \
        /*purposefully do a binary comparison for speed; do not support generalized comparison operators for elements*/ \
        return memcmp(this->data(), rhs.data(), rhs.SizeInBytes()) == 0;                                                \
    }                                                                                                                   \
}                                                                                                                       \
template<class U>                                                                                                       \
bool operator!=(const U& rhs)                                                                                           \
{                                                                                                                       \
    return !(*this == rhs);                                                                                             \
}

#define STD_ARRAY_UTILITY_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_ALIGNED_CONSTRUCTOR_SHARED_BODY             \
assert((uintptr_t)p % alignment == 0);                                                                                  \
assert(sizeof(T) % alignment == 0);                                                                                     \
SetElementsNumMax(elementsNumMax);                                                                                      \
SetArray(p);

#define STD_ARRAY_UTILITY_ARRAYSAFE_CONSTRUCTOR_ARRAYSAFE_SHARED_BODY                                                   \
assert(elementsNum > 0);                                                                                                \
MemcpyFromStart(r.data(), elementsNum * sizeof(T));


#define STD_ARRAY_UTILITY_MEMCPY_FROM_INDEX_FUNCTION_SIGNATURE void MemcpyFromIndex(const void*const input, const size_t index, const size_t inputBytesNum)
#define STD_ARRAY_UTILITY_MEMPCY_FROM_INDEX_VECTOR_BODY                                                                                                 \
    const size_t originalSize = size();                                                                                                                 \
    assert(index <= originalSize);/*new elements must start at the first invalid index and/or overwrite existing elements -- eg do not create a "hole" of invalid elements between the old valid elements and the new valid elements*/\
    const size_t newElementsNumPlusIndex = inputBytesNum / sizeof(T) + index;                                                                           \
    SetElementsNumCurrent(originalSize > newElementsNumPlusIndex ? originalSize : newElementsNumPlusIndex);                                             \
    AssertValid()
#define STD_ARRAY_UTILITY_MEMCPY_FROM_INDEX_SHARED_BODY                                                                                                 \
    AssertValid();                                                                                                                                      \
    assert(index * sizeof(T) + inputBytesNum <= SizeMaxInBytes());                                                                                      \
    assert(input);                                                                                                                                      \
    assert(index >= 0);                                                                                                                                 \
    /*assert(inputBytesNum >= 0);*//*allow memcpy's of 0 bytes for generality*/                                                                         \
    assert(inputBytesNum % sizeof(T) == 0);                                                                                                             \
    memcpy(&GetAddressOfUnderlyingArray()[index], input, inputBytesNum)

#define STD_UTILITY_MEMCPY_FROM_FREAD_FUNCTION_SIGNATURE void MemcpyFromFread(FILE*const f, const size_t elementsNum)
#define STD_UTILITY_MEMCPY_FROM_FREAD_FUNCTION_SHARED_BODY  \
    assert(f);                                              \
    assert(elementsNum > 0);                                \
    assert(elementsNum <= SizeMax());                       \
    ::Fread(f, &m_array[0], sizeof(T), elementsNum)


#define STD_UTILITY_ARRAYSAFEREF_VECTORSAFECONSTRUCTOR_BODY \
    assert(other);                                          \
    SetElementsNumMax(other->size());/*note that size() is used here -- not SizeMax() -- so that *this provides a "view" into only the current size of a VectorSafeRef; in other words, *this takes its maximum size to be the VectorSafeRef's current size, and ignores the VectorSafeRef's maximum size*/ \
    SetArray(other->begin())

#define STD_UTILITY_CONSTARRAYSAFEREF_VECTORSAFECONSTRUCTOR_BODY    \
    SetElementsNumMax(other.size());/*note that size() is used here -- not SizeMax() -- so that *this provides a "view" into only the current size of a (Const)VectorSafeRef; in other words, *this takes its maximum size to be the (Const)VectorSafeRef's current size, and ignores the (Const)VectorSafeRef's maximum size*/ \
    SetArray(other.begin())



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

#define NTF_FWRITE_SPRINTF_PREFACE              \
    assert(file);                               \
    assert(formatString);                       \
    assert(strlen(formatString) > 0);           \
                                                \
    ArraySafe<char, kElementsMax> string;       \
    va_list args;                               \
    va_start(args, formatString);               \
    string.Sprintf_va_list(formatString, args); \
    va_end(args);
#define NTF_FWRITE_SPRINTF_FWRITE string.Fwrite(file, strlen(string.begin()) + 1)
template<size_t kElementsMax=512>
void FwriteSprintf(FILE*const file, const char*const formatString, ...)
{
    NTF_FWRITE_SPRINTF_PREFACE;
    NTF_FWRITE_SPRINTF_FWRITE;
}
template<size_t kElementsMax = 512>
void FwriteSprintf(FILE*const file, RTL_CRITICAL_SECTION*const criticalSectionPtr, const char*const formatString, ...)
{
    assert(criticalSectionPtr);
    NTF_FWRITE_SPRINTF_PREFACE;

    CriticalSectionEnter(criticalSectionPtr);
    NTF_FWRITE_SPRINTF_FWRITE;
    CriticalSectionLeave(criticalSectionPtr);
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

template<class T, size_t kElementsMax>
class VectorSafe;

template<class T, size_t kElementsNum>
class ArraySafe;

template<class T>
class ConstVectorSafeRef;

template<class T>
class ConstArraySafeRef;

template<class T>
class VectorSafeRef
{
public:
    typedef VectorSafeRef<T> ThisDataType;
    STD_ARRAY_UTILITY_CONST_TYPEDEFS;
    STD_ARRAY_UTILITY_NONCONST_TYPEDEFS;

private:
    T* m_array;
    size_t* m_elementsNumCurrent;
#if STD_UTILITY_DEBUG
    size_t m_elementsNumMax;
#endif//#if STD_UTILITY_DEBUG

    void SetElementsNumCurrent(const size_t elementsNumCurrent)
    {
        assert(m_elementsNumCurrent);
        *m_elementsNumCurrent = elementsNumCurrent;
    }
    void SetElementsNumCurrentPtr(size_t* const elementsNumCurrentPtr)
    {
        assert(elementsNumCurrentPtr);
        m_elementsNumCurrent = elementsNumCurrentPtr;
    }
    void SetElementsNumMax(const size_t elementsNumMax)
    {
        assert(elementsNumMax > 0);
#if STD_UTILITY_DEBUG
        m_elementsNumMax = elementsNumMax;
#endif//#if STD_UTILITY_DEBUG
    }

    STD_ARRAY_UTILITY_NONCONST_REF_PRIVATE_METHODS;

public:
    //allow writable arguments to be preceded by an & (ambersand) -- this is best-practice for documenting argument writability.  In terms of performance, I'm trusting compilers to simply reference a single class of *this's pointers rather than duplicating them; note that C++ does allow ConstVectorSafe and ConstArraySafe to be passed by const&, leaving no chance of unnecessarily duplicated pointers
    VectorSafeRef(VectorSafeRef<T>*const vectorSafeRef)
    {
        assert(vectorSafeRef);
        SetElementsNumCurrentPtr(vectorSafeRef->m_elementsNumCurrent);
#if STD_UTILITY_DEBUG
        SetElementsNumMax(vectorSafeRef->m_elementsNumMax);
#endif//#if STD_UTILITY_DEBUG
        SetArray(vectorSafeRef->begin());
    }

    template<size_t kElementsMax>
    VectorSafeRef(VectorSafe<T,kElementsMax>*const vectorSafe)
    {
        assert(vectorSafe);

        SetElementsNumCurrentPtr(&vectorSafe->m_elementsNumCurrent);
#if STD_UTILITY_DEBUG
        SetElementsNumMax(vectorSafe->SizeMax());
#endif//#if STD_UTILITY_DEBUG
        SetArray(vectorSafe->begin());
    }

    VectorSafeRef()
    {
        Reset();
    }                                                                                                                                                 

    void Reset()
    {
        m_array = nullptr;
        m_elementsNumCurrent = 0;

#if STD_UTILITY_DEBUG
        m_elementsNumMax = 0;
#endif//#if STD_UTILITY_DEBUG
    }

    void AssertValid() const
    {
#if STD_UTILITY_DEBUG
        assert(m_elementsNumCurrent);
        assert(*m_elementsNumCurrent <= m_elementsNumMax);
        assert(m_elementsNumMax > 0);
#endif//#if STD_UTILITY_DEBUG
    }

    size_type size() const noexcept
    {
        return m_elementsNumCurrent ? *m_elementsNumCurrent : 0;
    }
#if STD_UTILITY_DEBUG
    size_t SizeMax() const
    {
        return m_elementsNumMax;
    }
    size_t SizeMaxInBytes() const
    {
        AssertValid();
        return m_elementsNumMax * sizeof(T);
    }
#endif//#if STD_UTILITY_DEBUG
    void size(const size_t size)
    {
        SetElementsNumCurrent(size);
    }
    void sizeIncrement()
    {
        AssertValid();
        ++(*m_elementsNumCurrent);
        AssertValid();
    }
    void sizeDecrement()
    {
        AssertValid();
        --(*m_elementsNumCurrent);
        AssertValid();
    }

    STD_ARRAY_UTILITY_SPRINTF_VA_LIST_FUNCTION_SIGNATURE
    {
        NTF_STATIC_ASSERT(sizeof(T) == sizeof(char));//this function is intended to be used only when *this holds ASCII characters
#if STD_UTILITY_DEBUG
        STD_ARRAY_UTILITY_SPRINTF_VA_LIST_DEBUG_BODY;
#else
        STD_ARRAY_UTILITY_SPRINTF_VA_LIST_RELEASE_BODY;
#endif//#if STD_UTILITY_DEBUG
        STD_ARRAY_UTILITY_SPRINTF_VA_LIST_POSTFACE;
    }

    STD_ARRAY_UTILITY_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS;
    STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS;
    STD_ARRAY_UTILITY_NON_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS;
    STD_ARRAY_UTILITY_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFEREF_VECTORSAFEREF_NON_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONSTVECTORSAFE_OPERATOR_EQUIVALENCE;

    STD_ARRAY_UTILITY_MEMCPY_FROM_INDEX_FUNCTION_SIGNATURE
    {
        STD_ARRAY_UTILITY_MEMPCY_FROM_INDEX_VECTOR_BODY;
        STD_ARRAY_UTILITY_MEMCPY_FROM_INDEX_SHARED_BODY;
    }
    STD_UTILITY_MEMCPY_FROM_FREAD_FUNCTION_SIGNATURE
    {
        STD_UTILITY_MEMCPY_FROM_FREAD_FUNCTION_SHARED_BODY;
        SetElementsNumCurrent(elementsNum);
    }
};


template<class T>
class ArraySafeRef
{
public:
    typedef ArraySafeRef<T> ThisDataType;
    STD_ARRAY_UTILITY_CONST_TYPEDEFS;
    STD_ARRAY_UTILITY_NONCONST_TYPEDEFS;

private:
    void size(const size_t size)
    {
        assert(size <= m_elementsNumMax);
    }//to allow implementation of STD_ARRAY_UTILITY_SPRINTF_VA_LIST_BODY()
    void SetElementsNumMax(const size_t elementsNumMax)
    {
#if STD_UTILITY_DEBUG
        m_elementsNumMax = elementsNumMax;
#endif//#if STD_UTILITY_DEBUG
    }

    STD_ARRAY_UTILITY_NONCONST_REF_PRIVATE_METHODS;

    T* m_array;
#if STD_UTILITY_DEBUG
    size_t m_elementsNumMax;
#endif//#if STD_UTILITY_DEBUG

public:
    ///@todo
    //ArraySafeRef(T*const pointer, const std::initializer_list<T>& initializerList, const size_t maxSize)
    //{
    //    SetArray(pointer);
    //    SetElementsNumMax(maxSize);
    //    MemcpyFromStart(initializerList.begin(), initializerList.size()*sizeof(T));
    //    AssertValid();
    //}
    //allow writable arguments to be preceded by an & (ambersand) -- this is best-practice for documenting argument writability.  In terms of performance, I'm trusting compilers to simply reference a single class of *this's pointers rather than duplicating them; note that C++ does allow ConstVectorSafe and ConstArraySafe to be passed by const&, leaving no chance of unnecessarily duplicated pointers
    ArraySafeRef(ArraySafeRef<T>*const other)
    {
        assert(other);
#if STD_UTILITY_DEBUG
        SetElementsNumMax(other->SizeMax());
#endif//#if STD_UTILITY_DEBUG
        SetArray(other->begin());
    }
    template<size_t kElementsMax>
    ArraySafeRef(ArraySafe<T, kElementsMax>*const other)
    {
        assert(other);
#if STD_UTILITY_DEBUG
        SetElementsNumMax(other->SizeMax());
#endif//#if STD_UTILITY_DEBUG
        SetArray(other->begin());
    }
    ArraySafeRef(VectorSafeRef<T>*const other)//can't be a non-const C++-reference, because the language (needlessly, in my view), disallows passing non-const temporary arguments by non-const reference
    {
        STD_UTILITY_ARRAYSAFEREF_VECTORSAFECONSTRUCTOR_BODY;
    }
    template<size_t kElementsMax>
    ArraySafeRef(VectorSafe<T, kElementsMax>*const other)//can't be a non-const C++-reference, because the language (needlessly, in my view), disallows passing non-const temporary arguments by non-const reference
    {
        STD_UTILITY_ARRAYSAFEREF_VECTORSAFECONSTRUCTOR_BODY;
    }

    ArraySafeRef(T*const p, const size_t elementsNumMax)
    {
#if STD_UTILITY_DEBUG
        SetElementsNumMax(elementsNumMax);
#endif//#if STD_UTILITY_DEBUG
        SetArray(p);
    }

    ArraySafeRef(T*const p, const size_t elementsNumMax, const size_t alignment)
    {
        STD_ARRAY_UTILITY_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_ALIGNED_CONSTRUCTOR_SHARED_BODY;
    }

    ArraySafeRef()
    {
        Reset();
    }
    void Reset()
    {
        m_array = nullptr;

#if STD_UTILITY_DEBUG
        m_elementsNumMax = 0;
#endif//#if STD_UTILITY_DEBUG
    }

    void AssertValid() const
    {
#if STD_UTILITY_DEBUG
        assert(m_elementsNumMax > 0);
        assert(m_array);
#endif//#if STD_UTILITY_DEBUG
    }

    STD_ARRAY_UTILITY_SPRINTF_VA_LIST_FUNCTION_SIGNATURE
    {
        NTF_STATIC_ASSERT(sizeof(T) == sizeof(char));//this function is intended to be used only when *this holds ASCII characters
#if STD_UTILITY_DEBUG
        STD_ARRAY_UTILITY_SPRINTF_VA_LIST_DEBUG_BODY;
#else
        STD_ARRAY_UTILITY_SPRINTF_VA_LIST_RELEASE_BODY;
#endif//#if STD_UTILITY_DEBUG
        STD_ARRAY_UTILITY_SPRINTF_VA_LIST_POSTFACE;
    }

    STD_ARRAY_UTILITY_MEMCPY_FROM_INDEX_FUNCTION_SIGNATURE
    {
        STD_ARRAY_UTILITY_MEMCPY_FROM_INDEX_SHARED_BODY;
    }
    STD_UTILITY_MEMCPY_FROM_FREAD_FUNCTION_SIGNATURE
    {
        STD_UTILITY_MEMCPY_FROM_FREAD_FUNCTION_SHARED_BODY;
    }

#if STD_UTILITY_DEBUG
    STD_ARRAY_UTILITY_ARRAYSAFE_CONSTARRAYSAFE_CONSTVECTORSAFE_SIZE_CONST_METHOD;
    STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS;
#endif//#if STD_UTILITY_DEBUG

    STD_ARRAY_UTILITY_CONST_METHODS;
    STD_ARRAY_UTILITY_NON_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFEREF_VECTORSAFEREF_NON_CONST_METHODS;
};

///can't just use a const ArraySafeRef<>() because I want to encourage the use of & for writable arguments -- and no & for read-only arguments -- as much as possible.  This class allows for the latter with ArraySafe
///is best passed by const& -- eg const ConstArraySafeRef& -- since that all but ensures that all compilers will not create extraneous size datafields, even in debug builds
template<class T>
class ConstArraySafeRef
{
public:
    typedef ConstArraySafeRef<T> ThisDataType;
    STD_ARRAY_UTILITY_CONST_TYPEDEFS;

private:
    void SetElementsNumMax(const size_t elementsNumMax)
    {
#if STD_UTILITY_DEBUG
        m_elementsNumMax = elementsNumMax;
#endif//#if STD_UTILITY_DEBUG
    }

    STD_ARRAY_UTILITY_CONST_REF_PRIVATE_METHODS;

    const T* m_array;
#if STD_UTILITY_DEBUG
    size_t m_elementsNumMax;
#endif//#if STD_UTILITY_DEBUG

public:
    ///@todo
    //ConstArraySafeRef(T*const pointer, const std::initializer_list<T>& initializerList, const size_t maxSize)
    //{
    //    SetArray(pointer);
    //    SetElementsNumMax(maxSize);
    //    MemcpyFromStart(initializerList.begin(), initializerList.size()*sizeof(T));
    //    AssertValid();
    //}

    //can't do this, because it would allow *this to be initialized with a (Const)VectorSafe(Ref), and read elements between (Const)VectorSafe(Ref)::size() and (Const)VectorSafe(Ref)::SizeMax()
//    template<typename U>
//    ConstArraySafeRef(const U& other)
//    {
//        assert(other);
//#if STD_UTILITY_DEBUG
//        SetElementsNumMax(other->SizeMax());
//#endif//#if STD_UTILITY_DEBUG
//        SetArray(other->begin());
//    }

    template<size_t kElementsMax>
    ConstArraySafeRef(const ArraySafe<T, kElementsMax>& other)
    {
#if STD_UTILITY_DEBUG
        SetElementsNumMax(other.SizeMax());
#endif//#if STD_UTILITY_DEBUG
        SetArray(other.begin());
    }
    ConstArraySafeRef(const ArraySafeRef<T>& other)
    {
#if STD_UTILITY_DEBUG
        SetElementsNumMax(other.SizeMax());
#endif//#if STD_UTILITY_DEBUG
        SetArray(other.begin());
    }
    ConstArraySafeRef(const VectorSafeRef<T>& other)
    {
        STD_UTILITY_CONSTARRAYSAFEREF_VECTORSAFECONSTRUCTOR_BODY;
    }
    ConstArraySafeRef(const ConstVectorSafeRef<T>& other)
    {
        STD_UTILITY_CONSTARRAYSAFEREF_VECTORSAFECONSTRUCTOR_BODY;
    }
    template<size_t kElementsMax>
    ConstArraySafeRef(const VectorSafe<T, kElementsMax>& other)
    {
        STD_UTILITY_CONSTARRAYSAFEREF_VECTORSAFECONSTRUCTOR_BODY;
    }

    ConstArraySafeRef(const T*const p, const size_t elementsNumMax)
    {
#if STD_UTILITY_DEBUG
        SetElementsNumMax(elementsNumMax);
#endif//#if STD_UTILITY_DEBUG
        SetArray(p);
    }

    ConstArraySafeRef(const T*const p, const size_t elementsNumMax, const size_t alignment)
    {
        STD_ARRAY_UTILITY_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_ALIGNED_CONSTRUCTOR_SHARED_BODY;
    }

    ConstArraySafeRef()
    {
        Reset();
    }
    void Reset()
    {
        m_array = nullptr;

#if STD_UTILITY_DEBUG
        m_elementsNumMax = 0;
#endif//#if STD_UTILITY_DEBUG
    }

    void AssertValid() const
    {
#if STD_UTILITY_DEBUG
        assert(m_elementsNumMax > 0);
        assert(m_array);
#endif//#if STD_UTILITY_DEBUG
    }

#if STD_UTILITY_DEBUG
    STD_ARRAY_UTILITY_ARRAYSAFE_CONSTARRAYSAFE_CONSTVECTORSAFE_SIZE_CONST_METHOD;
    STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS;
#endif//#if STD_UTILITY_DEBUG

    STD_ARRAY_UTILITY_CONST_METHODS;
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


///can't just use a const VectorSafeRef<>() because I want to encourage the use of & for writable arguments -- and no & for read-only arguments -- as much as possible.  This class allows for the latter with VectorSafe
///is best passed by const& -- eg const ConstVectorSafeRef& -- since that all but ensures that all compilers will not create extraneous pointers from this double-pointer class
template<class T>
class ConstVectorSafeRef
{
public:
    typedef ConstVectorSafeRef<T> ThisDataType;
    STD_ARRAY_UTILITY_CONST_TYPEDEFS;

private:
    const T* m_array;
    size_t m_elementsNumMax;

    void SetElementsNumMax(const size_t elementsNumMax)
    {
        m_elementsNumMax = elementsNumMax;
    }

    STD_ARRAY_UTILITY_CONST_REF_PRIVATE_METHODS;
public:
    ConstVectorSafeRef()
    {
        Reset();
    }

    ConstVectorSafeRef(T*const p, const size_t elementsNumMax, const size_t alignment)
    {
        STD_ARRAY_UTILITY_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_ALIGNED_CONSTRUCTOR_SHARED_BODY;
    }

    ConstVectorSafeRef(const T*const pointer, const size_t elementsNumMax)
    {
        SetElementsNumMax(elementsNumMax);
        SetArray(pointer);
    }

    template<typename U>
    ConstVectorSafeRef(const U& other)
    {
        SetElementsNumMax(other.size());
        SetArray(other.begin());
    }

    void Reset()
    {
        SetArray(nullptr);
        SetElementsNumMax(0);
    }

    void AssertValid() const
    {
        //purposefully no-op to allow null ConstVectorSafeRef semantics
    }

    STD_ARRAY_UTILITY_ARRAYSAFE_CONSTARRAYSAFE_CONSTVECTORSAFE_SIZE_CONST_METHOD;
    STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS;

    STD_ARRAY_UTILITY_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS;
    STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONSTVECTORSAFE_OPERATOR_EQUIVALENCE;
};

template<class T, size_t kElementsNum>
class ArraySafe
{
public:
    typedef ArraySafe<T, kElementsNum> ThisDataType;
    STD_ARRAY_UTILITY_CONST_TYPEDEFS;
    STD_ARRAY_UTILITY_NONCONST_TYPEDEFS;

private:
    void size(const size_t size)
    {
        assert(size <= kElementsNum);
    }//to allow implementation of STD_ARRAY_UTILITY_SPRINTF_VA_LIST_BODY()

    T m_array[kElementsNum];
public:
    ArraySafe()
    {
    }
    ArraySafe(FILE*const f, const size_t elementsNum)
    {
        assert(f);
        assert(elementsNum > 0);
        assert(elementsNum <= kElementsNum);
        Fread(f, elementsNum);
    }
    ArraySafe(const std::initializer_list<T>& initializerList)
    {
        MemcpyFromStart(initializerList.begin(), initializerList.size()*sizeof(T));
    }
    ArraySafe(const T*const p, const size_t elementsNum)
    {
        MemcpyFromStart(p, elementsNum*sizeof(T));
    }
    //don't use template<class U>ArraySafe(const U& r), since while this does eliminate a little code duplication, it also yields confusing templated compiler errors when a user erroneously passes a pointer without a number of elements rather than calling ArraySafe(const T*const p, const size_t elementsNum).  Prefer usability at the cost of minor code duplication
    ArraySafe(const ConstVectorSafeRef<T>& r)
    {
        MemcpyFromStart(r);
    }
    ArraySafe(VectorSafeRef<T> r)
    {
        MemcpyFromStart(r);
    }
    ArraySafe(ArraySafeRef<T> r, const size_t elementsNum)
    {
        STD_ARRAY_UTILITY_ARRAYSAFE_CONSTRUCTOR_ARRAYSAFE_SHARED_BODY;
    }
    ArraySafe(const ConstArraySafeRef<T>& r, const size_t elementsNum)
    {
        STD_ARRAY_UTILITY_ARRAYSAFE_CONSTRUCTOR_ARRAYSAFE_SHARED_BODY;
    }
    template<size_t kOtherElementsMax>
    ArraySafe(const VectorSafe<T, kOtherElementsMax>& other)
    {
        assert(kElementsNum >= other.size());
        MemcpyFromStart(other);
    }
    template<class T, size_t kElementsNum>
    operator ArraySafeRef<T>()
    {
        return ArraySafeRef(this);
    }

    void AssertValid() const
    {
        //purposefully no-op to satisfy STD_ARRAY_UTILITY* macros, to minimize code duplication
    }

    STD_ARRAY_UTILITY_SPRINTF_VA_LIST_FUNCTION_SIGNATURE
    {
        NTF_STATIC_ASSERT(sizeof(T) == sizeof(char));//this function is intended to be used only when *this holds ASCII characters
#if STD_UTILITY_DEBUG
        STD_ARRAY_UTILITY_SPRINTF_VA_LIST_DEBUG_BODY;
#else
        STD_ARRAY_UTILITY_SPRINTF_VA_LIST_RELEASE_BODY;
#endif//#if STD_UTILITY_DEBUG
        STD_ARRAY_UTILITY_SPRINTF_VA_LIST_POSTFACE;
    }

    STD_ARRAY_UTILITY_MEMCPY_FROM_INDEX_FUNCTION_SIGNATURE
    {
        STD_ARRAY_UTILITY_MEMCPY_FROM_INDEX_SHARED_BODY;
    }
    STD_UTILITY_MEMCPY_FROM_FREAD_FUNCTION_SIGNATURE
    {
        STD_UTILITY_MEMCPY_FROM_FREAD_FUNCTION_SHARED_BODY;
    }

    size_type size() const noexcept
    {
        return kElementsNum;
    }

    ///@todo: implement reverse iterator
    //reverse_iterator rbegin() noexcept 
    //{
    //    return reverse_iterator(end());
    //}
    //iterator rend() and crend() not implemented

    STD_ARRAY_UTILITY_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS;
    STD_ARRAY_UTILITY_NON_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONSTVECTORSAFE_OPERATOR_EQUIVALENCE;
};

template<class T, size_t kElementsMax>
class VectorSafe
{
public:
    typedef VectorSafe<T, kElementsMax> ThisDataType;
    STD_ARRAY_UTILITY_CONST_TYPEDEFS;
    STD_ARRAY_UTILITY_NONCONST_TYPEDEFS;

private:
    T m_array[kElementsMax];
    size_t m_elementsNumCurrent;

    friend class VectorSafeRef<T>;
    friend class ConstVectorSafeRef<T>;

    void SetElementsNumCurrent(const size_t elementsNumCurrent)
    {
        m_elementsNumCurrent = elementsNumCurrent;
        AssertValid();
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
        size(0);//don't trigger spurious assert about unset size in debug builds
        MemcpyFromStart(initializerList.begin(), initializerList.size()*sizeof(T));
    }
    VectorSafe(const T*const p, const size_t elementsNum)
    {
        size(0);//don't trigger spurious assert about unset size in debug builds
        MemcpyFromStart(p, elementsNum * sizeof(T));
    }
    VectorSafe(const ConstVectorSafeRef<T>& r)
    {
        size(0);//don't trigger spurious assert about unset size in debug builds if argument is of size zero
        MemcpyFromStart(r);
    }
    template<class T, size_t kElementsMax>
    operator const ConstVectorSafeRef<T>&()
    {
        return ConstVectorSafeRef(this);
    }
    ///can't use; no way of distinguishing conversion to VectorSafeRef vs ArraySafeRef, which is needed for clean interoperability
    //VectorSafeRef<T> operator&()
    //{
    //    return VectorSafeRef<T>(this);
    //}
    template<class T, size_t kElementsMax>
    operator VectorSafeRef<T>()
    {
        return VectorSafeRef(this);
    }

    void AssertValid() const
    {
#if STD_UTILITY_DEBUG
        assert(m_elementsNumCurrent <= SizeMax());

        static_assert(kElementsMax > 0, "VectorSafe<T>::kElementsMax must be greater than 0");
#endif//#if STD_UTILITY_DEBUG
    }

    STD_ARRAY_UTILITY_MEMCPY_FROM_INDEX_FUNCTION_SIGNATURE
    {
        STD_ARRAY_UTILITY_MEMPCY_FROM_INDEX_VECTOR_BODY;
        STD_ARRAY_UTILITY_MEMCPY_FROM_INDEX_SHARED_BODY;
    }
    STD_UTILITY_MEMCPY_FROM_FREAD_FUNCTION_SIGNATURE
    {
        STD_UTILITY_MEMCPY_FROM_FREAD_FUNCTION_SHARED_BODY;
        SetElementsNumCurrent(elementsNum);
    }

    STD_ARRAY_UTILITY_SPRINTF_VA_LIST_FUNCTION_SIGNATURE
    {
        NTF_STATIC_ASSERT(sizeof(T) == sizeof(char));//this function is intended to be used only when *this holds ASCII characters
#if STD_UTILITY_DEBUG
    STD_ARRAY_UTILITY_SPRINTF_VA_LIST_DEBUG_BODY;
#else
    STD_ARRAY_UTILITY_SPRINTF_VA_LIST_RELEASE_BODY;
#endif//#if STD_UTILITY_DEBUG
    STD_ARRAY_UTILITY_SPRINTF_VA_LIST_POSTFACE;
    }

    size_type size() const noexcept
    {
        AssertValid();
        return m_elementsNumCurrent;
    }
    void size(const size_t size)
    {
        m_elementsNumCurrent = size;
        AssertValid();
    }
    void sizeIncrement()
    {
        AssertValid();
        ++m_elementsNumCurrent;
        AssertValid();
    }
    void sizeDecrement()
    {
        AssertValid();
        --m_elementsNumCurrent;
        AssertValid();
    }
    size_t SizeMax() const
    {
        return kElementsMax;
    }
    size_t SizeMaxInBytes() const
    {
        return SizeMax()*sizeof(T);
    }

    STD_ARRAY_UTILITY_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS;
    STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS;
    STD_ARRAY_UTILITY_NON_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS;
    STD_ARRAY_UTILITY_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFEREF_VECTORSAFEREF_NON_CONST_METHODS;
    STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONSTVECTORSAFE_OPERATOR_EQUIVALENCE;
};

#pragma warning(default : 4996)//disallow sprintf() outside of this header                                                                                                             \
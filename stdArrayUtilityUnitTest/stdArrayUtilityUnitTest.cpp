#include <stdio.h>
#include <stdlib.h>
#include"../VulkanTest01/QueueCircular.h"
#include"../VulkanTest01/stdArrayUtility.h"
#include"../VulkanTest01/WindowsUtil.h"

///@todo: allow failed asserts to continue -- eg allow unit tests to succeed BECAUSE they trigger asserts, verifying that the asserts are working as intended.  Replace raw assert's with a macro that can be defined to exit(-1) in non-debug builds

//don't complain about scanf being unsafe
#pragma warning(disable : 4996)
#pragma warning(disable : 4702)

static void ConsolePauseForUserAcknowledgement()
{
    int i;
    printf("Enter a character and press ENTER to exit\n");
    scanf("%i", &i);
}

static void ExitOnFail(const size_t lineNumber)
{
    printf("Failure on line %zi of file %s\n", lineNumber, __FILE__);
    ConsolePauseForUserAcknowledgement();
    exit(-1);
}

const size_t kTestElementsNum = 16;
static const char s_strTest[kTestElementsNum] = "Testing";
static size_t s_arrayTest[kTestElementsNum] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
static size_t s_arrayTestOther[kTestElementsNum] = { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 };

template<class T, class U>
static void TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalence(T c, U cOriginal, U cDifferent)
{
    if (c != cOriginal)
    {
        ExitOnFail(__LINE__);
    }

    if (c == cDifferent)
    {
        ExitOnFail(__LINE__);
    }
}

template<class T, class U>
static void TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalenceSize(T c, U cDifferent)
{
    if (c.size() == cDifferent.size())
    {
        ExitOnFail(__LINE__);
    }
    if (c == cDifferent)
    {
        ExitOnFail(__LINE__);
    }
}

template<size_t kTestElementsNumForArraySafe, class T>
static void Test_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalence(T c)
{

    VectorSafe<T::value_type, kTestElementsNum> vCopyOfC(c);
    VectorSafe<T::value_type, kTestElementsNum> vOther(&s_arrayTestOther[0], kTestElementsNumForArraySafe);
    TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalence(c, ArraySafe<T::value_type, kTestElementsNumForArraySafe>(c), ArraySafe<T::value_type, kTestElementsNumForArraySafe>(&s_arrayTestOther[0], kTestElementsNumForArraySafe));//need to be kTestElementsNumForArraySafe since ArraySafe always tests the entire array for equality, where VectorSafe* only tests valid elements
    TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalence(c, vCopyOfC, vOther);//need to use kTestElementsNumForArraySafe to make sure the different container has the same number of elements as the original container, just different values
    TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalence(c, VectorSafeRef<T::value_type>(&vCopyOfC), VectorSafeRef<T::value_type>(&vOther));//need to use kTestElementsNumForArraySafe to make sure the different container has the same number of elements as the original container, just different values
    TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalence(c, ConstVectorSafeRef<T::value_type>(vCopyOfC), ConstVectorSafeRef<T::value_type>(vOther));

    const size_t kTestElementsNumForArraySafeLess = kTestElementsNumForArraySafe - 1;
    vOther.size(kTestElementsNumForArraySafeLess);
    TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalenceSize(c, ArraySafe<T::value_type, kTestElementsNumForArraySafeLess>(&s_arrayTest[0], kTestElementsNumForArraySafeLess));
    TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalenceSize(c, VectorSafe<T::value_type, kTestElementsNum>(&s_arrayTest[0], kTestElementsNumForArraySafeLess));
    TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalenceSize(c, VectorSafeRef<T::value_type>(&vOther));
    TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalenceSize(c, ConstVectorSafeRef<T::value_type>(vOther));
}

template<class T>
static void TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_CONSTARRAYSAFE_CONSTVECTORSAFE_METHODS(T c, const size_t itemsNum)
{
    if (c.size() != itemsNum)
    {
        ExitOnFail(__LINE__);
    }
}

static void Test_STD_ARRAY_UTILITY_ARRAYSAFE_CONSTARRAYSAFE_CONSTVECTORSAFE_METHODS()
{
    {
        const size_t kTestElementsNumNotFull = kTestElementsNum - 4;
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNumNotFull);//VectorSafe isn't full
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_CONSTARRAYSAFE_CONSTVECTORSAFE_METHODS(ConstVectorSafeRef<size_t>(v), kTestElementsNumNotFull);
    }
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNum);//VectorSafe is full
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_CONSTARRAYSAFE_CONSTVECTORSAFE_METHODS(ConstVectorSafeRef<size_t>(v), kTestElementsNum);

#if STD_UTILITY_DEBUG        
        ArraySafe<size_t, kTestElementsNum> a(&s_arrayTest[0], kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_CONSTARRAYSAFE_CONSTVECTORSAFE_METHODS(a, kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_CONSTARRAYSAFE_CONSTVECTORSAFE_METHODS(ConstArraySafeRef<size_t>(a), kTestElementsNum);
#endif//#if STD_UTILITY_DEBUG
    }
}

template<class T>
static void TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS(T c)
{
    const size_t lastValidIndex = c.GetLastValidIndex();
    if (c[lastValidIndex] != s_arrayTest[lastValidIndex])
    {
        ExitOnFail(__LINE__);
    }
    if (c.GetOneAfterLastValidIndex() != c.size())
    {
        ExitOnFail(__LINE__);
    }
    if (c.back() != c[lastValidIndex])
    {
        ExitOnFail(__LINE__);
    }
    if(c.end() != &c[lastValidIndex] + 1)
    {
        ExitOnFail(__LINE__);
    }
    if (c.end() != c.cend())
    {
        ExitOnFail(__LINE__);
    }
    if (c.SizeInBytes() != c.size() * sizeof(T::value_type))
    {
        ExitOnFail(__LINE__);
    }
}
template<class T>
static void TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_string(T c)
{
    const char*const testString = &s_strTest[0];
    if (strlen(testString) != c.Strnlen())
    {
        ExitOnFail(__LINE__);
    }
}
static void Test_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS()
{
    {
        const size_t kTestElementsNumNotFull = kTestElementsNum - 4;
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNumNotFull);//VectorSafe isn't full
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS(v);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS(VectorSafeRef<size_t>(&v));
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS(ConstVectorSafeRef<size_t>(v));

        Test_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalence<kTestElementsNumNotFull>(v);
        Test_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalence<kTestElementsNumNotFull>(VectorSafeRef<size_t>(&v));
        Test_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalence<kTestElementsNumNotFull>(ConstVectorSafeRef<size_t>(v));
    }
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNum);//VectorSafe is full
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS(v);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS(VectorSafeRef<size_t>(&v));
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS(ConstVectorSafeRef<size_t>(v));

        Test_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalence<kTestElementsNum>(v);
        Test_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalence<kTestElementsNum>(VectorSafeRef<size_t>(&v));
        Test_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalence<kTestElementsNum>(ConstVectorSafeRef<size_t>(v));
    }
    {
        ArraySafe<size_t, kTestElementsNum> a(&s_arrayTest[0], kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS(a);
        Test_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_operatorEquivalence<kTestElementsNum>(a);
    }

    {
        VectorSafe<char, kTestElementsNum> v(&s_strTest[0], kTestElementsNum - 4);//VectorSafe isn't full
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_string(v);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_string(VectorSafeRef<char>(&v));
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_string(ConstVectorSafeRef<char>(v));
    }
    {
        VectorSafe<char, kTestElementsNum> v(&s_strTest[0], kTestElementsNum);//VectorSafe is full
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_string(v);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_string(VectorSafeRef<char>(&v));
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_string(ConstVectorSafeRef<char>(v));
    }
    {
        ArraySafe<char, kTestElementsNum> a(&s_strTest[0], kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS_string(a);
    }
}

template<class T>
static void TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS(T c, const size_t itemsNumMax)
{
    if (c.SizeMaxInBytes() != sizeof(T::value_type)*itemsNumMax)
    {
        ExitOnFail(__LINE__);
    }
    if (c.SizeMax() != itemsNumMax)
    {
        ExitOnFail(__LINE__);
    }
}
static void Test_STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS()
{
    {
        const size_t kTestElementsNumNotFull = kTestElementsNum - 4;
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNumNotFull);//VectorSafe isn't full
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS(v, kTestElementsNum);

#if STD_UTILITY_DEBUG
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS(ConstVectorSafeRef<size_t>(v), v.size());
#endif//#if STD_UTILITY_DEBUG    
    }
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNum);//VectorSafe is full
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS(v, kTestElementsNum);
#if STD_UTILITY_DEBUG
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS(ConstVectorSafeRef<size_t>(v), v.size());
#endif//#if STD_UTILITY_DEBUG

        ArraySafe<size_t, kTestElementsNum> a(&s_arrayTest[0], kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS(a, kTestElementsNum);

#if STD_UTILITY_DEBUG
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS(ArraySafeRef<size_t>(&a), kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS(ConstArraySafeRef<size_t>(a), kTestElementsNum);
#endif//#if STD_UTILITY_DEBUG
    }
}


template<class T>
static void TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_ARRAYSAFE_CONST_METHODS(T c, const size_t itemsNum, const size_t itemsNumMax)
{
    if (c.SizeMaxInBytes() != sizeof(T::value_type)*itemsNumMax)
    {
        ExitOnFail(__LINE__);
    }
    if (c.SizeMax() != itemsNumMax)
    {
        ExitOnFail(__LINE__);
    }
}
static void Test_STD_ARRAY_UTILITY_VECTORSAFE_ARRAYSAFE_CONST_METHODS()
{
    {
        const size_t kTestElementsNumNotFull = kTestElementsNum - 4;
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNumNotFull);//VectorSafe isn't full
        TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_ARRAYSAFE_CONST_METHODS(v, v.size(), kTestElementsNum);
    }
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNum);//VectorSafe is full
        TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_ARRAYSAFE_CONST_METHODS(v, v.size(), kTestElementsNum);

        ArraySafe<size_t, kTestElementsNum> a(&s_arrayTest[0], kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_ARRAYSAFE_CONST_METHODS(a, a.size(), kTestElementsNum);
    }
}

template<class T>
static void TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS(T c)
{
    if (c.SizeInBytes() != c.size()*sizeof(size_t))
    {
        ExitOnFail(__LINE__);
    }
    if (c.empty())
    {
        ExitOnFail(__LINE__);
    }
    for(size_t i=0; i < c.size(); ++i)
    {
        if (c.Find(i) != CastWithAssert<size_t,int>(i))
        {
            ExitOnFail(__LINE__);
        }
    }
    if (c.Find(c.size()) != -1)
    {
        ExitOnFail(__LINE__);
    }
    if (c.empty())
    {
        ExitOnFail(__LINE__);
    }

}
template<class T>
static void TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS_empty(T c)
{
    if (!c.empty())
    {
        ExitOnFail(__LINE__);
    }
}
static void Test_STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS()
{
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNum - 4);//VectorSafe isn't full
        TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS(v);
        TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS(VectorSafeRef<size_t>(&v));
        TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS(ConstVectorSafeRef<size_t>(v));
    }
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNum);//VectorSafe is full
        TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS(v);
        TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS(VectorSafeRef<size_t>(&v));
        TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS(ConstVectorSafeRef<size_t>(v));
    }
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], 0);//VectorSafe is empty
        TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS_empty(v);
        TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS_empty(VectorSafeRef<size_t>(&v));
        TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS_empty(ConstVectorSafeRef<size_t>(v));
    }
}

template<class T>
static void TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_string(T c)
{
    {
        //not full
        c.Sprintf("New %s %i", "string", 37);
        const char*const resultantString = &"New string 37"[0];
        if (strncmp(resultantString, c.data(), strlen(resultantString) + 1) != 0)
        {
            ExitOnFail(__LINE__);
        }
    }
    {
        //full
        c.Sprintf("New %s %i", "string!!", 37);
        const char*const resultantString = &"New string!! 37"[0];
        if (strncmp(resultantString, c.data(), strlen(resultantString) + 1) != 0)
        {
            ExitOnFail(__LINE__);
        }
    }
}

uint32_t s_freadUnitTestDotBin[kTestElementsNum] = { 15,0,14,1,13,2,12,3,11,4,10,5,9,6,8,7 };//duplicated in FreadUnitTest.bin
uint32_t s_notFreadUnitTestDotBin[kTestElementsNum] = { 0,1,2,3,15,14,13,12,4,5,6,7,11,10,9,8 };//duplicated in FreadUnitTest.bin

template<class T>
static void TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS(T c, const size_t itemsNum)
{
    const size_t addedIncrement = 1;
    size_t added = 1;
    {
        for (size_t i = 0; i < itemsNum; ++i)
        {
            *(c.data() + i) += addedIncrement;
        }
        for (size_t i = 0; i < itemsNum; ++i)
        {
            if (*(c.data() + i) != s_arrayTest[i] + added)
            {
                ExitOnFail(__LINE__);
            }
        }
        added += addedIncrement;
    }

    {
        for (size_t i = 0; i < itemsNum; ++i)
        {
            c.GetChecked(i) += addedIncrement;
        }
        for (size_t i = 0; i < itemsNum; ++i)
        {
            if (c.GetChecked(i) != s_arrayTest[i] + added)
            {
                ExitOnFail(__LINE__);
            }
        }
        ++added;
    }

    {
        for (size_t i = 0; i < itemsNum; ++i)
        {
            c[i] += addedIncrement;
        }
        for (size_t i = 0; i < itemsNum; ++i)
        {
            if (c[i] != s_arrayTest[i] + added)
            {
                ExitOnFail(__LINE__);
            }
        }
        ++added;
    }

    {
        for (size_t i = 0; i < itemsNum; ++i)
        {
            c.at(i) += addedIncrement;
        }
        for (size_t i = 0; i < itemsNum; ++i)
        {
            if (c.at(i) != s_arrayTest[i] + added)
            {
                ExitOnFail(__LINE__);
            }
        }
        //++added;//all done adding
    }

    if(c.front() != s_arrayTest[0] + added)
    {
        ExitOnFail(__LINE__);
    }

    if (*c.begin() != s_arrayTest[0] + added)
    {
        ExitOnFail(__LINE__);
    }

    {
        T cOriginal(c);

        const size_t vBase = 256;
        VectorSafe<size_t, 3> v({vBase,vBase+1,vBase+2});
        const size_t indexToMemcpyFrom = 10;
        const size_t vSizePlusIndexToMemcpyFrom = v.size() + indexToMemcpyFrom;
        const size_t resultSize = itemsNum > vSizePlusIndexToMemcpyFrom ? itemsNum : vSizePlusIndexToMemcpyFrom;
        c.MemcpyFromIndex(v.data(), indexToMemcpyFrom, v.SizeInBytes());///<@todo: consider convenience function that copies entire VectorSafe*

        //#TestMemcpyFromIndex: make sure elements are correctly written; size() testing is elsewhere
        for (size_t i = 0; i < indexToMemcpyFrom; ++i)
        {
            if (cOriginal[i] != c[i])
            {
                ExitOnFail(__LINE__);
            }
        }
        for (size_t i = indexToMemcpyFrom; i < indexToMemcpyFrom + v.size(); ++i)
        {
            if (v[i-indexToMemcpyFrom] != c[i])
            {
                ExitOnFail(__LINE__);
            }
        }
        for (size_t i = indexToMemcpyFrom + v.size(); i < itemsNum; ++i)
        {
            if (cOriginal[i] != c[i])
            {
                ExitOnFail(__LINE__);
            }
        }

        //this case asserts for a container of size 12, because 
        //c.MemcpyFromIndex(v.data(), 14, v.SizeInBytes());
    }

    {
        T cOriginal(c);
        const size_t vBase = 256;
        VectorSafe<size_t, 3> v({ vBase,vBase + 1,vBase + 2 });
        c.MemcpyFromStart(v.data(), v.SizeInBytes());

        for (size_t i = 0; i < v.size(); ++i)
        {
            if (v[i] != c[i])
            {
                ExitOnFail(__LINE__);
            }
        }
    }
}

template<class T>
static void TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_Fread(T c)
{
    T cOriginal(c);

    {
        FILE*const f = fopen("FreadUnitTest.bin", "rb");
        c.Fread(f, kTestElementsNum);
        for (size_t i = 0; i < kTestElementsNum; ++i)
        {
            if (c[i] != s_freadUnitTestDotBin[i])
            {
                ExitOnFail(__LINE__);
            }
        }
        fclose(f);
    }

    {
        c = cOriginal;
        FILE*const f = fopen("FreadUnitTest.bin", "rb");
        c.MemcpyFromFread(f, kTestElementsNum);

        for (size_t i = 0; i < kTestElementsNum; ++i)
        {
            if (c[i] != s_freadUnitTestDotBin[i])
            {
                ExitOnFail(__LINE__);
            }
        }
        fclose(f);
    }
}
static void Test_STD_ARRAY_UTILITY_NON_CONST_METHODS()
{
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNum - 4);//VectorSafe isn't full
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS(v, v.size());
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS(VectorSafeRef<size_t>(&v), v.size());
    }
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNum);//VectorSafe is full
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS(v, v.size());
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS(VectorSafeRef<size_t>(&v), v.size());
    }
    {
        ArraySafe<size_t, kTestElementsNum> a(&s_arrayTest[0], kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS(a, kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS(ArraySafeRef<size_t>(&a), kTestElementsNum);
    }

    const size_t strlenStr = strlen(&s_strTest[0]);
    {
        VectorSafe<char, kTestElementsNum> v(&s_strTest[0], kTestElementsNum - 4);//VectorSafe isn't full
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_string(v);
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_string(VectorSafeRef<char>(&v));
    }
    {
        VectorSafe<char, kTestElementsNum> v(&s_strTest[0], kTestElementsNum);//VectorSafe is full
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_string(v);
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_string(VectorSafeRef<char>(&v));
    }
    {
        ArraySafe<char, kTestElementsNum> a(&s_strTest[0], kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_string(a);
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_string(ArraySafeRef<char>(&a));
    }

    
    {
        VectorSafe<uint32_t, kTestElementsNum> v(&s_notFreadUnitTestDotBin[0], kTestElementsNum - 4);//VectorSafe isn't full
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_Fread(v);
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_Fread(VectorSafeRef<uint32_t>(&v));
    }
    {
        VectorSafe<uint32_t, kTestElementsNum> v(&s_notFreadUnitTestDotBin[0], kTestElementsNum);//VectorSafe is full
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_Fread(v);
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_Fread(VectorSafeRef<uint32_t>(&v));
    }
    {
        ArraySafe<uint32_t, kTestElementsNum> a(&s_notFreadUnitTestDotBin[0], kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_Fread(a);
        TestImpl_STD_ARRAY_UTILITY_NON_CONST_METHODS_Fread(ArraySafeRef<uint32_t>(&a));
    }
}

template<typename T, typename U>
static void TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(T& c, const U& other, const size_t numElements)
{
    c.MemcpyFromStart(other);
    for (size_t i = 0; i < numElements; ++i)
    {
        if (c[i] != other[i])
        {
            ExitOnFail(__LINE__);
        }
    }

    c.MemsetEntireArray(0);
    for (size_t i = 0; i < numElements; ++i)
    {
        if (c[i] != 0)
        {
            ExitOnFail(__LINE__);
        }
    }
}

template<class T>
static void TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS(T c)
{
    if (c.back() != *(c.end()-1))
    {
        ExitOnFail(__LINE__);
    }

    {
        VectorSafe<size_t, kTestElementsNum> vNotFull(&s_arrayTestOther[0], kTestElementsNum - 4);//VectorSafe isn't full
        VectorSafe<size_t, kTestElementsNum> vFull(&s_arrayTestOther[0], kTestElementsNum);//VectorSafe is full
        ArraySafe<size_t, kTestElementsNum> a(&s_arrayTestOther[0], kTestElementsNum);

        const VectorSafe<size_t, kTestElementsNum> cOriginal(c);

        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, vNotFull, vNotFull.size());
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, cOriginal, cOriginal.size());
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, vFull, vFull.size());
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, cOriginal, cOriginal.size());
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, a, kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, cOriginal, cOriginal.size());

        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, VectorSafeRef<size_t>(&vNotFull), vNotFull.size());
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, cOriginal, cOriginal.size());
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, VectorSafeRef<size_t>(&vFull), vFull.size());
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, cOriginal, cOriginal.size());

        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, ConstVectorSafeRef<size_t>(vNotFull), vNotFull.size());
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, cOriginal, cOriginal.size());
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, ConstVectorSafeRef<size_t>(vFull), vFull.size());
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_Copy(c, cOriginal, cOriginal.size());
    }
}
static void Test_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS()
{
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNum - 4);//VectorSafe isn't full
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS(v);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS(VectorSafeRef<size_t>(&v));
    }
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNum);//VectorSafe is full
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS(v);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS(VectorSafeRef<size_t>(&v));
    }
    {
        ArraySafe<size_t, kTestElementsNum> a(&s_arrayTest[0], kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS(a);
    }
}

const int kDuplicateTestMaxNum = 7;
template<class T, class S> 
static void TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS(T vectorSafeDuplicateTest, S vAppend0, S vAppend1)
{
    vectorSafeDuplicateTest.SortAndRemoveDuplicates();
    if (vectorSafeDuplicateTest.size() != kDuplicateTestMaxNum + 1)
    {
        ExitOnFail(__LINE__);
    }
    for (int i = 0; i < kDuplicateTestMaxNum; ++i)
    {
        if (vectorSafeDuplicateTest[i] != i)
        {
            ExitOnFail(__LINE__);
        }
    }

    vAppend0.Append(vAppend1);
    if (vAppend0.size() != 4)
    {
        ExitOnFail(__LINE__);
    }
    for (size_t i = 0; i < vAppend0.size(); ++i)
    {
        if (i != vAppend0[i])
        {
            ExitOnFail(__LINE__);
        }
    }

    size_t toPush = 4;
    {
        const size_t originalSize = vAppend0.size();
        vAppend0.Push(toPush);
        if (vAppend0.size() != originalSize + 1)
        {
            ExitOnFail(__LINE__);
        }
        if (vAppend0.back() != toPush)
        {
            ExitOnFail(__LINE__);
        }
    }

    {
        const size_t originalSize = vAppend0.size();
        vAppend0.PushIfUnique(toPush);
        if (vAppend0.size() != originalSize)
        {
            ExitOnFail(__LINE__);
        }

        const size_t five = 5;
        vAppend0.PushIfUnique(five);
        if (vAppend0.size() != originalSize + 1)
        {
            ExitOnFail(__LINE__);
        }
        if (vAppend0.back() != five)
        {
            ExitOnFail(__LINE__);
        }
    }
    {
        const size_t originalSize = vAppend0.size();
        vAppend0.Pop();
        if (vAppend0.size() != originalSize - 1)
        {
            ExitOnFail(__LINE__);
        }
        if (vAppend0.back() != originalSize - 2)
        {
            ExitOnFail(__LINE__);
        }
    }

    {
        const size_t originalSize = vAppend0.size();
        vAppend0.RemoveItemAtIndex(3);
        if (vAppend0.size() != originalSize - 1)
        {
            ExitOnFail(__LINE__);
        }
        if (vAppend0[3] != 4)
        {
            ExitOnFail(__LINE__);
        }

        if (vAppend0.Remove(3))
        {
            ExitOnFail(__LINE__);
        }
        if (!vAppend0.Remove(2))
        {
            ExitOnFail(__LINE__);
        }
        if (vAppend0[2] != 4)
        {
            ExitOnFail(__LINE__);
        }
        if (vAppend0.size() != originalSize - 2)
        {
            ExitOnFail(__LINE__);
        }
    }
}
template<class T>
static void TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_MemcpyFromIndexSize(T c)
{
    {
        const size_t vBase = 256;
        VectorSafe<size_t, 3> v({ vBase,vBase + 1,vBase + 2 });
        const size_t indexToMemcpyFrom = 10;
        const size_t vSizePlusIndexToMemcpyFrom = v.size() + indexToMemcpyFrom;
        const size_t resultSize = c.size() > vSizePlusIndexToMemcpyFrom ? c.size() : vSizePlusIndexToMemcpyFrom;
        c.MemcpyFromIndex(v.data(), indexToMemcpyFrom, v.SizeInBytes());///<@todo: consider convenience function that copies entire VectorSafe*

        //#TestMemcpyFromIndex: make sure size() is correct; element testing is elsewhere
        if (c.size() != resultSize)
        {
            ExitOnFail(__LINE__);
        }
    }
}
static void Test_STD_ARRAY_UTILITY_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS()
{
    const int kElementsMax = 16;
    VectorSafe<int, kElementsMax> vectorSafeDuplicateTest({ 0,4,3,1,1,1,2,5,6,6,kDuplicateTestMaxNum,5,5 });
    const size_t kAppendVectorSize = 6;
    VectorSafe<size_t, kAppendVectorSize> vAppend0({ 0,1 });
    VectorSafe<size_t, kAppendVectorSize> vAppend1({ 2,3 });

    TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS(vectorSafeDuplicateTest, vAppend0, vAppend1);
    TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS(
        VectorSafeRef<int>(&vectorSafeDuplicateTest),
        VectorSafeRef<size_t>(&vAppend0),
        VectorSafeRef<size_t>(&vAppend1));

    VectorSafe<size_t, kTestElementsNum> vNotFull(&s_arrayTestOther[0], kTestElementsNum - 4);//VectorSafe isn't full
    VectorSafe<size_t, kTestElementsNum> vFull(&s_arrayTestOther[0], kTestElementsNum);//VectorSafe is full
    TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_MemcpyFromIndexSize(vNotFull);
    TestImpl_STD_ARRAY_UTILITY_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS_MemcpyFromIndexSize(vFull);
}
template<class T>
static void TestImpl_STD_ARRAY_UTILITY_CONST_METHODS(T c)
{
    const char*const testString = &s_strTest[0];
    if (strncmp(c.data(), testString, strlen(testString)) != 0)
    {
        ExitOnFail(__LINE__);
    }
    if (c.data() != c.GetAddressOfUnderlyingArray())
    {
        ExitOnFail(__LINE__);
    }
}
template<class T>
static void TestImpl_STD_ARRAY_UTILITY_CONST_METHODS_nonString(T c, const size_t itemsNum, const size_t itemsNumMax)
{
    const size_t testIndex = 11;
    if (testIndex != c.GetChecked(testIndex))
    {
        ExitOnFail(__LINE__);
    }

    {
        FILE*const fWrite = fopen("TestImpl_STD_ARRAY_UTILITY_CONST_METHODS_nonString.bin", "wb");
        c.Fwrite(fWrite, itemsNum);
        fclose(fWrite);

        FILE*const fRead = fopen("TestImpl_STD_ARRAY_UTILITY_CONST_METHODS_nonString.bin", "r");
        VectorSafe<T::value_type, 32> cReadback;
        cReadback.Fread(fWrite, itemsNum);
        if (memcmp(cReadback.data(), c.data(), sizeof(T::value_type)*itemsNum) != 0)
        {
            ExitOnFail(__LINE__);
        }
        fclose(fRead);
    }

    if (c[3] != c.at(3))
    {
        ExitOnFail(__LINE__);
    }
    if (c.front() != *c.begin())
    {
        ExitOnFail(__LINE__);
    }
    if (c.cbegin() != c.begin())
    {
        ExitOnFail(__LINE__);
    }
}
static void Test_STD_ARRAY_UTILITY_CONST_METHODS()
{
    const size_t strlenStr = strlen(&s_strTest[0]);
    {
        VectorSafe<char, kTestElementsNum> v(&s_strTest[0], strlenStr);//VectorSafe isn't full
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS(v);
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS(VectorSafeRef<char>(&v));
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS(ConstVectorSafeRef<char>(v));
    }
    {
        VectorSafe<char, kTestElementsNum> v(&s_strTest[0], kTestElementsNum);//VectorSafe is full
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS(v);
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS(VectorSafeRef<char>(&v));
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS(ConstVectorSafeRef<char>(v));
    }
    {
        ArraySafe<char, kTestElementsNum> a(&s_strTest[0], strlenStr);
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS(a);
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS(ArraySafeRef<char>(&a));
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS(ConstArraySafeRef<char>(a));
    }


    {
        const size_t kTestElementNumNotFull = kTestElementsNum - 4;
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementNumNotFull);//VectorSafe isn't full
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS_nonString(v, v.size(), kTestElementNumNotFull);
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS_nonString(VectorSafeRef<size_t>(&v), v.size(), kTestElementNumNotFull);
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS_nonString(ConstVectorSafeRef<size_t>(v), v.size(), kTestElementNumNotFull);
    }
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNum);//VectorSafe is full
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS_nonString(v, v.size(), kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS_nonString(VectorSafeRef<size_t>(&v), v.size(), kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS_nonString(ConstVectorSafeRef<size_t>(v), v.size(), kTestElementsNum);
    }
    {
        ArraySafe<size_t, kTestElementsNum> a(&s_arrayTest[0], kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS_nonString(a, a.size(), kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS_nonString(ArraySafeRef<size_t>(&a), a.size(), kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_CONST_METHODS_nonString(ConstArraySafeRef<size_t>(a), a.size(), kTestElementsNum);
    }
}

template<class T>
static void TestImpl_STD_ARRAY_UTILITY_ARRAYSAFEREF_VECTORSAFEREF_NON_CONST_METHODS(T c)
{
    {
        c.SetArray(&s_arrayTestOther[0], sizeof(s_arrayTestOther) / sizeof(s_arrayTestOther[0]));
        if (memcmp(c.GetAddressOfUnderlyingArray(), &s_arrayTestOther[0], sizeof(s_arrayTestOther)) != 0)
        {
            ExitOnFail(__LINE__);
        }
    }
    {
#if STD_UTILITY_DEBUG
        if (c.size() == 0)
        {
            ExitOnFail(__LINE__);
        }
#endif//#if STD_UTILITY_DEBUG
        c.Reset();
#if STD_UTILITY_DEBUG
        if (c.size() != 0)
        {
            ExitOnFail(__LINE__);
        }
#endif//#if STD_UTILITY_DEBUG
        if (c.data() != nullptr)
        {
            ExitOnFail(__LINE__);
        }
    }
}
static void Test_STD_ARRAY_UTILITY_ARRAYSAFEREF_VECTORSAFEREF_NON_CONST_METHODS()
{
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNum - 4);//VectorSafe isn't full
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFEREF_VECTORSAFEREF_NON_CONST_METHODS(VectorSafeRef<size_t>(&v));
    }
    {
        VectorSafe<size_t, kTestElementsNum> v(&s_arrayTest[0], kTestElementsNum);//VectorSafe is full
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFEREF_VECTORSAFEREF_NON_CONST_METHODS(VectorSafeRef<size_t>(&v));
    }
    {
        ArraySafe<size_t, kTestElementsNum> a(&s_arrayTest[0], kTestElementsNum);
        TestImpl_STD_ARRAY_UTILITY_ARRAYSAFEREF_VECTORSAFEREF_NON_CONST_METHODS(ArraySafeRef<size_t>(&a));
    }
}

template<class T, size_t kMaxSize>
static void ConstMethodTesting(const VectorSafe<T,kMaxSize>& vectorSafe, const int actualSize, const int lastValidValue)
{
    //const function testing
    for (int i = 0; i < actualSize; ++i)
    {
        if (vectorSafe[i] != i)
        {
            ExitOnFail(__LINE__);
        }
    }
    for (int i = 0; i < actualSize; ++i)
    {
        if (vectorSafe.at(i) != i)
        {
            ExitOnFail(__LINE__);
        }
    }
    if (vectorSafe.front() != 0)
    {
        ExitOnFail(__LINE__);
    }
    if (vectorSafe.back() != lastValidValue)
    {
        ExitOnFail(__LINE__);
    }


    {
        auto vectorSafeEnd = vectorSafe.end();
        int index = 0;
        for (auto it = vectorSafe.begin(); it != vectorSafeEnd; ++it)
        {
            if (*it != index)
            {
                ExitOnFail(__LINE__);
            }
            ++index;
        }
    }
    {
        auto vectorSafeEnd = vectorSafe.cend();
        int index = 0;
        for (auto it = vectorSafe.begin(); it != vectorSafeEnd; ++it)
        {
            if (*it != index)
            {
                ExitOnFail(__LINE__);
            }
            ++index;
        }
    }
    //{
    //    auto vectorSafeREnd = vectorSafe.rend();
    //    int index = lastValidValue;
    //    for (auto it = vectorSafe.crbegin(); it != vectorSafeREnd; ++it)
    //    {
    //        if (*it != index)
    //        {
    //            ExitOnFail(__LINE__);
    //        }
    //        --index;
    //    }
    //}
}

template<class T, size_t kMaxElements>
static void AddFirstThreeElements(QueueCircular<T, kMaxElements>*const testPtr)
{
    NTF_REF(testPtr, test);

    test.Enqueue(0);
    assert(test.Size() == 1);
    test.Enqueue(1);
    assert(test.Size() == 2);
    test.Enqueue(2);
    assert(test.Size() == 3);
}

//#define since a templated function bizarrely can't decide if "bool operator !=(const GUID &,const GUID &)" or the obviously correct operator!= should be called on VS2015
#define STD_ARRAY_UTILITY_UNIT_TEST_FAIL_IF_NOT_EQUIVALENT(t,u) \
{                                                               \
    if (t != u)                                                 \
    {                                                           \
        ExitOnFail(__LINE__);                                   \
    }                                                           \
}

template<class T, class U>
void FailIfNotEquivalentRawMemcmp(T t, U u, const size_t elementsNum)
{
    NTF_STATIC_ASSERT(sizeof(T::value_type) == sizeof(U::value_type));
    if (memcmp(t.data(), u.data(), sizeof(T::value_type)*elementsNum))
    {
        ExitOnFail(__LINE__);
    }
}

void FreadTest(const char*const testFileName)
{
    const char*const kTestStringFinal = "Testing 66 lots of things for testing";
    FILE* f;
    Fopen(&f, testFileName, "r");
    ArraySafe<char, 256> buf;
    buf.Fread(f, strlen(kTestStringFinal) + 1);
    Fclose(f);
    if (strcmp(buf.data(), &kTestStringFinal[0]) != 0)
    {
        ExitOnFail(__LINE__);
    }
}

int main()
{
    //BEG_#VectorSafe
    enum { kElementsMax = 16 };
    VectorSafe<int, kElementsMax> vectorSafe;

    const size_t actualSize = 12;
    vectorSafe.size(actualSize);
    if (vectorSafe.SizeInBytes() != actualSize * sizeof(int))
    {
        ExitOnFail(__LINE__);
    }
    if (vectorSafe.SizeMaxInBytes() != kElementsMax * sizeof(int))
    {
        ExitOnFail(__LINE__);
    }
    for (int i = 0; i < actualSize; ++i)
    {
        vectorSafe[i] = i;
    }
    for (int i = 0; i < actualSize; ++i)
    {
        if (vectorSafe.GetChecked(i) != i)
        {
            ExitOnFail(__LINE__);
        }
    }
    const int lastValidValue = actualSize - 1;
    if (vectorSafe.GetLastValidIndex() != lastValidValue)
    {
        ExitOnFail(__LINE__);
    }
    if (vectorSafe.GetOneAfterLastValidIndex() != actualSize)
    {
        ExitOnFail(__LINE__);
    }
    for (int i = 0; i < actualSize; ++i)
    {
        if (vectorSafe[i] != i)
        {
            ExitOnFail(__LINE__);
        }
    }
    for (int i = 0; i < actualSize; ++i)
    {
        if (vectorSafe.at(i) != i)
        {
            ExitOnFail(__LINE__);
        }
    }
    if (vectorSafe.front() != 0)
    {
        ExitOnFail(__LINE__);
    }
    if (vectorSafe.back() != lastValidValue)
    {
        ExitOnFail(__LINE__);
    }
    {
        auto vectorSafeEnd = vectorSafe.end();
        int index = 0;
        for (auto it = vectorSafe.begin(); it != vectorSafeEnd; ++it)
        {
            if (*it != index)
            {
                ExitOnFail(__LINE__);
            }
            ++index;
        }
    }

    //{
    //    auto vectorSafeREnd = vectorSafe.rend();
    //    int index = lastValidValue;
    //    for (auto it = vectorSafe.rbegin(); it != vectorSafeREnd; ++it)
    //    {
    //        if (*it != index)
    //        {
    //            ExitOnFail(__LINE__);
    //        }
    //        --index;
    //    }
    //}

    if (vectorSafe.empty())
    {
        ExitOnFail(__LINE__);
    }
    VectorSafe<int, kElementsMax> vectorSafeEmpty;
    vectorSafeEmpty.size(0);
    if (!vectorSafeEmpty.empty())
    {
        ExitOnFail(__LINE__);
    }
    if (vectorSafeEmpty.SizeInBytes() != 0)
    {
        ExitOnFail(__LINE__);
    }

    if (vectorSafeEmpty == vectorSafe)
    {
        ExitOnFail(__LINE__);
    }
    if (!(vectorSafeEmpty != vectorSafe))
    {
        ExitOnFail(__LINE__);
    }

    VectorSafe<int, kElementsMax> vectorSafeTwo(vectorSafe.size());
    for (size_t i = 0; i < vectorSafe.size(); ++i)
    {
        vectorSafeTwo[i] = vectorSafe[i];
    }

    if (vectorSafe != vectorSafeTwo)
    {
        ExitOnFail(__LINE__);
    }
    vectorSafeTwo[3] = 666;
    if (vectorSafe == vectorSafeTwo)
    {
        ExitOnFail(__LINE__);
    }

    const int vectorSafeTwoOldBack = vectorSafeTwo.back();
    vectorSafeTwo.Push(static_cast<int>(vectorSafeTwo.size()));
    if (vectorSafeTwo.back() != static_cast<int>(vectorSafeTwo.size()) - 1 || vectorSafeTwo.size() != vectorSafe.size() + 1)
    {
        ExitOnFail(__LINE__);
    }
    vectorSafeTwo.sizeDecrement();
    if (vectorSafeTwo.size() != vectorSafe.size() || vectorSafeTwo.back() != vectorSafeTwoOldBack)
    {
        ExitOnFail(__LINE__);
    }

    VectorSafe<int, kElementsMax> vectorSafeCopy;
    vectorSafeCopy.MemcpyFromStart(vectorSafe);
    if (vectorSafeCopy != vectorSafe)
    {
        ExitOnFail(__LINE__);
    }

    VectorSafe<int, kElementsMax> vectorSafeInitializerList({ 0, 1, 2 });
    for (int i = 0; i < 3; ++i)
    {
        if (vectorSafeInitializerList[i] != i)
        {
            ExitOnFail(__LINE__);
        }
    }

    Test_STD_ARRAY_UTILITY_CONST_METHODS();
    Test_STD_ARRAY_UTILITY_VECTORSAFE_ARRAYSAFE_CONST_METHODS();
    Test_STD_ARRAY_UTILITY_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS();
    Test_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_CONST_METHODS();
    Test_STD_ARRAY_UTILITY_ARRAYSAFE_ARRAYSAFEREF_CONSTARRAYSAFEREF_CONSTVECTORSAFEREF_METHODS();
    Test_STD_ARRAY_UTILITY_VECTORSAFE_CONST_METHODS();
    Test_STD_ARRAY_UTILITY_NON_CONST_METHODS();
    Test_STD_ARRAY_UTILITY_ARRAYSAFE_VECTORSAFE_VECTORSAFEREF_NON_CONST_METHODS();
    Test_STD_ARRAY_UTILITY_ARRAYSAFEREF_VECTORSAFEREF_NON_CONST_METHODS();
    Test_STD_ARRAY_UTILITY_ARRAYSAFE_CONSTARRAYSAFE_CONSTVECTORSAFE_METHODS();

    {
        const size_t alignmentBytes = sizeof(size_t);
        size_t*const alignedMemPtr = reinterpret_cast<size_t*>(AlignedMalloc(kElementsMax, alignmentBytes));

        {
            //these should assert if executed:
            {
                //ConstVectorSafeRef<size_t> vAligned(reinterpret_cast<size_t*>(reinterpret_cast<uint8_t*>(alignedMemPtr) + 1), kElementsMax, alignmentBytes);
                //ArraySafeRef<size_t> aAligned(reinterpret_cast<size_t*>(reinterpret_cast<uint8_t*>(alignedMemPtr) + 1), kElementsMax, alignmentBytes);
                //ConstArraySafeRef<size_t> aAlignedConst(reinterpret_cast<size_t*>(reinterpret_cast<uint8_t*>(alignedMemPtr) + 1), kElementsMax, alignmentBytes);
            }

            {
                //ConstVectorSafeRef<uint8_t> vAligned(reinterpret_cast<uint8_t*>(alignedMemPtr), kElementsMax, alignmentBytes);
                //ArraySafeRef<uint8_t> aAligned(reinterpret_cast<uint8_t*>(alignedMemPtr), kElementsMax, alignmentBytes);
                //ConstArraySafeRef<uint8_t> aAlignedConst(reinterpret_cast<uint8_t*>(alignedMemPtr), kElementsMax, alignmentBytes);
            }
        }

        VectorSafe<size_t, kElementsMax> v(alignedMemPtr, kElementsMax);
        ArraySafe<size_t, kElementsMax> a(alignedMemPtr, kElementsMax);

        {
            ConstVectorSafeRef<size_t> vAlignedConst(alignedMemPtr, kElementsMax, alignmentBytes);
            ArraySafeRef<size_t> aAligned(alignedMemPtr, kElementsMax, alignmentBytes);
            ConstArraySafeRef<size_t> aAlignedConst(alignedMemPtr, kElementsMax, alignmentBytes);

            //if the *Ref instances are equivalent to the (thoroughly-tested elsewhere) regular Instances, then we can conclude they were constructed correctly and will operate as expected
            STD_ARRAY_UTILITY_UNIT_TEST_FAIL_IF_NOT_EQUIVALENT(v, vAlignedConst);
            FailIfNotEquivalentRawMemcmp(a, aAligned, kElementsMax);
            FailIfNotEquivalentRawMemcmp(a, aAlignedConst, kElementsMax);


            ConstVectorSafeRef<size_t> vUnalignedConst(alignedMemPtr, kElementsMax);
            ArraySafeRef<size_t> aUnaligned(alignedMemPtr, kElementsMax);
            ConstArraySafeRef<size_t> aUnalignedConst(alignedMemPtr, kElementsMax);

            STD_ARRAY_UTILITY_UNIT_TEST_FAIL_IF_NOT_EQUIVALENT(v, vUnalignedConst);
            FailIfNotEquivalentRawMemcmp(a, aUnaligned, kElementsMax);
            FailIfNotEquivalentRawMemcmp(a, aUnalignedConst, kElementsMax);
        }

        AlignedFree(alignedMemPtr);
    }

    {
        const char*const testFileName = "FwriteSprintfTest.txt";
        FILE* f;
        Fopen(&f, testFileName, "w");
        const char*const initialString = "Testing %i lots of things %s";
        const int testInt = 66;
        const char*const testString = "for testing";
        FwriteSprintf(f, initialString, testInt, testString);
        Fclose(f);

        FreadTest(testFileName);

        Fopen(&f, testFileName, "w");
        CRITICAL_SECTION criticalSection;
        CriticalSectionCreate(&criticalSection);
        FwriteSprintf(f, &criticalSection, initialString, testInt, testString);
        Fclose(f);
        CriticalSectionDelete(&criticalSection);

        FreadTest(testFileName);
    }

    //old tests below are kept because why not -- maybe a little extra coverage hiding in here
    ConstMethodTesting(vectorSafe, actualSize, lastValidValue);

    VectorSafe<int, kElementsMax> vectorSafeRemove({ 0,1,2,3,4 });
    size_t vectorSafeRemoveSize = vectorSafeRemove.size();
    --vectorSafeRemoveSize;
    vectorSafeRemove.Remove(1);
    assert(vectorSafeRemove.size() == vectorSafeRemoveSize);
    assert(vectorSafeRemove[0] == 0);
    assert(vectorSafeRemove[1] == 4);
    assert(vectorSafeRemove[2] == 2);
    assert(vectorSafeRemove[3] == 3);

    --vectorSafeRemoveSize;
    vectorSafeRemove.RemoveItemAtIndex(vectorSafeRemove.GetLastValidIndex());
    assert(vectorSafeRemove.size() == vectorSafeRemoveSize);
    assert(vectorSafeRemove[0] == 0);
    assert(vectorSafeRemove[1] == 4);
    assert(vectorSafeRemove[2] == 2);

    --vectorSafeRemoveSize;
    vectorSafeRemove.Remove(0);
    assert(vectorSafeRemove.size() == vectorSafeRemoveSize);
    assert(vectorSafeRemove[0] == 2);
    assert(vectorSafeRemove[1] == 4);
    //END_#VectorSafe

    //BEG_#VectorSafeRef
    VectorSafeRef<int>vectorSafeRemoveSizeRef(&vectorSafeRemove);
    const size_t vectorSafeRemoveSizeRefOriginalSize = vectorSafeRemoveSizeRef.size();
    vectorSafeRemoveSizeRef.sizeIncrement();
    assert(vectorSafeRemoveSizeRefOriginalSize == vectorSafeRemoveSizeRef.size() - 1);
    vectorSafeRemoveSizeRef.sizeDecrement();
    assert(vectorSafeRemoveSizeRefOriginalSize == vectorSafeRemoveSizeRef.size());
    //END_#VectorSafeRef


    //BEG_#QueueCircular
    const int size = 3;
    QueueCircular<int, 3> test;
    test.Clear();
    assert(test.Size() == 0);

    AddFirstThreeElements(&test);
#if NTF_DEBUG
    int lastQueuedItem, nextItemToDequeue;
#endif//#if NTF_DEBUG
    assert(test.PeekLastQueuedItem(&lastQueuedItem));
    assert(lastQueuedItem == 2);
    assert(test.PeekNextItemToDequeue(&nextItemToDequeue));
    assert(nextItemToDequeue == 0);

    assert(test[0] == 0);
    assert(test[1] == 1);
    assert(test[2] == 2);

    int dequeued = test.Dequeue();
    assert(dequeued == 0);
    assert(test.Size() == 2);

    dequeued = test.Dequeue();
    assert(dequeued == 1);
    assert(test.Size() == 1);
    assert(test.PeekLastQueuedItem(&lastQueuedItem));
    assert(lastQueuedItem == 2);

    test.Clear();
    assert(test.Size() == 0);

    AddFirstThreeElements(&test);
    assert(test.Size() == size);
    test.Clear();
    assert(test.Size() == 0);

    AddFirstThreeElements(&test);
    assert(test.Size() == size);
    test.Clear();
    assert(test.Size() == 0);

    test.Enqueue(1);
    assert(test.PeekLastQueuedItem(&lastQueuedItem));
    assert(lastQueuedItem == 1);
    assert(test.Size() == 1);
    test.Clear();

    AddFirstThreeElements(&test);
    test.Dequeue(2);
    assert(test.Size() == 1);
    assert(test.PeekLastQueuedItem(&lastQueuedItem));
    assert(lastQueuedItem == 2);
    test.Clear();

    QueueCircular<int,4> testSize4;
    AddFirstThreeElements(&testSize4);
    testSize4.Enqueue(58);
    testSize4[0] = 45;
    testSize4[2] = 34;
    testSize4[3] = 66;
    assert(testSize4.Size() == 4);
    assert(testSize4[0] == 45);
    assert(testSize4[1] == 1);
    assert(testSize4[2] == 34);
    assert(testSize4[3] == 66);

    //BEG_#QueueCircularIterator
    //size_t index = 0;
    //for (auto& element : testSize4)
    //{
    //    switch (index)
    //    {
    //        case 0:
    //        {
    //            assert(element == 45);
    //            ++index;
    //            break;
    //        }
    //        case 1:
    //        {
    //            assert(element == 1);
    //            ++index;
    //            break;
    //        }
    //        case 2:
    //        {
    //            assert(element == 34);
    //            ++index;
    //            break;
    //        }
    //        case 3:
    //        {
    //            assert(element == 66);
    //            ++index;
    //            break;
    //        }
    //        default:
    //        {
    //            assert(false);
    //            break;
    //        }
    //    }
    //}
    //assert(index == testSize4.Size());
    //END_#QueueCircularIterator

    test.Clear();
    //END_#QueueCircular

    //BEG_#CastWithAssert
    const uint32_t failOnx86_successOnx64 = CastWithAssert<uint64_t, uint32_t>(((size_t)(1) << 31) - 1);
    //const uint32_t failOnx86_successOnx64 = CastWithAssert<uint64_t, uint32_t>(((size_t)(1) << 32) - 1);
    //const uint32_t fail = CastWithAssert<uint64_t, uint32_t>((size_t)(1) << 32);
    //END_#CastWithAssert

    //done!
    printf("Unit test SUCCESS!\n");
    ConsolePauseForUserAcknowledgement();
    return 0;
}

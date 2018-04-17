#include <stdio.h>
#include <stdlib.h>
#include"../VulkanTest01/stdArrayUtility.h"

//don't complain about scanf being unsafe
#pragma warning(disable : 4996)

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

int main()
{
    enum { kSizeMax = 16 };
    VectorSafe<int,kSizeMax> vectorSafe;
    
    const size_t actualSize = 12;
    vectorSafe.size(actualSize);
    if (vectorSafe.SizeCurrentInBytes() != actualSize*sizeof(int))
    {
        ExitOnFail(__LINE__);
    }
    if (vectorSafe.SizeMaxInBytes() != kSizeMax*sizeof(int))
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
    VectorSafe<int, kSizeMax> vectorSafeEmpty;
    vectorSafeEmpty.size(0);
    if (!vectorSafeEmpty.empty())
    {
        ExitOnFail(__LINE__);
    }
    if(vectorSafeEmpty.SizeCurrentInBytes() != 0)
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

    VectorSafe<int, kSizeMax> vectorSafeTwo(vectorSafe.size());
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

    VectorSafe<int, kSizeMax> vectorSafeCopy;
    vectorSafeCopy.Copy(vectorSafe);
    if (vectorSafeCopy != vectorSafe)
    {
        ExitOnFail(__LINE__);
    }

    VectorSafe<int, kSizeMax> vectorSafeInitializerList({0, 1, 2 });
    for (int i = 0; i < 3; ++i)
    {
        if (vectorSafeInitializerList[i] != i)
        {
            ExitOnFail(__LINE__);
        }
    }

    const int kDuplicateTestMaxNum = 7;
    VectorSafe<int, kSizeMax> vectorSafeDuplicateTest({0,4,3,1,1,1,2,5,6,6,kDuplicateTestMaxNum,5,5});
    SortAndRemoveDuplicatesFromVectorSafe(&vectorSafeDuplicateTest);
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

    ConstMethodTesting(vectorSafe, actualSize, lastValidValue);

    printf("Unit test SUCCESS!\n");
    ConsolePauseForUserAcknowledgement();
    return 0;
}


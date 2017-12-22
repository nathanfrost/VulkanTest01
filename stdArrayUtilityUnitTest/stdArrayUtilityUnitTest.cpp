#include<stdio.h>
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
static void ConstMethodTesting(const ArrayFixed<T,kMaxSize>& arrayFixed, const int actualSize, const int lastValidValue)
{
    //const function testing
    for (int i = 0; i < actualSize; ++i)
    {
        if (arrayFixed[i] != i)
        {
            ExitOnFail(__LINE__);
        }
    }
    for (int i = 0; i < actualSize; ++i)
    {
        if (arrayFixed.at(i) != i)
        {
            ExitOnFail(__LINE__);
        }
    }
    if (arrayFixed.front() != 0)
    {
        ExitOnFail(__LINE__);
    }
    if (arrayFixed.back() != lastValidValue)
    {
        ExitOnFail(__LINE__);
    }


    {
        auto arrayFixedEnd = arrayFixed.end();
        int index = 0;
        for (auto it = arrayFixed.begin(); it != arrayFixedEnd; ++it)
        {
            if (*it != index)
            {
                ExitOnFail(__LINE__);
            }
            ++index;
        }
    }
    {
        auto arrayFixedEnd = arrayFixed.cend();
        int index = 0;
        for (auto it = arrayFixed.begin(); it != arrayFixedEnd; ++it)
        {
            if (*it != index)
            {
                ExitOnFail(__LINE__);
            }
            ++index;
        }
    }
    {
        auto arrayFixedREnd = arrayFixed.rend();
        int index = lastValidValue;
        for (auto it = arrayFixed.crbegin(); it != arrayFixedREnd; ++it)
        {
            if (*it != index)
            {
                ExitOnFail(__LINE__);
            }
            --index;
        }
    }
}

int main()
{
    enum { kSizeMax = 16 };
    ArrayFixed<int,kSizeMax> arrayFixed;
    
    const size_t actualSize = 12;
    arrayFixed.size(actualSize);
    if (arrayFixed.SizeCurrentInBytes() != actualSize*sizeof(int))
    {
        ExitOnFail(__LINE__);
    }
    if (arrayFixed.SizeMaxInBytes() != kSizeMax*sizeof(int))
    {
        ExitOnFail(__LINE__);
    }
    for (int i = 0; i < actualSize; ++i)
    {
        arrayFixed[i] = i;
    }
    for (int i = 0; i < actualSize; ++i)
    {
        if (arrayFixed.GetChecked(i) != i)
        {
            ExitOnFail(__LINE__);
        }
    }
    const int lastValidValue = actualSize - 1;
    if (arrayFixed.GetLastValidIndex() != lastValidValue)
    {
        ExitOnFail(__LINE__);
    }
    if (arrayFixed.GetOneAfterLastValidIndex() != actualSize)
    {
        ExitOnFail(__LINE__);
    }
    for (int i = 0; i < actualSize; ++i)
    {
        if (arrayFixed[i] != i)
        {
            ExitOnFail(__LINE__);
        }
    }
    for (int i = 0; i < actualSize; ++i)
    {
        if (arrayFixed.at(i) != i)
        {
            ExitOnFail(__LINE__);
        }
    }
    if (arrayFixed.front() != 0)
    {
        ExitOnFail(__LINE__);
    }
    if (arrayFixed.back() != lastValidValue)
    {
        ExitOnFail(__LINE__);
    }
    {
        auto arrayFixedEnd = arrayFixed.end();
        int index = 0;
        for (auto it = arrayFixed.begin(); it != arrayFixedEnd; ++it)
        {
            if (*it != index)
            {
                ExitOnFail(__LINE__);
            }
            ++index;
        }
    }

    {
        auto arrayFixedREnd = arrayFixed.rend();
        int index = lastValidValue;
        for (auto it = arrayFixed.rbegin(); it != arrayFixedREnd; ++it)
        {
            if (*it != index)
            {
                ExitOnFail(__LINE__);
            }
            --index;
        }
    }

    if (arrayFixed.empty())
    {
        ExitOnFail(__LINE__);
    }
    ArrayFixed<int, kSizeMax> arrayFixedEmpty;
    arrayFixedEmpty.size(0);
    if (!arrayFixedEmpty.empty())
    {
        ExitOnFail(__LINE__);
    }
    if(arrayFixedEmpty.SizeCurrentInBytes() != 0)
    {
        ExitOnFail(__LINE__);
    }

    if (arrayFixedEmpty == arrayFixed)
    {
        ExitOnFail(__LINE__);
    }
    if (!(arrayFixedEmpty != arrayFixed))
    {
        ExitOnFail(__LINE__);
    }

    ArrayFixed<int, kSizeMax> arrayFixedTwo(arrayFixed.size());
    auto arrayFixedEnd = arrayFixedTwo.end();
    for (size_t i = 0; i < arrayFixed.size(); ++i)
    {
        arrayFixedTwo[i] = arrayFixed[i];
    }

    if (arrayFixed != arrayFixedTwo)
    {
        ExitOnFail(__LINE__);
    }
    arrayFixedTwo[3] = 666;
    if (arrayFixed == arrayFixedTwo)
    {
        ExitOnFail(__LINE__);
    }

    const int arrayFixedTwoOldBack = arrayFixedTwo.back();
    arrayFixedTwo.Push(static_cast<int>(arrayFixedTwo.size()));
    if (arrayFixedTwo.back() != static_cast<int>(arrayFixedTwo.size()) - 1 || arrayFixedTwo.size() != arrayFixed.size() + 1)
    {
        ExitOnFail(__LINE__);
    }
    arrayFixedTwo.sizeDecrement();
    if (arrayFixedTwo.size() != arrayFixed.size() || arrayFixedTwo.back() != arrayFixedTwoOldBack)
    {
        ExitOnFail(__LINE__);
    }

    ArrayFixed<int, kSizeMax> arrayFixedCopy;
    arrayFixedCopy.Copy(arrayFixed);
    if (arrayFixedCopy != arrayFixed)
    {
        ExitOnFail(__LINE__);
    }

    ArrayFixed<int, kSizeMax> arrayFixedInitializerList({0, 1, 2 });
    for (int i = 0; i < 3; ++i)
    {
        if (arrayFixedInitializerList[i] != i)
        {
            ExitOnFail(__LINE__);
        }
    }

    ConstMethodTesting(arrayFixed, actualSize, lastValidValue);

    ConsolePauseForUserAcknowledgement();

    const int kDuplicateTestMaxNum = 7;
    ArrayFixed<int, kSizeMax> arrayFixedDuplicateTest({0,4,3,1,1,1,2,5,6,6,kDuplicateTestMaxNum,5,5});
    SortAndRemoveDuplicatesFromArray(&arrayFixedDuplicateTest);
    if (arrayFixedDuplicateTest.size() != kDuplicateTestMaxNum + 1)
    {
        ExitOnFail(__LINE__);
    }
    for (int i = 0; i < kDuplicateTestMaxNum; ++i)
    {
        if (arrayFixedDuplicateTest[i] != i)
        {
            ExitOnFail(__LINE__);
        }
    }

    return 0;
}


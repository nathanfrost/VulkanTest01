#include <stdio.h>
#include <stdlib.h>
#include"../VulkanTest01/stdArrayUtility.h"
#include"../VulkanTest01/QueueCircular.h"

///@todo: allow failed asserts to continue -- eg allow unit tests to succeed BECAUSE they trigger asserts, verifying that the asserts are working as intended

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

int main()
{
    //BEG_#VectorSafe
    enum { kElementsMax = 16 };
    VectorSafe<int,kElementsMax> vectorSafe;
    
    const size_t actualSize = 12;
    vectorSafe.size(actualSize);
    if (vectorSafe.SizeCurrentInBytes() != actualSize*sizeof(int))
    {
        ExitOnFail(__LINE__);
    }
    if (vectorSafe.SizeMaxInBytes() != kElementsMax*sizeof(int))
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
    vectorSafeCopy.Copy(vectorSafe);
    if (vectorSafeCopy != vectorSafe)
    {
        ExitOnFail(__LINE__);
    }

    VectorSafe<int, kElementsMax> vectorSafeInitializerList({0, 1, 2 });
    for (int i = 0; i < 3; ++i)
    {
        if (vectorSafeInitializerList[i] != i)
        {
            ExitOnFail(__LINE__);
        }
    }

    const int kDuplicateTestMaxNum = 7;
    VectorSafe<int, kElementsMax> vectorSafeDuplicateTest({0,4,3,1,1,1,2,5,6,6,kDuplicateTestMaxNum,5,5});
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

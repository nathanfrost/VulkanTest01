#include"StreamingUnitTest.h"
#include"StreamingUnitManager.h"

static void UnitTest_StreamingUnitsLoadIfNotLoaded(
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToLoad,
    bool*const issuedLoadCommandPtr,
    bool*const advanceToNextUnitTestPtr,
    AssetLoadingArgumentsThreadCommand*const threadCommandPtr,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
    RTL_CRITICAL_SECTION*const streamingUnitsAddToLoadCriticalSectionPtr,
    const HANDLE assetLoadingThreadWakeHandle)
{
    NTF_REF(issuedLoadCommandPtr, issuedLoadCommand);
    NTF_REF(advanceToNextUnitTestPtr, advanceToNextUnitTest);
    NTF_REF(threadCommandPtr, threadCommand);
    NTF_REF(streamingUnitsAddToLoadCriticalSectionPtr, streamingUnitsAddToLoadCriticalSection);

    for (auto& streamingUnitToLoadPtr : streamingUnitsToLoad)
    {
        NTF_REF(streamingUnitToLoadPtr, streamingUnitToLoad);
        if (streamingUnitToLoad.StateCriticalSection() == StreamingUnitRuntime::State::kUnloaded)
        {
            StreamingUnitAddToLoadCriticalSection(&streamingUnitToLoad, &streamingUnitsAddToLoad, &streamingUnitsAddToLoadCriticalSection);
            AssetLoadingThreadExecuteLoad(&threadCommand, assetLoadingThreadWakeHandle);
            issuedLoadCommand = true;
            advanceToNextUnitTest = false;
        }
    }
}
static void UnitTest_StreamingUnitLoadIfNotLoaded(
    StreamingUnitRuntime*const streamingUnit,
    bool*const issuedLoadCommandPtr,
    bool*const advanceToNextUnitTestPtr,
    AssetLoadingArgumentsThreadCommand*const threadCommandPtr,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
    RTL_CRITICAL_SECTION*const streamingUnitsAddToLoadCriticalSectionPtr,
    const HANDLE assetLoadingThreadWakeHandle)
{
    VectorSafe<StreamingUnitRuntime*, 1> temp;
    temp.Push(streamingUnit);
    UnitTest_StreamingUnitsLoadIfNotLoaded(
        &temp,
        issuedLoadCommandPtr,
        advanceToNextUnitTestPtr,
        threadCommandPtr,
        streamingUnitsAddToLoad,
        streamingUnitsAddToLoadCriticalSectionPtr,
        assetLoadingThreadWakeHandle);
}

static void UnitTest_StreamingUnitsUnloadIfNotUnloaded(
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToUnload,
    bool*const advanceToNextUnitTestPtr,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToUnload,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsRenderable)
{
    NTF_REF(advanceToNextUnitTestPtr, advanceToNextUnitTest);

    for (auto& streamingUnitToUnloadPtr : streamingUnitsAddToUnload)
    {
        NTF_REF(streamingUnitToUnloadPtr, streamingUnitToUnload);

        const bool streamingUnitIsUnloaded = streamingUnitToUnload.StateCriticalSection() == StreamingUnitRuntime::State::kUnloaded;
        const bool streamingUnitIsOrWillBeUnloaded = streamingUnitIsUnloaded || streamingUnitsToUnload.Find(&streamingUnitToUnload) >= 0;
        if (!streamingUnitIsOrWillBeUnloaded)
        {
            StreamingUnitAddToUnload(&streamingUnitToUnload, &streamingUnitsRenderable, &streamingUnitsToUnload);
        }
        if (!streamingUnitIsUnloaded)
        {
            advanceToNextUnitTest = false;
        }
    }
}
static void UnitTest_StreamingUnitUnloadIfNotUnloaded(
    StreamingUnitRuntime*const streamingUnitPtr,
    bool*const advanceToNextUnitTestPtr,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToUnload,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsRenderable)
{
    VectorSafe<StreamingUnitRuntime*, 1> temp;
    temp.Push(streamingUnitPtr);
    UnitTest_StreamingUnitsUnloadIfNotUnloaded(
        &temp,
        advanceToNextUnitTestPtr,
        streamingUnitsToUnload,
        streamingUnitsRenderable);
}

static void StreamingUnitTestAdvanceState(
    StreamingUnitRuntime*const streamingUnit0Ptr,
    StreamingUnitRuntime*const streamingUnit1Ptr,
    StreamingUnitRuntime*const streamingUnit2Ptr,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsRenderable,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToUnload,
    AssetLoadingArgumentsThreadCommand*const threadCommandPtr,
    bool*const issuedLoadCommandPtr,
    RTL_CRITICAL_SECTION*const streamingUnitsAddToLoadCriticalSectionPtr,
    const bool*const assetLoadingThreadIdlePtr,
    const HANDLE assetLoadingThreadWakeHandle)
{
    static enum class UnitTestState :size_t
    {
        k0_LoadIndexZero = 0,
        k1_LoadIndexOne,
        k2_LoadIndexTwo,
        k3_UnloadIndexZero,
        k4_UnloadIndexTwo,
        k5_UnloadIndexOne_And_LoadIndexTwo,
        k6_UnloadIndexTwo,
        k7_LoadIndexZero_And_LoadIndexOne,
        k8_UnloadIndexZero_And_UnloadIndexOne,
        k9_DoNothing,
        k10_LoadIndexZeroTwice,
        k11_UnloadIndexZeroTwice,
        k12_LoadThenUnloadIndexZero,
        k13_UnloadThenLoadIndexZero,
        k14_ManyCommandsIssuedForIndexZero,
        k15_UnloadIndexZeroAndIndexOne,
        k16_LoadIndexZeroThenImmediatelyLoadIndexOne,
        k17_UnloadIndexZeroAndOne,
        k18_LoadIndexZeroThenImmediatelyLoadIndexZero,
        k19_UnloadIndexZero,
        kNum
    } s_state;
    NTF_REF(streamingUnit0Ptr, streamingUnit0);
    NTF_REF(streamingUnit1Ptr, streamingUnit1);
    NTF_REF(streamingUnit2Ptr, streamingUnit2);
    NTF_REF(issuedLoadCommandPtr, issuedLoadCommand);
    NTF_REF(streamingUnitsAddToLoadCriticalSectionPtr, streamingUnitsAddToLoadCriticalSection);
    NTF_REF(assetLoadingThreadIdlePtr, assetLoadingThreadIdle);
    NTF_REF(threadCommandPtr, threadCommand);

    issuedLoadCommand = false;
    bool advanceToNextUnitTest = true;
    VectorSafe<StreamingCommand, 2> streamingCommands;
    switch (s_state)
    {
        case UnitTestState::k0_LoadIndexZero:
        {
            UnitTest_StreamingUnitLoadIfNotLoaded(
                &streamingUnit0,
                &issuedLoadCommand,
                &advanceToNextUnitTest,
                &threadCommand,
                streamingUnitsAddToLoad,
                &streamingUnitsAddToLoadCriticalSection,
                assetLoadingThreadWakeHandle);
            break;
        }
        case UnitTestState::k1_LoadIndexOne:
        {
            UnitTest_StreamingUnitLoadIfNotLoaded(
                &streamingUnit1,
                &issuedLoadCommand,
                &advanceToNextUnitTest,
                &threadCommand,
                streamingUnitsAddToLoad,
                &streamingUnitsAddToLoadCriticalSection,
                assetLoadingThreadWakeHandle);
            break;
        }
        case UnitTestState::k2_LoadIndexTwo:
        {
            UnitTest_StreamingUnitLoadIfNotLoaded(
                &streamingUnit2,
                &issuedLoadCommand,
                &advanceToNextUnitTest,
                &threadCommand,
                streamingUnitsAddToLoad,
                &streamingUnitsAddToLoadCriticalSection,
                assetLoadingThreadWakeHandle);
            break;
        }
        case UnitTestState::k3_UnloadIndexZero:
        {
            UnitTest_StreamingUnitUnloadIfNotUnloaded(
                &streamingUnit0,
                &advanceToNextUnitTest,
                streamingUnitsToUnload,
                streamingUnitsRenderable);
            break;
        }
        case UnitTestState::k4_UnloadIndexTwo:
        {
            UnitTest_StreamingUnitUnloadIfNotUnloaded(
                &streamingUnit2,
                &advanceToNextUnitTest,
                streamingUnitsToUnload,
                streamingUnitsRenderable);
            break;
        }
        case UnitTestState::k5_UnloadIndexOne_And_LoadIndexTwo:
        {
            UnitTest_StreamingUnitUnloadIfNotUnloaded(
                &streamingUnit1,
                &advanceToNextUnitTest,
                streamingUnitsToUnload,
                streamingUnitsRenderable);

            UnitTest_StreamingUnitLoadIfNotLoaded(
                &streamingUnit2,
                &issuedLoadCommand,
                &advanceToNextUnitTest,
                &threadCommand,
                streamingUnitsAddToLoad,
                &streamingUnitsAddToLoadCriticalSection,
                assetLoadingThreadWakeHandle);
            break;
        }
        case UnitTestState::k6_UnloadIndexTwo:
        {
            UnitTest_StreamingUnitUnloadIfNotUnloaded(
                &streamingUnit2,
                &advanceToNextUnitTest,
                streamingUnitsToUnload,
                streamingUnitsRenderable);
            break;
        }
        case UnitTestState::k7_LoadIndexZero_And_LoadIndexOne:
        {
            VectorSafe<StreamingUnitRuntime*, 2> streamingUnitsToLoad;
            streamingUnitsToLoad.Push(&streamingUnit0);
            streamingUnitsToLoad.Push(&streamingUnit1);

            UnitTest_StreamingUnitsLoadIfNotLoaded(
                &streamingUnitsToLoad,
                &issuedLoadCommand,
                &advanceToNextUnitTest,
                &threadCommand,
                streamingUnitsAddToLoad,
                &streamingUnitsAddToLoadCriticalSection,
                assetLoadingThreadWakeHandle);
            break;
        }
        case UnitTestState::k8_UnloadIndexZero_And_UnloadIndexOne:
        {
            VectorSafe<StreamingUnitRuntime*, 2> streamingUnitsToAddToUnload;
            streamingUnitsToAddToUnload.Push(&streamingUnit0);
            streamingUnitsToAddToUnload.Push(&streamingUnit1);

            UnitTest_StreamingUnitsUnloadIfNotUnloaded(
                &streamingUnitsToAddToUnload,
                &advanceToNextUnitTest,
                &streamingUnitsToUnload,
                streamingUnitsRenderable);
            break;
        }
        case UnitTestState::k9_DoNothing:
        {
            break;
        }
        case UnitTestState::k10_LoadIndexZeroTwice:
        {
            VectorSafe<StreamingUnitRuntime*, 2> streamingUnitsToLoad;
            streamingUnitsToLoad.Push(&streamingUnit0);
            streamingUnitsToLoad.Push(&streamingUnit0);

            UnitTest_StreamingUnitsLoadIfNotLoaded(
                &streamingUnitsToLoad,
                &issuedLoadCommand,
                &advanceToNextUnitTest,
                &threadCommand,
                streamingUnitsAddToLoad,
                &streamingUnitsAddToLoadCriticalSection,
                assetLoadingThreadWakeHandle);
            break;
        }
        case UnitTestState::k11_UnloadIndexZeroTwice:
        {
            VectorSafe<StreamingUnitRuntime*, 2> streamingUnitsToAddToUnload;
            streamingUnitsToAddToUnload.Push(&streamingUnit0);
            streamingUnitsToAddToUnload.Push(&streamingUnit0);

            UnitTest_StreamingUnitsUnloadIfNotUnloaded(
                &streamingUnitsToAddToUnload,
                &advanceToNextUnitTest,
                &streamingUnitsToUnload,
                streamingUnitsRenderable);
            break;
        }
        case UnitTestState::k12_LoadThenUnloadIndexZero:
        {
            StreamingUnitAddToLoadCriticalSection(&streamingUnit0, &streamingUnitsAddToLoad, &streamingUnitsAddToLoadCriticalSection);
            AssetLoadingThreadExecuteLoad(&threadCommand, assetLoadingThreadWakeHandle);

            UnitTest_StreamingUnitUnloadIfNotUnloaded(
                &streamingUnit0,
                &advanceToNextUnitTest,
                streamingUnitsToUnload,
                streamingUnitsRenderable);

            AssetLoadingThreadExecuteLoad(&threadCommand, assetLoadingThreadWakeHandle);
            advanceToNextUnitTest = true;
            break;
        }
        case UnitTestState::k13_UnloadThenLoadIndexZero:
        {
            StreamingUnitAddToUnload(&streamingUnit0, streamingUnitsRenderable, &streamingUnitsToUnload);

            StreamingUnitAddToLoadCriticalSection(&streamingUnit2, &streamingUnitsAddToLoad, &streamingUnitsAddToLoadCriticalSection);
            AssetLoadingThreadExecuteLoad(&threadCommand, assetLoadingThreadWakeHandle);
            advanceToNextUnitTest = true;
            break;
        }
        case UnitTestState::k14_ManyCommandsIssuedForIndexZero:
        {
            VectorSafe<StreamingUnitRuntime*, 2> streamingUnitsToLoad;
            streamingUnitsToLoad.Push(&streamingUnit0);
            streamingUnitsToLoad.Push(&streamingUnit0);

            StreamingUnitsAddToLoadCriticalSection(&streamingUnitsToLoad, streamingUnitsAddToLoad, &streamingUnitsAddToLoadCriticalSection);

            VectorSafe<StreamingUnitRuntime*, 2> streamingUnitsToUnload;
            streamingUnitsToUnload.Push(&streamingUnit0);
            streamingUnitsToUnload.Push(&streamingUnit0);
            StreamingUnitsAddToUnload(&streamingUnitsToUnload, streamingUnitsRenderable, &streamingUnitsToUnload);

            StreamingUnitAddToUnload(&streamingUnit0, streamingUnitsRenderable, &streamingUnitsToUnload);
            StreamingUnitAddToLoadCriticalSection(&streamingUnit0, &streamingUnitsAddToLoad, &streamingUnitsAddToLoadCriticalSection);

            StreamingUnitAddToLoadCriticalSection(&streamingUnit0, &streamingUnitsAddToLoad, &streamingUnitsAddToLoadCriticalSection);
            StreamingUnitAddToUnload(&streamingUnit0, streamingUnitsRenderable, &streamingUnitsToUnload);

            AssetLoadingThreadExecuteLoad(&threadCommand, assetLoadingThreadWakeHandle);
            advanceToNextUnitTest = true;
            break;
        }
        case UnitTestState::k15_UnloadIndexZeroAndIndexOne:
        {
            VectorSafe<StreamingUnitRuntime*, 2> streamingUnitsToAddToUnload;
            streamingUnitsToAddToUnload.Push(&streamingUnit0);
            streamingUnitsToAddToUnload.Push(&streamingUnit1);

            UnitTest_StreamingUnitsUnloadIfNotUnloaded(
                &streamingUnitsToAddToUnload,
                &advanceToNextUnitTest,
                &streamingUnitsToUnload,
                streamingUnitsRenderable);
            break;
        }
        case UnitTestState::k16_LoadIndexZeroThenImmediatelyLoadIndexOne:
        {
            UnitTest_StreamingUnitLoadIfNotLoaded(
                &streamingUnit0,
                &issuedLoadCommand,
                &advanceToNextUnitTest,
                &threadCommand,
                streamingUnitsAddToLoad,
                &streamingUnitsAddToLoadCriticalSection,
                assetLoadingThreadWakeHandle);

            Sleep(1);//ensure asset loading thread is already handling the first load while receiving another

            UnitTest_StreamingUnitLoadIfNotLoaded(
                &streamingUnit1,
                &issuedLoadCommand,
                &advanceToNextUnitTest,
                &threadCommand,
                streamingUnitsAddToLoad,
                &streamingUnitsAddToLoadCriticalSection,
                assetLoadingThreadWakeHandle);
            break;
        }
        case UnitTestState::k17_UnloadIndexZeroAndOne:
        {
            VectorSafe<StreamingUnitRuntime*, 2> streamingUnitsToAddToUnload;
            streamingUnitsToAddToUnload.Push(&streamingUnit0);
            streamingUnitsToAddToUnload.Push(&streamingUnit1);
            UnitTest_StreamingUnitsUnloadIfNotUnloaded(
                &streamingUnitsToAddToUnload,
                &advanceToNextUnitTest,
                &streamingUnitsToUnload,
                streamingUnitsRenderable);
            break;
        }
        case UnitTestState::k18_LoadIndexZeroThenImmediatelyLoadIndexZero:
        {
            assert(streamingUnit0.m_state == StreamingUnitRuntime::State::kUnloaded);
            if (assetLoadingThreadIdle)
            {
                for (int i = 0; i < 2; ++i)
                {
                    StreamingUnitAddToLoadCriticalSection(&streamingUnit0, &streamingUnitsAddToLoad, &streamingUnitsAddToLoadCriticalSection);
                    AssetLoadingThreadExecuteLoad(&threadCommand, assetLoadingThreadWakeHandle);
                    Sleep(1);//ensure asset loading thread is already handling the first load while receiving another
                }
                advanceToNextUnitTest = true;
            }
            break;
        }
        case UnitTestState::k19_UnloadIndexZero:
        {
            UnitTest_StreamingUnitUnloadIfNotUnloaded(
                &streamingUnit0,
                &advanceToNextUnitTest,
                streamingUnitsToUnload,
                streamingUnitsRenderable);
            break;
        }
        default:
        {
            assert(false);
            break;
        }
    }

    if (advanceToNextUnitTest)
    {
#if NTF_UNIT_TEST_STREAMING_LOG
        FwriteSnprintf(s_streamingDebug, &s_streamingDebugCriticalSection, "s_state EXECUTED=%i\n", (int)s_state);
#endif//#if NTF_UNIT_TEST_STREAMING_LOG

        s_state = static_cast<UnitTestState>(static_cast<size_t>(s_state) + 1);
        if (s_state >= UnitTestState::kNum)
        {
            s_state = UnitTestState::k0_LoadIndexZero;
        }

#if NTF_UNIT_TEST_STREAMING_LOG
        FwriteSnprintf(s_streamingDebug, &s_streamingDebugCriticalSection, "s_state NEXT=%i\n", (int)s_state);
#endif//#if NTF_UNIT_TEST_STREAMING_LOG
    }
#if NTF_UNIT_TEST_STREAMING_LOG
    else
    {
        FwriteSnprintf(s_streamingDebug, &s_streamingDebugCriticalSection, "s_state=%i -- did not advance to next unit test\n", (int)s_state);
    }
#endif//#if NTF_UNIT_TEST_STREAMING_LOG

}

void StreamingUnitTestTick(
    StreamingUnitRuntime*const streamingUnit0Ptr,
    StreamingUnitRuntime*const streamingUnit1Ptr,
    StreamingUnitRuntime*const streamingUnit2Ptr,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsRenderable,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToUnload,
    AssetLoadingArgumentsThreadCommand*const threadCommandPtr,
    RTL_CRITICAL_SECTION*const streamingUnitsAddToLoadCriticalSectionPtr,
    const bool*const assetLoadingThreadIdlePtr,
    const HANDLE assetLoadingThreadWakeHandle,
    const StreamingUnitRuntime::FrameNumber frameNumberCurrentCpu,
    const StreamingUnitRuntime::FrameNumber frameToSwapState)
{
    NTF_REF(streamingUnit0Ptr, streamingUnit0);
    NTF_REF(streamingUnit1Ptr, streamingUnit1);
    NTF_REF(streamingUnit2Ptr, streamingUnit2);
    NTF_REF(threadCommandPtr, threadCommand);
    NTF_REF(streamingUnitsAddToLoadCriticalSectionPtr, streamingUnitsAddToLoadCriticalSection);
    NTF_REF(assetLoadingThreadIdlePtr, assetLoadingThreadIdle);

    assert(frameToSwapState > 1);
    static bool s_lastUnitTestExecuteIssuedLoadWithNoRenderableStreamingUnits;

    bool executeUnitTest = false;
    if (frameNumberCurrentCpu % frameToSwapState == frameToSwapState - 1)
    {
        if (streamingUnitsRenderable.size())
        {
            executeUnitTest = true;
            s_lastUnitTestExecuteIssuedLoadWithNoRenderableStreamingUnits = false;
        }
        else if (!s_lastUnitTestExecuteIssuedLoadWithNoRenderableStreamingUnits)
        {
            executeUnitTest = true;
        }

#if NTF_UNIT_TEST_STREAMING_LOG
        FwriteSnprintf(s_streamingDebug,
            &s_streamingDebugCriticalSection,
            "%s:%i:streamingUnitsRenderable.size()=%zu, s_lastUnitTestExecuteIssuedLoadWithNoRenderableStreamingUnits=%i -> executeUnitTest=%i\n",
            __FILE__, __LINE__, streamingUnitsRenderable.size(), s_lastUnitTestExecuteIssuedLoadWithNoRenderableStreamingUnits, executeUnitTest);
#endif//#if NTF_UNIT_TEST_STREAMING_LOG
    }
    if (executeUnitTest)
    {
        bool executedLoadCommand;
        StreamingUnitTestAdvanceState(
            &streamingUnit0,
            &streamingUnit1,
            &streamingUnit2,
            &streamingUnitsAddToLoad,
            &streamingUnitsRenderable,
            &streamingUnitsToUnload,
            &threadCommand,
            &executedLoadCommand,
            &streamingUnitsAddToLoadCriticalSection,
            &assetLoadingThreadIdle,
            assetLoadingThreadWakeHandle);

#if NTF_UNIT_TEST_STREAMING_LOG
        FwriteSnprintf(s_streamingDebug,
            &s_streamingDebugCriticalSection,
            "%s:%i:UnitTest() done: streamingUnitsRenderable.size()=%zu, s_lastUnitTestExecuteIssuedLoadWithNoRenderableStreamingUnits=%i, executedLoadCommand=%i\n",
            __FILE__, __LINE__, streamingUnitsRenderable.size(), s_lastUnitTestExecuteIssuedLoadWithNoRenderableStreamingUnits, executedLoadCommand);
#endif//#if NTF_UNIT_TEST_STREAMING_LOG
        if (!streamingUnitsRenderable.size() && !s_lastUnitTestExecuteIssuedLoadWithNoRenderableStreamingUnits && executedLoadCommand)
        {
            s_lastUnitTestExecuteIssuedLoadWithNoRenderableStreamingUnits = true;
        }
    }
}
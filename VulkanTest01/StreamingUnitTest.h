#pragma once

#include"stdArrayUtility.h"
#include"StreamingUnit.h"

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
    const StreamingUnitRuntime::FrameNumber frameToSwapState);

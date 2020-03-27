#include"WindowsUtil.h"

#include<assert.h>

HANDLE MutexCreate()
{
    const HANDLE mutexHandle = CreateMutex(
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex
    assert(mutexHandle);
    return mutexHandle;
}
void MutexRelease(const HANDLE mutex)
{
    assert(mutex);
    const BOOL releaseMutexResult = ReleaseMutex(mutex);
    assert(releaseMutexResult == TRUE);
}
void CriticalSectionCreate(RTL_CRITICAL_SECTION*const criticalSectionPtr)
{
    assert(criticalSectionPtr);
    InitializeCriticalSectionAndSpinCount(criticalSectionPtr, 0x400);
}
void CriticalSectionEnter(RTL_CRITICAL_SECTION*const criticalSectionPtr)
{
    assert(criticalSectionPtr);
    EnterCriticalSection(criticalSectionPtr);
}
void CriticalSectionLeave(RTL_CRITICAL_SECTION*const criticalSectionPtr)
{
    assert(criticalSectionPtr);
    LeaveCriticalSection(criticalSectionPtr);
}
void CriticalSectionDelete(RTL_CRITICAL_SECTION*const criticalSectionPtr)
{
    assert(criticalSectionPtr);
    DeleteCriticalSection(criticalSectionPtr);
}

HANDLE ThreadSignalingEventCreate()
{
    const HANDLE ret = CreateEvent(
        NULL,               // default security attributes
        FALSE,              // auto-reset; after signaling immediately set to nonsignaled
        FALSE,              // initial state is nonsignaled
        NULL                // no name -- if you have two events with the same name, the more recent one stomps the less recent one
    );
    assert(ret);
    return ret;
}

BOOL HandleCloseWindows(HANDLE*const h)
{
    assert(h);
    const BOOL closeHandleResult = CloseHandle(*h);
    assert(closeHandleResult);
    *h = NULL;
    return closeHandleResult;
}

void UnsignalSemaphoreWindows(const HANDLE semaphoreHandle)
{
    const BOOL resetEventResult = ResetEvent(semaphoreHandle);
    assert(resetEventResult);
}
void SignalSemaphoreWindows(const HANDLE semaphoreHandle)
{
    const BOOL setEventResult = SetEvent(semaphoreHandle);
    assert(setEventResult);
}
//use for both semaphores and mutexes
void WaitForSignalWindows(const HANDLE semaphoreOrMutexHandle)
{
    const DWORD result = WaitForSingleObject(semaphoreOrMutexHandle, INFINITE);
    assert(result == WAIT_OBJECT_0);
}

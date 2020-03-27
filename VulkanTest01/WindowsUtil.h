#pragma once

#include<windows.h>

HANDLE MutexCreate();
void MutexRelease(const HANDLE mutex);
HANDLE ThreadSignalingEventCreate();
BOOL HandleCloseWindows(HANDLE*const h);
void UnsignalSemaphoreWindows(const HANDLE semaphoreHandle);
void SignalSemaphoreWindows(const HANDLE semaphoreHandle);
void WaitForSignalWindows(const HANDLE semaphoreOrMutexHandle);
void CriticalSectionCreate(RTL_CRITICAL_SECTION*const criticalSectionPtr);
void CriticalSectionEnter(RTL_CRITICAL_SECTION*const criticalSectionPtr);
void CriticalSectionLeave(RTL_CRITICAL_SECTION*const criticalSectionPtr);
void CriticalSectionDelete(RTL_CRITICAL_SECTION*const criticalSectionPtr);

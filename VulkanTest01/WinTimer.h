#pragma once
#include<windows.h>
#include<assert.h>

#if NTF_WIN_TIMER
class WinTimerNTF
{
public:
    WinTimerNTF()
    {
        const BOOL frequencyResult = ::QueryPerformanceFrequency(&m_frequency);
        assert(frequencyResult == TRUE);
    }

    void Start()
    {
        const BOOL startResult = ::QueryPerformanceCounter(&m_start);
        assert(startResult == TRUE);
    }

    void Stop()
    {
        const BOOL endResult = ::QueryPerformanceCounter(&m_end);
        assert(endResult == TRUE);
    }

    double ElapsedSeconds()
    {
        return static_cast<double>(m_end.QuadPart - m_start.QuadPart) / m_frequency.QuadPart;
    }

    double ElapsedMilliseconds()
    {
        return ElapsedSeconds()*1000.f;
    }

private:
    LARGE_INTEGER m_frequency, m_start, m_end;
};
#endif//#if NTF_WIN_TIMER

#if NTF_WIN_TIMER
#define WIN_TIMER_DEF(name) WinTimerNTF name
#define WIN_TIMER_DEF_START(name) WinTimerNTF name;name.Start()
#define WIN_TIMER_START(name) name.Start()
#define WIN_TIMER_STOP(name) name.Stop()
#define WIN_TIMER_ELAPSED_SECONDS(name) name.ElapsedSeconds()
#define WIN_TIMER_ELAPSED_MILLISECONDS(name) name.ElapsedMilliseconds()
#else
#define WIN_TIMER_DEF(name)
#define WIN_TIMER_DEF_START(name)
#define WIN_TIMER_START(name)
#define WIN_TIMER_STOP(name)
#define WIN_TIMER_ELAPSED(name)
#endif
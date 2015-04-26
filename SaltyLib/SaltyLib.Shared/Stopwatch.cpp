#include "pch.h"
#include "Stopwatch.h"

Duration Duration::FromMicroseconds(LONGLONG value)
{
	Duration duration;
	duration.m_durationInMicroseconds = value;
	return duration;
}

LONGLONG Duration::GetMilliseconds() const
{
	return m_durationInMicroseconds / 1000;
}

Stopwatch::Stopwatch(bool startTiming /*= false*/)
{
	if (startTiming)
		Start();
}



void Stopwatch::Start()
{
	QueryPerformanceCounter(&m_startCounterValue);
}

Duration Stopwatch::Stop()
{
	LARGE_INTEGER stopCounter, elapsedMicroseconds;

	QueryPerformanceCounter(&stopCounter);
	LARGE_INTEGER timerFrequency;
	QueryPerformanceFrequency(&timerFrequency);

	elapsedMicroseconds.QuadPart = stopCounter.QuadPart - m_startCounterValue.QuadPart;
	elapsedMicroseconds.QuadPart *= 1000000;
	elapsedMicroseconds.QuadPart /= timerFrequency.QuadPart;

	return Duration::FromMicroseconds(elapsedMicroseconds.QuadPart);
}

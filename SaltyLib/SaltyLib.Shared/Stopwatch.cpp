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
	QueryPerformanceFrequency(&m_timerFrequency);
	QueryPerformanceCounter(&m_startCounterValue);
}

Duration Stopwatch::DurationFromCounters(LONGLONG startCounter, LONGLONG endCounter) const
{
	LONGLONG elapsedMicroseconds = endCounter - startCounter;
	elapsedMicroseconds *= 1000000;
	elapsedMicroseconds /= m_timerFrequency.QuadPart;

	return Duration::FromMicroseconds(elapsedMicroseconds);
}


Duration Stopwatch::Stop() const
{
	LARGE_INTEGER stopCounter;
	QueryPerformanceCounter(&stopCounter);

	return DurationFromCounters(m_startCounterValue.QuadPart, stopCounter.QuadPart);
}

Duration Stopwatch::Lap()
{
	LARGE_INTEGER stopCounter;
	QueryPerformanceCounter(&stopCounter);

	Duration result = DurationFromCounters(m_startCounterValue.QuadPart, stopCounter.QuadPart);
	m_startCounterValue = stopCounter;

	return result;
}


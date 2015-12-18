#pragma once


#include <stdint.h>
#include <windows.h>
 
class Duration
{
public:
	static Duration FromMicroseconds(int64_t value);

	int64_t GetMilliseconds() const;

private:
	int64_t m_durationInMicroseconds;
};

class Stopwatch
{
public:
	Stopwatch(bool startTiming = false);


	void Start();
	Duration Stop() const;
	Duration Lap();

private:
	Duration DurationFromCounters(int64_t startCounter, int64_t endCounter) const;

	LARGE_INTEGER m_timerFrequency;
	LARGE_INTEGER m_startCounterValue;
};


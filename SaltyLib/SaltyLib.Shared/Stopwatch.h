#pragma once


class Duration
{
public:
	static Duration FromMicroseconds(LONGLONG value);

	LONGLONG GetMilliseconds() const;

private:
	LONGLONG m_durationInMicroseconds;
};

class Stopwatch
{
public:
	Stopwatch(bool startTiming = false);


	void Start();
	Duration Stop() const;
	Duration Lap();

private:
	Duration DurationFromCounters(LONGLONG startCounter, LONGLONG endCounter) const;

	LARGE_INTEGER m_timerFrequency;
	LARGE_INTEGER m_startCounterValue;
};


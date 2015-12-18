#pragma once
#include <array>
template <typename T, size_t NumberOfElements>
class MovingAverage
{
public:
	T AddValue(T val)
	{
		m_sum -= m_values[m_index];
		m_sum += val;
		m_values[m_index] = val;
		if (++m_index == NumberOfElements)
			m_index = 0;

		return m_sum / NumberOfElements;
	}

private:
	T m_values[NumberOfElements];
	T m_sum = T();
	size_t m_index = 0;
};


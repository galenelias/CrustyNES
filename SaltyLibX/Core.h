#pragma once

#include <exception>
#include <intsafe.h>

class HrException : public std::exception
{
public:
	HrException(HRESULT hr)
		: m_hr(hr)
	{ }

private:
	HRESULT m_hr;
};

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		// Set a breakpoint on this line to catch Win32 API errors.
		throw HrException(hr);
	}
}

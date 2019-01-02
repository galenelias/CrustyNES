#pragma once

#include <stdint.h>
#include <memory>

class IAudioSource
{
public:
	virtual uint32_t GetBytesPerSample() const = 0;
	virtual uint32_t GetSamplesPerSecond() const = 0;

	virtual void SetVolume(float volume) = 0;
	virtual void SetChannelData(const uint8_t* pData, size_t cbData, bool shouldLoop) = 0;
	virtual void Play() = 0;
	virtual void Stop() = 0;
};


class IAudioDevice
{
public:
	virtual std::shared_ptr<IAudioSource> AddAudioSource() = 0;
};


// Concrete factories for various implementations of an IAudioDevice
std::shared_ptr<IAudioDevice> CreateXAudioDevice();

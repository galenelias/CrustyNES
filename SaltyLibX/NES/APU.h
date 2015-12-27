#pragma once

#include <stdint.h>
#include <memory>

namespace NES { namespace APU {


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

struct PulseWaveParameters
{
	union
	{
		uint8_t ByteValue;
		struct
		{
			uint8_t Volume:4;
			uint8_t ConstantVolumeFlag:1;
			uint8_t APULengthCounter:1;
			uint8_t DutyCycle:2;
		};
	} Byte1;

	uint8_t Byte2;

	union
	{
		struct
		{
			uint8_t Byte3Value;
			uint8_t Byte4Value;
		};
		struct
		{
			uint16_t RawPeriod:11;
			uint16_t LengthCounterLoad:5;
		};

	} Bytes3and4;
};

struct TriangleWaveParameters
{
	union
	{
		uint8_t ByteValue;
		struct
		{
			uint8_t LinearCounterLoad:7;
			uint8_t LinearCounterHalt:1;
		};
	} Byte1;

	uint8_t Byte2;

	union
	{
		struct
		{
			uint8_t Byte3Value;
			uint8_t Byte4Value;
		};
		struct
		{
			uint16_t RawPeriod:11;
			uint16_t LengthCounterLoad:5;
		};

	} Bytes3and4;
};

const int c_cpuFrequency = 1789773; // Hz (cycles/second), i.e. 1.789773 MHz


class Apu
{
public:
	Apu();

	void AddCycles(uint32_t cpuCycles);
	void WriteMemory8(uint16_t offset, uint8_t value);
	void PushAudio();

	void EnableSound(bool isEnabled);

private:
	void SetPulseWaveParameters(uint16_t offset, uint8_t value, _Inout_ PulseWaveParameters *pPulseWaveParameters);
	void SetTriangleWaveParameters(uint16_t offset, uint8_t value, _Inout_ TriangleWaveParameters *pPulseWaveParameters);

	void GeneratePulseWaveAudioSourceData(const PulseWaveParameters& params, size_t *pCbAudioData, std::unique_ptr<uint8_t[]> *pAudioDataSmartPtr, IAudioSource* pAudioSource);
	void GenerateTriangleWaveAudioSourceData(const TriangleWaveParameters& params, size_t *pCbAudioData, std::unique_ptr<uint8_t[]> *pAudioDataSmartPtr, IAudioSource* pAudioSource);

	PulseWaveParameters m_pulseWave1Parameters;
	PulseWaveParameters m_pulseWave2Parameters;
	TriangleWaveParameters m_triangleWaveParameters;
	
	std::shared_ptr<IAudioDevice> m_spAudioDevice;

	size_t m_cbPulse1AudioData = 0;
	size_t m_cbPulse2AudioData = 0;
	size_t m_cbTriangleAudioData = 0;

	bool m_isSoundEnabled = true;
	uint32_t m_accumulatedCpuCycles = 0;

	std::unique_ptr<uint8_t[]> m_spAudioData;
	uint32_t m_cbAudioData = 0;
	uint32_t m_audioWriteOffset = 0;
	//uint32_t m_queuedBuffers = 0;

	std::unique_ptr<uint8_t[]> m_spPulse1AudioData;
	std::unique_ptr<uint8_t[]> m_spPulse2AudioData;
	std::unique_ptr<uint8_t[]> m_spTriangleAudioData;

	std::shared_ptr<IAudioSource> m_spPulse1AudioSource;
	std::shared_ptr<IAudioSource> m_spPulse2AudioSource;
	std::shared_ptr<IAudioSource> m_spTriangleAudioSource;
};


} } // NES::APU

#include "stdafx.h"

#include "APU.h"

#include <stdexcept>

#include <algorithm>

#include "../Util/ComPtr.h"
#include "../Util/IAudioDevice.h"
#include "Cpu6502.h"

namespace NES { namespace APU {

const uint32_t c_cpuCyclesPerSecond = 1789773;

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

	union
	{
		uint8_t ByteValue;
		struct
		{
			uint8_t SweepShiftCount:3;
			uint8_t SweepNegateFlag:1;
			uint8_t SweepDividerPeriod:3;
			uint8_t SweepEnabledFlag:1;
		};
	} Byte2;

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

class PulseGenerator
{
public:

	uint16_t GetValue()
	{
		return m_duty[m_sequencerOffset];
	}

	void AddTicks(uint32_t apuTicks)
	{
		if (m_timer > apuTicks)
		{
			m_timer -= apuTicks;
		}
		else
		{
			m_timer = m_timerPeriod - (apuTicks - m_timer);
			ClockWaveform();
		}

		// Process sweep.
		if (m_sweepDividerCounter > apuTicks)
		{
			m_sweepDividerCounter -= apuTicks;
		}
		else
		{
			m_sweepDividerCounter = m_pulseWave1Parameters.Byte2.SweepDividerPeriod + 1 - (apuTicks - m_sweepDividerCounter);
			ClockSweepUnit();
		}

		m_lengthCounter -= apuTicks;
	}

	void ClockWaveform()
	{
		
	}

	void ClockSweepUnit()
	{
		if (m_pulseWave1Parameters.Byte2.SweepEnabledFlag)
		{
			const uint16_t periodAdjustment = m_pulseWave1Parameters.Bytes3and4.RawPeriod >> m_pulseWave1Parameters.Byte2.SweepShiftCount;

			if (m_pulseWave1Parameters.Byte2.SweepNegateFlag)
				m_pulseWave1Parameters.Bytes3and4.RawPeriod -= periodAdjustment;
			else
				m_pulseWave1Parameters.Bytes3and4.RawPeriod += periodAdjustment;
		}
	}


private:
	uint32_t m_timerPeriod = 0;
	uint32_t m_timer = 0;
	uint32_t m_lengthCounter = 0;
	uint8_t m_sequencerOffset = 0;
	uint16_t m_duty[8];

	uint16_t m_sweepDividerCounter = 0;

	PulseWaveParameters m_pulseWave1Parameters;

};

class Apu : public IApu
{
public:
	Apu();

	virtual void Reset(bool isHardReset) override;
	virtual void SetCpu(CPU::Cpu6502* pCpu) override;
	//virtual void AddCycles(uint32_t cpuCycles) override;
	virtual void WriteMemory8(uint16_t offset, uint8_t value) override;
	virtual uint8_t ReadStatus() override;
	virtual void PushAudio() override;

	void EnableSound(bool isEnabled) override;

private:
	uint32_t GetElapsedCpuTime() const;
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
	std::unique_ptr<uint8_t[]> m_spAudioData;
	uint32_t m_cbAudioData = 0;
	uint32_t m_audioWriteOffset = 0;
	int64_t m_cpuCyclesBias = 0;

	std::unique_ptr<uint8_t[]> m_spPulse1AudioData;
	std::unique_ptr<uint8_t[]> m_spPulse2AudioData;
	std::unique_ptr<uint8_t[]> m_spTriangleAudioData;

	std::shared_ptr<IAudioSource> m_spPulse1AudioSource;
	std::shared_ptr<IAudioSource> m_spPulse2AudioSource;
	std::shared_ptr<IAudioSource> m_spTriangleAudioSource;

	PulseGenerator m_pulse1Gen;

	CPU::Cpu6502* m_pCpu;
};


Apu::Apu()
{
	m_spAudioDevice = CreateXAudioDevice();

	m_spPulse1AudioSource = m_spAudioDevice->AddAudioSource();
	m_spPulse2AudioSource = m_spAudioDevice->AddAudioSource();
	m_spTriangleAudioSource = m_spAudioDevice->AddAudioSource();

}

void Apu::Reset(bool isHardReset)
{
	m_cpuCyclesBias = 0;

}


void Apu::SetCpu(CPU::Cpu6502* pCpu)
{
	m_pCpu = pCpu;

}


inline int GetPulseFrequencyFromTimerValue(uint32_t timerPeriod)
{
	return c_cpuFrequency / (16 * (timerPeriod + 1));
}


void Apu::EnableSound(bool isEnabled)
{
	m_isSoundEnabled = isEnabled;
}


void Apu::GeneratePulseWaveAudioSourceData(const PulseWaveParameters& params, size_t *pCbAudioData, std::unique_ptr<uint8_t[]> *pAudioDataSmartPtr, IAudioSource* pAudioSource)
{
	int timerPeriod = params.Bytes3and4.RawPeriod;

	if (timerPeriod < 8 || (params.Byte1.ConstantVolumeFlag == 1 && params.Byte1.Volume == 0))
	{
		pAudioSource->Stop();
		return;
	}

	int pulseFrequency = GetPulseFrequencyFromTimerValue(timerPeriod);

	static const int16_t dutyCycleSequences[4][8] = 
	{ { SHRT_MIN, SHRT_MAX, SHRT_MIN, SHRT_MIN, SHRT_MIN, SHRT_MIN, SHRT_MIN, SHRT_MIN }, // 01000000
	  { SHRT_MIN, SHRT_MAX, SHRT_MAX, SHRT_MIN, SHRT_MIN, SHRT_MIN, SHRT_MIN, SHRT_MIN }, // 01100000
	  { SHRT_MIN, SHRT_MAX, SHRT_MAX, SHRT_MAX, SHRT_MAX, SHRT_MIN, SHRT_MIN, SHRT_MIN }, // 01111000
	  { SHRT_MAX, SHRT_MIN, SHRT_MAX, SHRT_MAX, SHRT_MAX, SHRT_MAX, SHRT_MAX, SHRT_MAX }, // 10011111
	};

	const int16_t* pDutySequence = dutyCycleSequences[params.Byte1.DutyCycle];
	const int samplesPerSecond = pAudioSource->GetSamplesPerSecond();
	const int samplesPerWave = samplesPerSecond / pulseFrequency;

	size_t cbSampleData = pAudioSource->GetBytesPerSample() * samplesPerWave;

	if (cbSampleData > *pCbAudioData)
	{
		size_t toAllocate = std::max((size_t)800, cbSampleData);
		(*pAudioDataSmartPtr) = std::make_unique<uint8_t[]>(toAllocate);
		(*pCbAudioData) = toAllocate;
	}

	int16_t* pSampleData = reinterpret_cast<int16_t*>((*pAudioDataSmartPtr).get());
	const int samplesPerCycleEntry = std::max(1, samplesPerWave / 8);
	for (int sample = 0; sample < samplesPerWave; ++sample)
	{
		pSampleData[sample] = pDutySequence[(sample / samplesPerCycleEntry) % 8];
	}

	pAudioSource->SetChannelData((*pAudioDataSmartPtr).get(), cbSampleData, true /*shouldLoop*/);
	pAudioSource->Play();
}

inline int GetTriangleFrequencyFromTimerValue(uint32_t timerPeriod)
{
	return c_cpuFrequency / (32 * (timerPeriod + 1));
}

uint32_t Apu::GetElapsedCpuTime() const
{
	return static_cast<uint32_t>(m_pCpu->GetElapsedCycles() - m_cpuCyclesBias);
}


void Apu::GenerateTriangleWaveAudioSourceData(const TriangleWaveParameters& params, size_t *pCbAudioData, std::unique_ptr<uint8_t[]> *pAudioDataSmartPtr, IAudioSource* pAudioSource)
{
	int timerPeriod = params.Bytes3and4.RawPeriod;

	if (timerPeriod < 8)
	{
		pAudioSource->Stop();
		return;
	}

	int triangleFrequency = GetTriangleFrequencyFromTimerValue(timerPeriod);

	const int samplesPerSecond = pAudioSource->GetSamplesPerSecond();
	const int samplesPerWave = samplesPerSecond / triangleFrequency;

	size_t cbSampleData = pAudioSource->GetBytesPerSample() * samplesPerWave;

	if (cbSampleData > *pCbAudioData)
	{
		size_t toAllocate = std::max((size_t)800, cbSampleData);
		(*pAudioDataSmartPtr) = std::make_unique<uint8_t[]>(toAllocate);
		(*pCbAudioData) = toAllocate;
	}

	int16_t* pSampleData = reinterpret_cast<int16_t*>((*pAudioDataSmartPtr).get());
	const int samplesPerHalfWave = samplesPerWave / 2;
	const int valueStepsPerSample = USHRT_MAX / samplesPerHalfWave;

	for (int sample = 0; sample < samplesPerHalfWave; ++sample)
	{
		pSampleData[sample] = static_cast<int16_t>(SHRT_MAX - (sample * valueStepsPerSample));
	}

	for (int sample = samplesPerHalfWave; sample < samplesPerWave; ++sample)
	{
		pSampleData[sample] = static_cast<int16_t>(SHRT_MIN + (sample - samplesPerHalfWave) * valueStepsPerSample);
	}

	pAudioSource->SetChannelData((*pAudioDataSmartPtr).get(), cbSampleData, true /*shouldLoop*/);
	pAudioSource->Play();
}

void Apu::PushAudio()
{
	static const int16_t dutyCycleSequences[4][8] = {
		{ SHRT_MIN, SHRT_MAX, SHRT_MIN, SHRT_MIN, SHRT_MIN, SHRT_MIN, SHRT_MIN, SHRT_MIN }, // 01000000
		{ SHRT_MIN, SHRT_MAX, SHRT_MAX, SHRT_MIN, SHRT_MIN, SHRT_MIN, SHRT_MIN, SHRT_MIN }, // 01100000
		{ SHRT_MIN, SHRT_MAX, SHRT_MAX, SHRT_MAX, SHRT_MAX, SHRT_MIN, SHRT_MIN, SHRT_MIN }, // 01111000
		{ SHRT_MAX, SHRT_MIN, SHRT_MAX, SHRT_MAX, SHRT_MAX, SHRT_MAX, SHRT_MAX, SHRT_MAX }, // 10011111
	};

	if (m_spAudioData == nullptr)
	{
		m_cbAudioData = m_spPulse1AudioSource->GetSamplesPerSecond() * m_spPulse1AudioSource->GetBytesPerSample();
		m_spAudioData =	std::make_unique<uint8_t[]>(m_cbAudioData);
		m_audioWriteOffset = 0;
	}

	const int64_t elapsedCycles = GetElapsedCpuTime();
	const uint32_t c_cpuCyclesPerSecond = 1789773;


	m_pulse1Gen.AddTicks(static_cast<uint32_t>(elapsedCycles));

	uint32_t milliSecondsToPush = (uint32_t)(elapsedCycles * 1000 / c_cpuCyclesPerSecond);
	uint32_t samplesToGenerate = milliSecondsToPush * m_spPulse1AudioSource->GetSamplesPerSecond() / 1000;

	uint32_t cbBufferData = milliSecondsToPush * m_spPulse1AudioSource->GetSamplesPerSecond() / 1000 * m_spPulse1AudioSource->GetBytesPerSample();
	if (m_audioWriteOffset + cbBufferData >= m_cbAudioData)
		m_audioWriteOffset = 0;

	uint8_t* pBufferData = m_spAudioData.get() + m_audioWriteOffset;
	m_audioWriteOffset += cbBufferData;
	memset(pBufferData, 0, cbBufferData);

	for (int iWave = 0; iWave < 2; ++iWave)
	{
		const auto& params = iWave == 0 ? m_pulseWave1Parameters : m_pulseWave2Parameters;
		int timerPeriod = params.Bytes3and4.RawPeriod;
		int pulseFrequency = GetPulseFrequencyFromTimerValue(timerPeriod);

		if (timerPeriod < 8 || (params.Byte1.ConstantVolumeFlag == 1 && params.Byte1.Volume == 0))
		{
			continue;
			//m_spPulse1AudioSource->Stop();
			//return;
		}

		const int samplesPerSecond = m_spPulse1AudioSource->GetSamplesPerSecond();
		const int samplesPerWave = samplesPerSecond / pulseFrequency;

		int16_t* pSampleData = reinterpret_cast<int16_t*>(pBufferData);
		const int samplesPerCycleEntry = std::max(1, samplesPerWave / 8);

		uint32_t index = 0;
		const int16_t* pDutySequence = dutyCycleSequences[params.Byte1.DutyCycle];
		uint32_t totalSamples = cbBufferData / m_spPulse1AudioSource->GetBytesPerSample();
		while (index < totalSamples)
		{
			for (int sample = 0; sample < samplesPerWave && index < totalSamples; ++sample)
			{
				int16_t sampleEntry = pDutySequence[(sample / samplesPerCycleEntry) % 8];
				if ((int)sampleEntry + pSampleData[index] < SHRT_MIN)
					pSampleData[index] = SHRT_MIN;
				else if ((int)sampleEntry + pSampleData[index] > SHRT_MAX)
					pSampleData[index] = SHRT_MAX;
				else
					pSampleData[index] += sampleEntry;

				++index;
			}
		}
	}

	m_spPulse1AudioSource->SetChannelData(pBufferData, cbBufferData, false /*shouldLoop*/);
	m_spPulse1AudioSource->Play();

	m_cpuCyclesBias += elapsedCycles;
	//m_accumulatedCpuCycles = 0;
}

uint8_t Apu::ReadStatus()
{
	return 0;
}

void Apu::WriteMemory8(uint16_t offset, uint8_t value)
{
	if (!m_isSoundEnabled)
		return;

	if (offset >= 0x4000 && offset < 0x4004)
	{
		SetPulseWaveParameters(offset - 0x4000, value, &m_pulseWave1Parameters);
		//GeneratePulseWaveAudioSourceData(m_pulseWave1Parameters, &m_cbPulse1AudioData, &m_spPulse1AudioData, m_spPulse1AudioSource.get());
	}
	else if (offset >= 0x4004 && offset < 0x4008)
	{
		SetPulseWaveParameters(offset - 0x4004, value, &m_pulseWave2Parameters);
		//GeneratePulseWaveAudioSourceData(m_pulseWave2Parameters, &m_cbPulse2AudioData, &m_spPulse2AudioData, m_spPulse2AudioSource.get());
	}
	else if (offset >= 0x4008 && offset < 0x400C)
	{
		SetTriangleWaveParameters(offset - 0x4008, value, &m_triangleWaveParameters);
		//GenerateTriangleWaveAudioSourceData(m_triangleWaveParameters, &m_cbTriangleAudioData, &m_spTriangleAudioData, m_spTriangleAudioSource.get());
	}
}

void Apu::SetPulseWaveParameters(uint16_t offset, uint8_t value, _Inout_ PulseWaveParameters *pPulseWaveParameters)
{
	if (offset == 0)
		pPulseWaveParameters->Byte1.ByteValue = value;
	else if (offset == 2)
		pPulseWaveParameters->Bytes3and4.Byte3Value = value;
	else if (offset == 3)
		pPulseWaveParameters->Bytes3and4.Byte4Value = value;
}


void Apu::SetTriangleWaveParameters(uint16_t offset, uint8_t value, _Inout_ TriangleWaveParameters *pPulseWaveParameters)
{
	if (offset == 0)
	{
		pPulseWaveParameters->Byte1.ByteValue = value;
	}
	else if (offset == 2)
	{
		pPulseWaveParameters->Bytes3and4.Byte3Value = value;
	}
	else if (offset == 3)
	{
		pPulseWaveParameters->Bytes3and4.Byte4Value = value;
	}
}


std::unique_ptr<IApu> CreateApu()
{
	return std::make_unique<Apu>();
}


} } // NES::APU

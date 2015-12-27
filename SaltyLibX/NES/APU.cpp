#include "stdafx.h"

#include "APU.h"

#include "Objbase.h"
#include <stdexcept>

#include <xaudio2.h>
#include <algorithm>

#include "../Util/ComPtr.h"

namespace NES { namespace APU {

class ComInitializer
{
public:
	~ComInitializer()
	{
		if (m_shouldUninitializeCom)
			CoUninitialize();
	}

	void InitializeCom(DWORD dwCoInit)
	{
		VerifyHr(CoInitializeEx(NULL, dwCoInit));
		m_shouldUninitializeCom = true;
	}

	void UninitializeCom() _NOEXCEPT
	{
		if (m_shouldUninitializeCom)
			CoUninitialize();
		m_shouldUninitializeCom = false;
	}

private:
	bool m_shouldUninitializeCom = false;
};

class XAudioDevice : public IAudioDevice
{
public:
	XAudioDevice();
	~XAudioDevice();

	void Initialize();

	virtual std::shared_ptr<IAudioSource> AddAudioSource() override;

private:
	ComInitializer m_autoComInitializer;
	CComPtr<IXAudio2> m_spXAudio2;
	IXAudio2MasteringVoice* m_pMasteringVoice = nullptr;
};


XAudioDevice::XAudioDevice()
{

}

XAudioDevice::~XAudioDevice()
{
	CoUninitialize();
}

void XAudioDevice::Initialize()
{
	m_autoComInitializer.InitializeCom(COINIT_APARTMENTTHREADED);
	VerifyHr(XAudio2Create(&m_spXAudio2));

	VerifyHr(m_spXAudio2->CreateMasteringVoice(&m_pMasteringVoice));

}

class XAudioSource : public IAudioSource
{
public:
	~XAudioSource();
	void Initialize(IXAudio2* pXAudio);

	static const int c_samplesPerSecond = 44100;
	static const int c_bitsPerSample = 16;

	virtual uint32_t GetBytesPerSample() const override { return c_bitsPerSample / 8; }
	virtual uint32_t GetSamplesPerSecond() const override { return c_samplesPerSecond; }

	virtual void SetVolume(float volume) override;
	virtual void SetChannelData(const uint8_t* pData, size_t cbData, bool shouldLoop) override;
	virtual void Play() override;
	virtual void Stop() override;

private:
	IXAudio2SourceVoice* m_pXAudioSourceVoice = nullptr;
};

XAudioSource::~XAudioSource()
{
	if (m_pXAudioSourceVoice != nullptr)
	{
		m_pXAudioSourceVoice->DestroyVoice();
		m_pXAudioSourceVoice = nullptr;
	}
}

std::shared_ptr<IAudioSource> XAudioDevice::AddAudioSource()
{
	auto spAudioSource = std::make_shared<XAudioSource>();
	spAudioSource->Initialize(m_spXAudio2);

	return spAudioSource;
}


void XAudioSource::Initialize(IXAudio2* pXAudio)
{
	// Create a source voice
	WAVEFORMATEX waveformat;
	waveformat.wFormatTag = WAVE_FORMAT_PCM;
	waveformat.nChannels = 1;
	waveformat.nSamplesPerSec = c_samplesPerSecond;
	waveformat.nAvgBytesPerSec = c_samplesPerSecond * GetBytesPerSample();
	waveformat.nBlockAlign = static_cast<WORD>(GetBytesPerSample());
	waveformat.wBitsPerSample = XAudioSource::c_bitsPerSample;
	waveformat.cbSize = 0;

	VerifyHr(pXAudio->CreateSourceVoice(&m_pXAudioSourceVoice, &waveformat));

	SetVolume(1.0f);

}
void XAudioSource::SetChannelData(const uint8_t* pData, size_t cbData, bool /*shouldLoop*/)
{
	XAUDIO2_VOICE_STATE voiceState;
	m_pXAudioSourceVoice->GetState(&voiceState, 0);

	if (voiceState.BuffersQueued > 1)
		return;

	XAUDIO2_BUFFER buffer = { 0 };
	buffer.AudioBytes = static_cast<uint32_t>(cbData);
	buffer.pAudioData = reinterpret_cast<const BYTE*>(pData);
	buffer.PlayBegin = 0;
	buffer.PlayLength = 0;
	//buffer.LoopCount = 2; // TODO: Real sample length
	//buffer.LoopCount = 200; // TODO: Real sample length

	//m_pXAudioSourceVoice->ExitLoop();
	//m_pXAudioSourceVoice->Stop();
	//m_pXAudioSourceVoice->FlushSourceBuffers();

	//XAUDIO2_VOICE_STATE state;
	//m_pXAudioSourceVoice->GetState(&state, 0);

	VerifyHr(m_pXAudioSourceVoice->SubmitSourceBuffer(&buffer));
}

void XAudioSource::Play()
{
	VerifyHr(m_pXAudioSourceVoice->Start());
}

void XAudioSource::Stop()
{
	VerifyHr(m_pXAudioSourceVoice->Stop());
}

void XAudioSource::SetVolume(float volume)
{
	static const float c_baseVolume = 0.1f;
	m_pXAudioSourceVoice->SetVolume(volume * c_baseVolume);
}

std::shared_ptr<IAudioDevice> CreateXAudioDevice()
{
	auto spXAudioDevice = std::make_shared<XAudioDevice>();
	spXAudioDevice->Initialize();
	return spXAudioDevice;
}

Apu::Apu()
{
	m_spAudioDevice = CreateXAudioDevice();

	m_spPulse1AudioSource = m_spAudioDevice->AddAudioSource();
	m_spPulse2AudioSource = m_spAudioDevice->AddAudioSource();
	m_spTriangleAudioSource = m_spAudioDevice->AddAudioSource();

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


void Apu::AddCycles(uint32_t cpuCycles)
{
	m_accumulatedCpuCycles += cpuCycles;
}

void Apu::PushAudio()
{

	static const int16_t dutyCycleSequences[4][8] = 
	{ { SHRT_MIN, SHRT_MAX, SHRT_MIN, SHRT_MIN, SHRT_MIN, SHRT_MIN, SHRT_MIN, SHRT_MIN }, // 01000000
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

	const uint32_t c_cpuCyclesPerSecond = 1789773;
	uint32_t milliSecondsToPush = m_accumulatedCpuCycles * 1000 / c_cpuCyclesPerSecond;
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

	m_accumulatedCpuCycles = 0;
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
		GenerateTriangleWaveAudioSourceData(m_triangleWaveParameters, &m_cbTriangleAudioData, &m_spTriangleAudioData, m_spTriangleAudioSource.get());
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


} } // NES::APU

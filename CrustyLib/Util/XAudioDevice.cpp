#include "stdafx.h"
#include "IAudioDevice.h"
#include "Objbase.h"
#include "ComPtr.h"
#include <xaudio2.h>

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
	void Initialize();

	virtual std::shared_ptr<IAudioSource> AddAudioSource() override;

private:
	ComInitializer m_autoComInitializer;
	CComPtr<IXAudio2> m_spXAudio2;
	IXAudio2MasteringVoice* m_pMasteringVoice = nullptr;
};


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

}
void XAudioSource::SetChannelData(const uint8_t* pData, size_t cbData, bool /*shouldLoop*/)
{
	XAUDIO2_VOICE_STATE voiceState;
	m_pXAudioSourceVoice->GetState(&voiceState);

	// Hacky: Avoid pile up if the audio engine is running faster than realtime.
	//  We will just drop new frames if things are getting jammed up. We should ideally
	//  drop frames from earlier in the queue if we haven't gotten to them yet, or even
	//  better assert that our audio playback isn't getting ahead of us
	if (voiceState.BuffersQueued > 5)
		return;

	XAUDIO2_BUFFER buffer = { 0 };
	buffer.AudioBytes = static_cast<uint32_t>(cbData);
	buffer.pAudioData = reinterpret_cast<const BYTE*>(pData);

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

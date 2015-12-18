#include "stdafx.h"

#include "APU.h"

#include "Objbase.h"
#include <stdexcept>
//#include <atlbase.h>

#include <xaudio2.h>
#include <algorithm>


template <class T>
class _NoAddRefReleaseOnCComPtr : 
	public T
{
private:
	STDMETHOD_(ULONG, AddRef)()=0;
	STDMETHOD_(ULONG, Release)()=0;
};

template <class T>
class CComPtrBase
{
protected:
	CComPtrBase() throw()
	{
		p = NULL;
	}
	CComPtrBase(_Inout_opt_ T* lp) throw()
	{
		p = lp;
		if (p != NULL)
			p->AddRef();
	}
	void Swap(CComPtrBase& other)
	{
		T* pTemp = p;
		p = other.p;
		other.p = pTemp;
	}
public:
	typedef T _PtrClass;
	~CComPtrBase() throw()
	{
		if (p)
			p->Release();
	}
	operator T*() const throw()
	{
		return p;
	}
	T& operator*() const
	{
		ATLENSURE(p!=NULL);
		return *p;
	}
	//The assert on operator& usually indicates a bug.  If this is really
	//what is needed, however, take the address of the p member explicitly.
	T** operator&() throw()
	{
		return &p;
	}
	_NoAddRefReleaseOnCComPtr<T>* operator->() const throw()
	{
		return (_NoAddRefReleaseOnCComPtr<T>*)p;
	}
	bool operator!() const throw()
	{	
		return (p == NULL);
	}
	bool operator<(_In_opt_ T* pT) const throw()
	{
		return p < pT;
	}
	bool operator!=(_In_opt_ T* pT) const
	{
		return !operator==(pT);
	}
	bool operator==(_In_opt_ T* pT) const throw()
	{
		return p == pT;
	}

	// Release the interface and set to NULL
	void Release() throw()
	{
		T* pTemp = p;
		if (pTemp)
		{
			p = NULL;
			pTemp->Release();
		}
	}
	// Compare two objects for equivalence
	bool IsEqualObject(_Inout_opt_ IUnknown* pOther) throw()
	{
		if (p == NULL && pOther == NULL)
			return true;	// They are both NULL objects

		if (p == NULL || pOther == NULL)
			return false;	// One is NULL the other is not

		CComPtr<IUnknown> punk1;
		CComPtr<IUnknown> punk2;
		p->QueryInterface(__uuidof(IUnknown), (void**)&punk1);
		pOther->QueryInterface(__uuidof(IUnknown), (void**)&punk2);
		return punk1 == punk2;
	}
	// Attach to an existing interface (does not AddRef)
	void Attach(_In_opt_ T* p2) throw()
	{
		if (p)
		{
			ULONG ref = p->Release();
			(ref);
			// Attaching to the same object only works if duplicate references are being coalesced.  Otherwise
			// re-attaching will cause the pointer to be released and may cause a crash on a subsequent dereference.
		}
		p = p2;
	}
	// Detach the interface (does not Release)
	T* Detach() throw()
	{
		T* pt = p;
		p = NULL;
		return pt;
	}
	_Check_return_ HRESULT CopyTo(_COM_Outptr_result_maybenull_ T** ppT) throw()
	{
		if (ppT == NULL)
			return E_POINTER;
		*ppT = p;
		if (p)
			p->AddRef();
		return S_OK;
	}
	_Check_return_ HRESULT SetSite(_Inout_opt_ IUnknown* punkParent) throw()
	{
		return AtlSetChildSite(p, punkParent);
	}
	_Check_return_ HRESULT Advise(
		_Inout_ IUnknown* pUnk, 
		_In_ const IID& iid, 
		_Out_ LPDWORD pdw) throw()
	{
		return AtlAdvise(p, pUnk, iid, pdw);
	}
	_Check_return_ HRESULT CoCreateInstance(
		_In_ REFCLSID rclsid, 
		_Inout_opt_ LPUNKNOWN pUnkOuter = NULL, 
		_In_ DWORD dwClsContext = CLSCTX_ALL) throw()
	{
		return ::CoCreateInstance(rclsid, pUnkOuter, dwClsContext, __uuidof(T), (void**)&p);
	}
#ifdef _ATL_USE_WINAPI_FAMILY_DESKTOP_APP
	_Check_return_ HRESULT CoCreateInstance(
		_In_z_ LPCOLESTR szProgID, 
		_Inout_opt_ LPUNKNOWN pUnkOuter = NULL, 
		_In_ DWORD dwClsContext = CLSCTX_ALL) throw()
	{
		CLSID clsid;
		HRESULT hr = CLSIDFromProgID(szProgID, &clsid);
		if (SUCCEEDED(hr))
			hr = ::CoCreateInstance(clsid, pUnkOuter, dwClsContext, __uuidof(T), (void**)&p);
		return hr;
	}
#endif // _ATL_USE_WINAPI_FAMILY_DESKTOP_APP
	template <class Q>
	_Check_return_ HRESULT QueryInterface(_Outptr_ Q** pp) const throw()
	{
		return p->QueryInterface(__uuidof(Q), (void**)pp);
	}
	T* p;
};

template <class T>
class CComPtr : 
	public CComPtrBase<T>
{
public:
	CComPtr() throw()
	{
	}
	CComPtr(_Inout_opt_ T* lp) throw() :
		CComPtrBase<T>(lp)
	{
	}
	CComPtr(_Inout_ const CComPtr<T>& lp) throw() :
		CComPtrBase<T>(lp.p)
	{	
	}
	T* operator=(_Inout_opt_ T* lp) throw()
	{
		if(*this!=lp)
		{
			CComPtr(lp).Swap(*this);
		}
		return *this;
	}
	template <typename Q>
	T* operator=(_Inout_ const CComPtr<Q>& lp) throw()
	{
		if( !IsEqualObject(lp) )
		{
			return static_cast<T*>(AtlComQIPtrAssign((IUnknown**)&p, lp, __uuidof(T)));
		}
		return *this;
	}
	T* operator=(_Inout_ const CComPtr<T>& lp) throw()
	{
		if(*this!=lp)
		{
			CComPtr(lp).Swap(*this);
		}
		return *this;
	}	
	CComPtr(_Inout_ CComPtr<T>&& lp) throw() :	
		CComPtrBase<T>()
	{	
		lp.Swap(*this);
	}	
	T* operator=(_Inout_ CComPtr<T>&& lp) throw()
	{			
		if (*this != lp)
		{
			CComPtr(static_cast<CComPtr&&>(lp)).Swap(*this);
		}
		return *this;		
	}
};


void VerifyHr(HRESULT hr)
{
	if (FAILED(hr))
		throw std::runtime_error("Unexpected bad HRESULT");
}

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
	XAUDIO2_BUFFER buffer = { 0 };
	buffer.AudioBytes = static_cast<uint32_t>(cbData);
	buffer.pAudioData = reinterpret_cast<const BYTE*>(pData);
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.PlayBegin = 0;
	buffer.PlayLength = 0;
	buffer.LoopCount = 200; // TODO: Real sample length

	m_pXAudioSourceVoice->ExitLoop();
	m_pXAudioSourceVoice->Stop();
	m_pXAudioSourceVoice->FlushSourceBuffers();

	XAUDIO2_VOICE_STATE state;
	m_pXAudioSourceVoice->GetState(&state, 0);

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

void Apu::WriteMemory8(uint16_t offset, uint8_t value)
{
	if (offset >= 0x4000 && offset < 0x4004)
	{
		SetPulseWaveParameters(offset - 0x4000, value, &m_pulseWave1Parameters);
		GeneratePulseWaveAudioSourceData(m_pulseWave1Parameters, &m_cbPulse1AudioData, &m_spPulse1AudioData, m_spPulse1AudioSource.get());
	}
	else if (offset >= 0x4004 && offset < 0x4008)
	{
		SetPulseWaveParameters(offset - 0x4004, value, &m_pulseWave2Parameters);
		GeneratePulseWaveAudioSourceData(m_pulseWave2Parameters, &m_cbPulse2AudioData, &m_spPulse2AudioData, m_spPulse2AudioSource.get());
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

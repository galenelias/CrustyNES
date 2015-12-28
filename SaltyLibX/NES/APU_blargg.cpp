#include "stdafx.h"

#include "APU_blargg.h"

#include "Objbase.h"
#include <stdexcept>

#include <xaudio2.h>
#include <algorithm>

#include "../Util/ComPtr.h"
#include "../Util/IAudioDevice.h"
#include "APU.h"

namespace NES { namespace APU { namespace blargg {

const uint32_t c_cpuCyclesPerSecond = 1789773;

class Apu : public IApu
{
public:
	Apu();

	virtual void AddCycles(uint32_t cpuCycles) override;
	virtual void WriteMemory8(uint16_t offset, uint8_t value) override;
	virtual uint8_t ReadStatus() override;
	virtual void PushAudio() override;

	void EnableSound(bool isEnabled) override;

private:
	std::shared_ptr<IAudioDevice> m_spAudioDevice;

	bool m_isSoundEnabled = true;
	uint32_t m_accumulatedCpuCycles = 0;

	std::unique_ptr<uint8_t[]> m_spAudioData;
	uint32_t m_cbAudioData = 0;
	uint32_t m_audioWriteOffset = 0;

	std::shared_ptr<IAudioSource> m_spPulse1AudioSource;

	Blip_Buffer m_blipBuf;
	Nes_Apu m_nesApu;
};


Apu::Apu()
{
	m_spAudioDevice = CreateXAudioDevice();

	m_spPulse1AudioSource = m_spAudioDevice->AddAudioSource();

	m_blipBuf.sample_rate(44100);
	m_blipBuf.clock_rate(c_cpuCyclesPerSecond);

	m_nesApu.output(&m_blipBuf);

}

void Apu::EnableSound(bool isEnabled)
{
	m_isSoundEnabled = isEnabled;
}

void Apu::AddCycles(uint32_t cpuCycles)
{
	m_accumulatedCpuCycles += cpuCycles;
}

void Apu::PushAudio()
{
	m_nesApu.end_frame(m_accumulatedCpuCycles);
	m_blipBuf.end_frame(m_accumulatedCpuCycles);

	if (m_spAudioData == nullptr)
	{
		m_cbAudioData = m_spPulse1AudioSource->GetSamplesPerSecond() * m_spPulse1AudioSource->GetBytesPerSample();
		m_spAudioData =	std::make_unique<uint8_t[]>(m_cbAudioData);
		m_audioWriteOffset = 0;
	}

	if (m_blipBuf.samples_avail() > 0)
	{
		auto samplesAvail = m_blipBuf.samples_avail();

		uint32_t cbBufferData = samplesAvail * m_spPulse1AudioSource->GetBytesPerSample();
		if (m_audioWriteOffset + cbBufferData >= m_cbAudioData)
			m_audioWriteOffset = 0;

		uint8_t* pBufferData = m_spAudioData.get() + m_audioWriteOffset;
		m_audioWriteOffset += cbBufferData;
		memset(pBufferData, 0, cbBufferData);

		size_t count = m_blipBuf.read_samples((blip_sample_t*)pBufferData, samplesAvail);
		m_spPulse1AudioSource->SetChannelData(pBufferData, cbBufferData, false /*shouldLoop*/);
		m_spPulse1AudioSource->Play();
	}
	m_accumulatedCpuCycles = 0;
}

uint8_t Apu::ReadStatus()
{
	return m_nesApu.read_status(m_accumulatedCpuCycles);
}

void Apu::WriteMemory8(uint16_t offset, uint8_t value)
{
	if (!m_isSoundEnabled)
		return;

	if (offset >= m_nesApu.start_addr && offset <= m_nesApu.end_addr)
		m_nesApu.write_register(m_accumulatedCpuCycles, offset, value);
}


std::unique_ptr<IApu> CreateBlarggApu()
{
	return std::make_unique<Apu>();
}

} } } // NES::APU::blargg

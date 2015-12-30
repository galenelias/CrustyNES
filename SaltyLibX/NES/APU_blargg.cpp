#include "stdafx.h"

#include "APU_blargg.h"

#include <stdexcept>

#include "../Util/IAudioDevice.h"
#include "APU.h"
#include "Cpu6502.h"

namespace NES { namespace APU { namespace blargg {

const uint32_t c_cpuCyclesPerSecond = 1789773;

class Apu : public IApu
{
public:
	Apu();

	virtual void SetCpu(CPU::Cpu6502* pCpu) override;
	virtual void WriteMemory8(uint16_t offset, uint8_t value) override;
	virtual uint8_t ReadStatus() override;
	virtual void PushAudio() override;

	void EnableSound(bool isEnabled) override;

private:
	cpu_time_t GetElapsedCpuTime() const;

	std::shared_ptr<IAudioDevice> m_spAudioDevice;

	int64_t m_cpuCyclesBias = 0;

	std::unique_ptr<uint8_t[]> m_spAudioData;
	uint32_t m_cbAudioData = 0;
	uint32_t m_audioWriteOffset = 0;

	std::shared_ptr<IAudioSource> m_spAudioSource;

	CPU::Cpu6502* m_pCpu;
	Blip_Buffer m_blipBuf;
	Nes_Apu m_nesApu;
};


Apu::Apu()
{
	m_spAudioDevice = CreateXAudioDevice();
	m_spAudioSource = m_spAudioDevice->AddAudioSource();

	m_blipBuf.sample_rate(44100);
	m_blipBuf.clock_rate(c_cpuCyclesPerSecond);

	m_nesApu.output(&m_blipBuf);

	m_nesApu.dmc_reader( [](void*, cpu_addr_t addr)->int
	{
		// TODO: Hook up to real CPU reads so DMC sound works correctly
		return 0;
	});
}


void Apu::SetCpu(CPU::Cpu6502* pCpu)
{
	m_pCpu = pCpu;
}


void Apu::EnableSound(bool isEnabled)
{
	if (isEnabled)
		m_nesApu.output(&m_blipBuf);
	else
		m_nesApu.output(nullptr);
}

cpu_time_t Apu::GetElapsedCpuTime() const
{
	return static_cast<cpu_time_t>(m_pCpu->GetElapsedCycles() - m_cpuCyclesBias);
}


void Apu::PushAudio()
{
	const int64_t elapsedCycles = GetElapsedCpuTime();
	m_nesApu.end_frame(static_cast<cpu_time_t>(elapsedCycles));
	m_blipBuf.end_frame(static_cast<cpu_time_t>(elapsedCycles));

	if (m_spAudioData == nullptr)
	{
		// Allocate a full second worth of audio data for now
		m_cbAudioData = m_spAudioSource->GetSamplesPerSecond() * m_spAudioSource->GetBytesPerSample();
		m_spAudioData =	std::make_unique<uint8_t[]>(m_cbAudioData);
		m_audioWriteOffset = 0;
	}

	if (m_blipBuf.samples_avail() > 0)
	{
		auto samplesAvail = m_blipBuf.samples_avail();

		uint32_t cbBufferData = samplesAvail * m_spAudioSource->GetBytesPerSample();

		// If there isn't enough room in our buffer, circle back around to the front
		//  and kind of just hope we're not blowing away data which is still being played
		if (m_audioWriteOffset + cbBufferData >= m_cbAudioData)
			m_audioWriteOffset = 0;

		uint8_t* pBufferData = m_spAudioData.get() + m_audioWriteOffset;
		m_audioWriteOffset += cbBufferData;
		memset(pBufferData, 0, cbBufferData);

		size_t count = m_blipBuf.read_samples((blip_sample_t*)pBufferData, samplesAvail);
		m_spAudioSource->SetChannelData(pBufferData, cbBufferData, false /*shouldLoop*/);
		m_spAudioSource->Play();
	}
	m_cpuCyclesBias += elapsedCycles;
}

uint8_t Apu::ReadStatus()
{
	return m_nesApu.read_status(GetElapsedCpuTime());
}

void Apu::WriteMemory8(uint16_t offset, uint8_t value)
{
	if (offset >= m_nesApu.start_addr && offset <= m_nesApu.end_addr)
		m_nesApu.write_register(GetElapsedCpuTime(), offset, value);
}


std::unique_ptr<IApu> CreateBlarggApu()
{
	return std::make_unique<Apu>();
}

} } } // NES::APU::blargg

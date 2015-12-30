#include "stdafx.h"
#include "NES.h"
#include "APU_blargg.h"

namespace NES
{


NES::NES()
	//: m_spApu(APU::CreateApu())
	: m_spApu(APU::blargg::CreateBlarggApu())
	, m_cpu(*this)
{
	m_ppu.SetCpu(&m_cpu);
	m_spApu->SetCpu(&m_cpu);
}

void NES::RunCycle()
{
	const uint32_t cpuCycles = GetCpu().RunNextInstruction();
	GetPpu().AddCycles(cpuCycles);
}

void NES::RunCycles(int numCycles)
{
	const uint32_t cpuCycles = GetCpu().RunInstructions(numCycles);
	GetPpu().AddCycles(cpuCycles);
}

void NES::LoadRomFile(IReadableFile* pRomFile)
{
	m_rom.LoadRomFromFile(pRomFile);

	m_spMapper = CreateMapper(m_rom.GetMapperId());
	m_spMapper->LoadFromRom(m_rom);

	m_cpu.SetRomMapper(m_spMapper.get());
	m_ppu.SetRomMapper(m_spMapper.get());
}

void NES::Reset()
{
	m_cpu.Reset();
	//m_ppu.Reset() // ?
}

}
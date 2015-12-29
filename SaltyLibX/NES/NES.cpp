#include "stdafx.h"
#include "NES.h"
#include "APU_blargg.h"

namespace NES
{


NES::NES()
	//: m_spApu(APU::CreateApu())
	: m_spApu(APU::blargg::CreateBlarggApu())
	, m_cpu(*this)
	, m_ppu(m_cpu)
{
	
}

void NES::RunCycle()
{
	m_instructionsRan++;

	const uint32_t cpuCycles = GetCpu().RunNextInstruction();
	GetPpu().AddCycles(cpuCycles);
	GetApu().AddCycles(cpuCycles);
}


int NES::GetCyclesRanSoFar() const
{
	return m_instructionsRan;
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
	m_instructionsRan = 0;
	m_cpu.Reset();
	//m_ppu.Reset() // ?
}

}
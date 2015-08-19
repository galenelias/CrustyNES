#include "pch.h"
#include "NES.h"

namespace NES
{


NES::NES()
	: m_cpu(*this)
	, m_ppu(m_cpu)
{
}

void NES::RunCycle()
{
	m_cyclesRan++;

	GetCpu().RunNextInstruction();
	GetPpu().DoStuff();
}


int NES::GetCyclesRanSoFar() const
{
	return m_cyclesRan;
}

void NES::LoadRomFile(IReadableFile* pRomFile)
{
	m_rom.LoadRomFromFile(pRomFile);
	m_cpu.MapRomMemory(m_rom);

	m_spMapper = CreateMapper(m_rom.GetMapperId());
	m_spMapper->LoadFromRom(m_rom);

	if (m_rom.HasChrRom())
		m_ppu.MapRomMemory(m_rom, m_spMapper.get());
}

void NES::Reset()
{
	m_cyclesRan = 0;
	m_cpu.Reset();
	//m_ppu.Reset() // ?
}

}
#pragma once

#include "NESRom.h"
#include "Ppu.h"
#include "Cpu6502.h"
#include "APU.h"
#include "Controller.h"
#include "IMapper.h"


namespace NES
{

class unsupported_mapper : public std::runtime_error
{
public:
	unsupported_mapper(int mapperNumber)
		: m_mapperNum(mapperNumber)
		, std::runtime_error("Unsupported mapper")
	{
	}

	int GetMapperNumber() const { return m_mapperNum; }

private:
	std::string m_errorString;
	int m_mapperNum;
};


class NES
{
public:
	NES();

	void LoadRomFile(IReadableFile* pRomFile);
	void Reset();

	void RunCycle();
	int GetCyclesRanSoFar() const;

	CPU::Cpu6502& GetCpu() { return m_cpu; }
	PPU::Ppu& GetPpu() { return m_ppu; }
	APU::Apu& GetApu() { return m_apu; }

	Controller& UseController1() { return m_controller1; }

private:
	int m_instructionsRan = 0;

	NESRom m_rom;
	PPU::Ppu m_ppu;
	CPU::Cpu6502 m_cpu;
	APU::Apu m_apu;
	Controller m_controller1;

	std::unique_ptr<IMapper> m_spMapper;
};

}
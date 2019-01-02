#pragma once

#include "NESRom.h"
#include "Ppu.h"
#include "Cpu6502.h"
#include "APU.h"
#include "APU_blargg.h"
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

//using ApuClass = APU::Apu;
//using ApuClass = APU::blargg::Apu;

class NES
{
public:
	NES();

	void LoadRomFile(IReadableFile* pRomFile);
	void Reset();

	void RunCycle();
	void RunCycles(int numCycles);

	CPU::Cpu6502& GetCpu() { return m_cpu; }
	PPU::Ppu& GetPpu() { return m_ppu; }
	APU::IApu& GetApu() { return *m_spApu; }

	Controller& UseController1() { return m_controller1; }

private:
	int m_instructionsRan = 0;

	NESRom m_rom;
	std::unique_ptr<APU::IApu> m_spApu;
	PPU::Ppu m_ppu;
	CPU::Cpu6502 m_cpu;
	Controller m_controller1;

	std::unique_ptr<IMapper> m_spMapper;
};

}
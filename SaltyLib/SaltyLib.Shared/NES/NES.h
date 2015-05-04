#pragma once

#include "NESRom.h"
#include "Ppu.h"
#include "Cpu6502.h"
#include "APU.h"
#include "Controller.h"

namespace NES
{


class NES
{
public:
	NES();
	~NES();

	void LoadRomFile(IReadableFile* pRomFile);
	void Reset();

	void RunCycle();
	int GetCyclesRanSoFar() const;

	CPU::Cpu6502& GetCpu() { return m_cpu; }
	PPU::Ppu& GetPpu() { return m_ppu; }
	APU::Apu& GetApu() { return m_apu; }

	Controller& UseController1() { return m_controller1; }
	//void SetController1Status(ControllerInput input, bool isPressed);

private:
	int m_cyclesRan = 0;

	NESRom m_rom;
	PPU::Ppu m_ppu;
	CPU::Cpu6502 m_cpu;
	APU::Apu m_apu;
	Controller m_controller1;
};

}
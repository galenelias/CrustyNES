#pragma once


#include <stdint.h>

namespace PPU
{

class Ppu
{
public:
	Ppu();
	~Ppu();

	bool InVBlank() const { return false; }

	void WriteControlRegister1(uint8_t value); // $2000
	void WriteControlRegister2(uint8_t value); // $2001
	uint8_t ReadPpuStatus() const { return m_ppuStatus; }

private:
	uint8_t ReadMemory8(uint16_t offset);

	void SetVBlankStatus(bool inVBlank);

	uint16_t GetBaseNametableOffset()
	{
		return 0x2000 + (m_ppuCtrl1 & 0x03) * 0x0400;
	}

	struct PpuControlFlags
	{
		uint8_t nametableBaseAddress:2;
		uint8_t vramAddressIncrement:1;
		uint8_t spritePatternTableAddress:1;
		uint8_t backgroundPatternTableAddress:1;
		uint8_t spriteSize:1;
		uint8_t ppuMasterSlave:1;
		uint8_t nmiFlag:1;
	};


	union
	{
		uint8_t m_ppuCtrl1;
		PpuControlFlags m_ppuCtrlFlags;
	};
	uint8_t m_ppuCtrl2;
	uint8_t m_ppuStatus;

	static const uint16_t c_cbVRAM = 16*1024;
	uint8_t m_vram[c_cbVRAM]; // 16KB of video ram
	uint8_t m_sprRam[256]; // Sprite RAM
};

} // namespace Ppu

#pragma once

#include <stdint.h>

// Right now, our pixel output is stored with blue as the least significant value, so we can't use Window's default RGB macro
#define PPU_RGB(r,g,b)          ((COLORREF)(((BYTE)(b)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(r))<<16)))

namespace NES {
	class NESRom;
	class IMapper;
}

namespace CPU {
	class Cpu6502;
}

namespace PPU
{

struct RenderOptions
{
	bool fDrawBackgroundGrid = false;
	bool fDrawSpriteOutline = false;
};

const int c_displayWidth = 256;
const int c_displayHeight = 240;

typedef DWORD ppuDisplayBuffer_t[c_displayHeight][c_displayWidth];

enum class PpuStatusFlag
{
	Bit0 = 0x01,
	Bit1 = 0x02,
	Bit2 = 0x04,
	Bit3 = 0x08,
	Bit4 = 0x10,
	Bit5 = 0x20,
	Bit6 = 0x40,
	InVBlank = 0x80,
};

enum class MirroringMode
{
	HorizontalMirroring,
	VerticalMirroring,
	FourScreen,
};

enum class PixelOutputType : uint8_t
{
	None = 0,
	Background = 1,
	Sprite = 2,
};

typedef PixelOutputType ppuPixelOutputTypeBuffer_t[c_displayHeight][c_displayWidth];


class Ppu
{
public:
	Ppu(CPU::Cpu6502& cpu);

	Ppu(const Ppu&) = delete;
	Ppu& operator=(const Ppu&) = delete;

	void MapRomMemory(const NES::NESRom& rom, NES::IMapper* pMapper);

	bool InVBlank() const { return false; }

	void DoStuff();
	bool ShouldRender();
	void RenderToBuffer(ppuDisplayBuffer_t displayBuffer, const RenderOptions& options);

	void WriteControlRegister1(uint8_t value); // $2000
	void WriteControlRegister2(uint8_t value); // $2001
	uint8_t ReadPpuStatus();
	void WriteCpuAddressRegister(uint8_t value);
	void WriteCpuDataRegister(uint8_t value);   //$2007
	uint8_t ReadCpuDataRegister();   //$2007

	void WriteScrollRegister(uint8_t value);   //$2005

	void WriteOamAddress(uint8_t value);
	void WriteOamData(uint8_t value);
	uint8_t ReadOamData() const;

	void TriggerOamDMA(uint8_t* pData);

private:
	uint8_t ReadMemory8(uint16_t offset);
	void WriteMemory8(uint16_t offset, uint8_t value);

	void UpdateStatusWithLastWrittenRegister(uint8_t value);

	void SetVBlankStatus(bool inVBlank);

	uint16_t GetBaseNametableOffset() const;
	uint16_t GetSpriteNametableOffset() const;

	uint16_t GetPatternTableOffset() const
	{
		return 0x1000 * m_ppuCtrlFlags.backgroundPatternTableAddress;
	}

	uint16_t CpuDataIncrementAmount() const;

	void DrawBkgTile(uint8_t tileNumber, uint8_t highOrderPixelData, int iRow, int iColumn, uint16_t patternTableOffset, ppuDisplayBuffer_t displayBuffer, ppuPixelOutputTypeBuffer_t outputTypeBuffer);
	void DrawSprTile(uint8_t tileNumber, uint8_t highOrderPixelData, int iRow, int iColumn, bool foregroundSprite, bool flipHorizontally, bool flipVertically, uint16_t patternTableOffset, ppuDisplayBuffer_t displayBuffer, ppuPixelOutputTypeBuffer_t outputTypeBuffer);

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

	static const int c_totalScanlines = 240;
	static const int c_pixelsPerScanlines = 340;

	uint8_t m_sprRam[256]; // Sprite RAM

	uint16_t m_cpuPpuAddr = 0;
	uint16_t m_cpuOamAddr = 0;
	uint8_t m_ppuAddressWriteParity = 0; // 0 = first write, 1 = second write

	uint8_t m_horizontalScrollOffset = 0;
	uint8_t m_verticalScrollOffset = 0;
	uint8_t m_scrollWriteParity = 0; // 0 = first write (horizontal), 1 = second write (vertical)


	const uint8_t* m_chrRom;

	int m_ppuClock = 0;
	int m_scanline = 0;
	int m_pixel = 0; // offset within scanline
	bool m_shouldRender = false;

	MirroringMode m_mirroringMode;

	CPU::Cpu6502& m_cpu;
	NES::IMapper* m_pMapper;
};

} // namespace Ppu

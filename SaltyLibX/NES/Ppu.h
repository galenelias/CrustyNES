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


struct PpuStatusFlag
{
	uint8_t Bit0:1;
	uint8_t Bit1 : 1;
	uint8_t Bit2 : 1;
	uint8_t Bit3 : 1;
	uint8_t Bit4 : 1;
	uint8_t Bit5 : 1;
	uint8_t SpriteZeroHit:1;
	uint8_t InVBlank:1;
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

enum SpriteSize : uint8_t
{
	Size8x8 = 0,
	Size8x16 = 1,
};

typedef PixelOutputType ppuPixelOutputTypeBuffer_t[c_displayHeight][c_displayWidth];


class Ppu
{
public:
	Ppu(CPU::Cpu6502& cpu);

	Ppu(const Ppu&) = delete;
	Ppu& operator=(const Ppu&) = delete;

	void SetRomMapper(NES::IMapper* pMapper);
	void SetRenderOptions(const RenderOptions& renderOptions);

	bool InVBlank() const { return false; }

	bool ShouldRender();
	void RenderToBuffer(ppuDisplayBuffer_t displayBuffer, const RenderOptions& options);
	void RenderToBuffer(const RenderOptions& options);
	void RenderScanline(int scanline);

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

	void AddCycles(uint32_t cpuCycles);

	uint32_t GetCycles() const;
	uint32_t GetScanline() const;

	const ppuDisplayBuffer_t& GetDisplayBuffer() const;

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

	uint16_t GetSpriteTileOffset(uint8_t tileNumber, bool is8x8Sprite) const;
	void DrawBkgTile(uint8_t tileNumber, uint8_t highOrderPixelData, int iRow, int iColumn, uint16_t patternTableOffset, ppuDisplayBuffer_t displayBuffer, ppuPixelOutputTypeBuffer_t outputTypeBuffer);
	bool DrawSprTile(uint8_t tileNumber, uint8_t highOrderPixelData, int iRow, int iColumn, bool foregroundSprite, bool flipHorizontally, bool flipVertically, ppuDisplayBuffer_t displayBuffer, ppuPixelOutputTypeBuffer_t outputTypeBuffer);

	struct PpuControlFlags
	{
		uint8_t nametableBaseAddress:2;
		uint8_t vramAddressIncrement:1;
		uint8_t spritePatternTableAddress:1;
		uint8_t backgroundPatternTableAddress:1;
		SpriteSize spriteSize:1;
		uint8_t ppuMasterSlave:1;
		uint8_t nmiFlag:1;
	};

	struct PpuMaskFlags
	{
		uint8_t grayScale:1; // 0 = normal, 1 = gray scale
		uint8_t showBackgroundOnLeft:1;
		uint8_t showSpritesOnLeft:1;
		uint8_t showBackground:1;
		uint8_t showSprites:1;
		uint8_t emphasizeRed:1;
		uint8_t emphasizeGreen:1;
		uint8_t emphasizeBlue:1;
	};


	union
	{
		uint8_t m_ppuCtrl1;
		PpuControlFlags m_ppuCtrlFlags;
	};

	union
	{
		uint8_t m_ppuCtrl2;
		PpuMaskFlags m_ppuMaskFlags;
	};
	
	union
	{
		uint8_t m_ppuStatus;
		PpuStatusFlag m_ppuStatusFlags;
	};

	uint8_t m_sprRam[256]; // Sprite RAM

	uint16_t m_cpuPpuAddr = 0;
	uint16_t m_cpuOamAddr = 0;
	uint8_t m_ppuAddressWriteParity = 0; // 0 = first write, 1 = second write

	uint8_t m_horizontalScrollOffset = 0;
	uint8_t m_verticalScrollOffset = 0;
	uint8_t m_scrollWriteParity = 0; // 0 = first write (horizontal), 1 = second write (vertical)

	uint32_t m_cycleCount = 0;
	int m_scanline = 241;

	const uint8_t* m_chrRom;

	bool m_shouldRender = false;
	RenderOptions m_renderOptions;
	PPU::ppuDisplayBuffer_t m_screenPixels;

	CPU::Cpu6502& m_cpu;
	NES::IMapper* m_pMapper;
};

} // namespace Ppu

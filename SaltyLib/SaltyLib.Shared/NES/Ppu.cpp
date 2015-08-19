#include "pch.h"
#include "Ppu.h"
#include "NESRom.h"
#include "Cpu6502.h"
#include "IMapper.h"

namespace PPU
{

const uint16_t c_paletteBkgOffset = 0x3F00;
const uint16_t c_paletteSprOffset = 0x3F10;

// Color table mapping the NES's color table to RGB values
static const DWORD c_nesRgbColorTable[64] = {
	0x808080, 0x003DA6, 0x0012B0, 0x440096,
	0xA1005E, 0xC70028, 0xBA0600, 0x8C1700,
	0x5C2F00, 0x104500, 0x054A00, 0x00472E,
	0x004166, 0x000000, 0x050505, 0x050505,
	0xC7C7C7, 0x0077FF, 0x2155FF, 0x8237FA,
	0xEB2FB5, 0xFF2950, 0xFF2200, 0xD63200,
	0xC46200, 0x358000, 0x058F00, 0x008A55,
	0x0099CC, 0x212121, 0x090909, 0x090909,
	0xFFFFFF, 0x0FD7FF, 0x69A2FF, 0xD480FF,
	0xFF45F3, 0xFF618B, 0xFF8833, 0xFF9C12,
	0xFABC20, 0x9FE30E, 0x2BF035, 0x0CF0A4,
	0x05FBFF, 0x5E5E5E, 0x0D0D0D, 0x0D0D0D,
	0xFFFFFF, 0xA6FCFF, 0xB3ECFF, 0xDAABEB,
	0xFFA8F9, 0xFFABB3, 0xFFD2B0, 0xFFEFA6,
	0xFFF79C, 0xD7E895, 0xA6EDAF, 0xA2F2DA,
	0x99FFFC, 0xDDDDDD, 0x111111, 0x111111,
};


Ppu::Ppu(CPU::Cpu6502& cpu)
	: m_cpu(cpu)
{
}


void Ppu::MapRomMemory(const NES::NESRom& rom, NES::IMapper* pMapper)
{
	m_pMapper = pMapper;
	m_mirroringMode = rom.GetMirroringMode();
}


uint8_t Ppu::ReadPpuStatus()
{
	// Clear v-blank when ppu status is checked.
	uint8_t currentStatus = m_ppuStatus;
	m_ppuStatus &= (~static_cast<uint8_t>(PpuStatusFlag::InVBlank));

	// Additionally clear our the cpu's ppu address register
	m_ppuAddressWriteParity = 0;
	m_cpuPpuAddr = 0x0000;

	m_scrollWriteParity = 0;

	return currentStatus;
}

void Ppu::WriteCpuAddressRegister(uint8_t value)
{
	if (m_ppuAddressWriteParity == 0)
	{
		m_cpuPpuAddr = (value << 8) | (m_cpuPpuAddr & 0x00FF);
	}
	else
	{
		m_cpuPpuAddr = (m_cpuPpuAddr & 0xFF00) | (value);
	}
	m_ppuAddressWriteParity = !m_ppuAddressWriteParity;
}

void Ppu::WriteScrollRegister(uint8_t value)
{
	if (m_scrollWriteParity == 0)
	{
		m_horizontalScrollOffset = value;
	}
	else
	{
		m_verticalScrollOffset = value;
	}
	m_scrollWriteParity = !m_scrollWriteParity;
}


uint16_t Ppu::CpuDataIncrementAmount() const
{
	if (m_ppuCtrlFlags.vramAddressIncrement)
		return 32;
	else
		return 1;
}


void Ppu::WriteCpuDataRegister(uint8_t value)
{
	WriteMemory8(m_cpuPpuAddr, value);

	m_cpuPpuAddr += CpuDataIncrementAmount(); // Auto-increment the ppu address register as reads/writes occur
}

uint8_t Ppu::ReadCpuDataRegister()
{
	return ReadMemory8(m_cpuPpuAddr);
}

void Ppu::WriteOamAddress(uint8_t value)
{
	m_cpuOamAddr = value;
}

void Ppu::WriteOamData(uint8_t value)
{
	m_sprRam[m_cpuOamAddr++] = value;
}

uint8_t Ppu::ReadOamData() const
{
	return m_sprRam[m_cpuOamAddr];
}

void Ppu::TriggerOamDMA(uint8_t* pData)
{
	// REVIEW: account for cycles taken: (513 or 514)
	memcpy_s(m_sprRam, 256, pData, 256);
}


uint8_t Ppu::ReadMemory8(uint16_t offset)
{
	return m_pMapper->ReadChrAddress(offset);
}

void Ppu::WriteMemory8(uint16_t offset, uint8_t value)
{
	m_pMapper->WriteChrAddress(offset, value);
}


void Ppu::WriteControlRegister1(uint8_t value)
{
	m_ppuCtrl1 = value;

	UpdateStatusWithLastWrittenRegister(value);
}

void Ppu::UpdateStatusWithLastWrittenRegister(uint8_t value)
{
	// The lower 5 bits of the status flag are unused, so will reflect that bits last written to the PPU over the internal data bus
	m_ppuStatus = (value & 0x1F) || (m_ppuStatus & 0xE0);
}


void Ppu::WriteControlRegister2(uint8_t value)
{
	m_ppuCtrl2 = value;
}


void Ppu::DoStuff()
{
	// We are going to do approximately ~3 cycles worth of work per 'DoStuff', which is 9 pixels of processing
	m_pixel += 9;
	if (m_pixel >= c_pixelsPerScanlines)
	{
		m_pixel %= c_pixelsPerScanlines;
		m_scanline++;

		if (m_scanline >= c_totalScanlines)
		{
			m_scanline %= c_totalScanlines;
			if (m_scanline == 0)
			{
				m_ppuStatus |= static_cast<uint8_t>(PpuStatusFlag::InVBlank);
				m_shouldRender = true;
				if (m_ppuCtrlFlags.nmiFlag == 1)
				{
					m_cpu.GenerateNonMaskableInterrupt();
				}
			}
		}
	}
}

bool Ppu::ShouldRender()
{
	bool shouldRender = m_shouldRender;
	if (shouldRender)
		m_shouldRender = false;
	return shouldRender;
}

uint8_t GetHighOrderColorFromAttributeEntry(uint8_t attributeData, int iRow, int iColumn)
{
	auto bitOffset = ((iRow & 2) << 1) | (iColumn & 2);
	return ((attributeData >> bitOffset) & 0x3) << 2;
}

uint16_t Ppu::GetBaseNametableOffset() const
{
	uint16_t offset = 0x2000 + (m_ppuCtrlFlags.nametableBaseAddress * 0x0400);

	if (offset > 0x2000)
		std::runtime_error("hrm");

	return offset;
}


uint16_t Ppu::GetSpriteNametableOffset() const
{
	return m_ppuCtrlFlags.spritePatternTableAddress == 0 ? 0x0000 : 0x1000;

}

void Ppu::DrawBkgTile(uint8_t tileNumber, uint8_t highOrderPixelData, int iRow, int iColumn, uint16_t patternTableOffset, ppuDisplayBuffer_t displayBuffer, ppuPixelOutputTypeBuffer_t outputTypeBuffer)
{
	const int tileOffsetBase = patternTableOffset + (tileNumber << 4);

	for (int iPixelRow = 0; iPixelRow != 8; ++iPixelRow)
	{
		if (iRow + iPixelRow >= c_displayHeight)
			break;

		for (int iPixelColumn = 0; iPixelColumn != 8; ++iPixelColumn)
		{
			const uint8_t colorByte1 = ReadMemory8(tileOffsetBase + iPixelRow);
			const uint8_t colorByte2 = ReadMemory8(tileOffsetBase + iPixelRow + 8);
			const uint8_t lowOrderColorBytes = ((colorByte1 & (1 << (7-iPixelColumn))) >> (7-iPixelColumn))
											 + ((colorByte2 & (1 << (7-iPixelColumn))) >> (7-iPixelColumn) << 1);
		
			const uint8_t fullPixelBytes = lowOrderColorBytes | highOrderPixelData;
			const uint8_t colorDataOffset = ReadMemory8(c_paletteBkgOffset + fullPixelBytes);

			displayBuffer[iRow + iPixelRow][iColumn + iPixelColumn] = c_nesRgbColorTable[colorDataOffset];
			
			if (lowOrderColorBytes != 0)
				outputTypeBuffer[iRow + iPixelRow][iColumn + iPixelColumn] = PixelOutputType::Background;
		}
	}
}


void Ppu::DrawSprTile(uint8_t tileNumber, uint8_t highOrderPixelData, int iRow, int iColumn, bool foregroundSprite, bool flipHorizontally, bool flipVertically, uint16_t patternTableOffset, ppuDisplayBuffer_t displayBuffer, ppuPixelOutputTypeBuffer_t outputTypeBuffer)
{
	const int tileOffsetBase = patternTableOffset + (tileNumber << 4);

	for (int iPixelRow = 0; iPixelRow != 8; ++iPixelRow)
	{
		if (iRow + iPixelRow >= c_displayHeight)
			break;

		for (int iPixelColumn = 0; iPixelColumn != 8; ++iPixelColumn)
		{
			const uint8_t colorByte1 = ReadMemory8(tileOffsetBase + iPixelRow);
			const uint8_t colorByte2 = ReadMemory8(tileOffsetBase + iPixelRow + 8);
			const uint8_t lowOrderColorBytes = ((colorByte1 & (1 << (7-iPixelColumn))) >> (7-iPixelColumn))
											 + ((colorByte2 & (1 << (7-iPixelColumn))) >> (7-iPixelColumn) << 1);
		
			const uint8_t fullPixelBytes = lowOrderColorBytes | highOrderPixelData;
			const uint8_t colorDataOffset = ReadMemory8(c_paletteSprOffset + fullPixelBytes);

			const int iPixelRowOffset = flipVertically ? (8 - iPixelRow) : iPixelRow;
			const int iPixelColumnOffset = flipHorizontally ? (8 - iPixelColumn) : iPixelColumn;

			// TODO: Need to emulate the sprite priority 'bug':  http://wiki.nesdev.com/w/index.php/PPU_sprite_priority

			if (lowOrderColorBytes != 0 
				&& ((outputTypeBuffer[iRow + iPixelRowOffset][iColumn + iPixelColumnOffset] == PixelOutputType::None)
				     || (foregroundSprite && (outputTypeBuffer[iRow + iPixelRowOffset][iColumn + iPixelColumnOffset] == PixelOutputType::Background))))
			{
				displayBuffer[iRow + iPixelRowOffset][iColumn + iPixelColumnOffset] = c_nesRgbColorTable[colorDataOffset];
				outputTypeBuffer[iRow + iPixelRowOffset][iColumn + iPixelColumnOffset] = PixelOutputType::Sprite;
			}
		}
	}
}



void Ppu::RenderToBuffer(ppuDisplayBuffer_t displayBuffer)
{
	const uint16_t nameTableOffset = GetBaseNametableOffset();
	const uint16_t patternTableOffset = GetPatternTableOffset();

	const int c_rows = 30;
	const int c_columnsPerRow = 32;

	ppuPixelOutputTypeBuffer_t pixelOutputTypeBuffer;
	memset(pixelOutputTypeBuffer, 0, sizeof(pixelOutputTypeBuffer));

	// Render background tiles
	for (int iRow = 0; iRow != c_rows; ++iRow)
	{
		for (int iColumn = 0; iColumn != c_columnsPerRow; ++iColumn)
		{
			const uint8_t tileNumber = ReadMemory8(nameTableOffset + iRow * c_columnsPerRow + iColumn);
			const uint8_t attributeIndex = static_cast<uint8_t>((iRow / 4) * 8 + (iColumn / 4));
			const uint8_t attributeData = ReadMemory8(nameTableOffset + (c_rows * c_columnsPerRow) + attributeIndex);
			const uint8_t highOrderColorBits = GetHighOrderColorFromAttributeEntry(attributeData, iRow, iColumn);

			DrawBkgTile(tileNumber, highOrderColorBits, iRow * 8, iColumn * 8, patternTableOffset, displayBuffer, pixelOutputTypeBuffer);
		}
	}

	const uint16_t spriteNameTableOffset = GetSpriteNametableOffset();
	const int c_bytesPerSprite = 4;
	for (int iSprite = 0; iSprite != 64; ++iSprite)
	{
		const size_t spriteByteOffset = iSprite * c_bytesPerSprite;
		uint8_t spriteY = m_sprRam[spriteByteOffset + 0];
		if (spriteY >= PPU::c_displayHeight - 2)
			continue;

		uint8_t spriteX = m_sprRam[spriteByteOffset + 3];
		uint8_t tileNumber = m_sprRam[spriteByteOffset + 1];

		const uint8_t thirdByte = m_sprRam[spriteByteOffset + 2];
		const uint8_t highOrderColorBits = (thirdByte & 0x3) << 2;
		const bool isForegroundSprite = (thirdByte & 0x20) == 0;
		const bool flipHorizontally = (thirdByte & 0x40) != 0;
		const bool flipVertically = (thirdByte & 0x80) != 0;

		DrawSprTile(tileNumber, highOrderColorBits, spriteY, spriteX, isForegroundSprite, flipHorizontally, flipVertically, spriteNameTableOffset, displayBuffer, pixelOutputTypeBuffer);
	}
}

} // namespace PPU


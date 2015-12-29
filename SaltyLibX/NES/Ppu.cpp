#include "stdafx.h"

#include "Ppu.h"
#include "NESRom.h"
#include "Cpu6502.h"
#include "IMapper.h"

#include <algorithm>

namespace PPU
{

const int c_tileSize = 8;

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

const DWORD c_nesColorYellow = 0xFFFF00;
const DWORD c_nesColorGreen = 0x00FF00;
const DWORD c_nesColorGray = 0x808080;
const DWORD c_nesColorRed = 0xFF0000;

static void DrawRectangle(ppuDisplayBuffer_t displayBuffer, DWORD nesColor, int left, int right, int top, int bottom)
{
	// Draw top/bottom line
	for (int iPixelColumn = std::max(0, left); iPixelColumn != right && iPixelColumn < c_displayWidth; ++iPixelColumn)
	{
		if (top >= 0 && top < c_displayHeight)
			displayBuffer[top][iPixelColumn] = nesColor;

		if (top >= 0 && bottom < c_displayHeight)
			displayBuffer[bottom][iPixelColumn] = nesColor;
	}

	// Draw left/right line
	for (int iPixelRow = std::max(0, top); iPixelRow != bottom && iPixelRow < c_displayHeight; ++iPixelRow)
	{
		if (left >= 0 && left < c_displayWidth)
			displayBuffer[iPixelRow][left] = nesColor;

		if (right < c_displayWidth)
			displayBuffer[iPixelRow][right] = nesColor;
	}
}

Ppu::Ppu(CPU::Cpu6502& cpu)
	: m_cpu(cpu)
{
}


void Ppu::SetRomMapper(NES::IMapper* pMapper)
{
	m_pMapper = pMapper;
}

void Ppu::SetRenderOptions(const RenderOptions& renderOptions)
{
	m_renderOptions = renderOptions;
}

uint8_t Ppu::ReadPpuStatus()
{
	// Clear v-blank when ppu status is checked.
	uint8_t currentStatus = m_ppuStatus;
	m_ppuStatusFlags.InVBlank = false;

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

	if (m_cpuOamAddr >= 256)
		m_cpuOamAddr = 0;
}

uint8_t Ppu::ReadOamData() const
{
	return m_sprRam[m_cpuOamAddr];
}

void Ppu::TriggerOamDMA(uint8_t* pData)
{
	// REVIEW: account for cycles taken: (513 or 514)

	if (m_cpuOamAddr == 0)
	{
		memcpy_s(m_sprRam, 256, pData, 256);
	}
	else
	{
		// OAM DMA technically starts at the current m_cpuOamAddr value, which is usually zero, but can be 
		// arbitrary, so we need to support this, but since it's slower, we make the common case 'fast'.
		// (probably not worth optimizing for infrequent 256 byte writes)
		for (uint32_t offset = 0; offset != 256; ++offset)
		{
			m_sprRam[(m_cpuOamAddr + offset) % 256] = pData[offset];
		}
	}
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


void Ppu::AddCycles(uint32_t cpuCycles)
{
	const int c_VBlankScanline = 241;
	const int c_minScanline = -1;
	const int c_maxScanline = 260;
	const int c_cyclesPerScanlines = 341;

	m_cycleCount += cpuCycles * 3;
	if (m_cycleCount >= c_cyclesPerScanlines)
	{
		m_cycleCount -= c_cyclesPerScanlines;
		m_scanline++;

		if (m_scanline == c_VBlankScanline)
		{
			m_shouldRender = true;

			m_ppuStatusFlags.InVBlank = true;
			if (m_ppuCtrlFlags.nmiFlag == 1)
			{
				m_cpu.GenerateNonMaskableInterrupt();
			}
		}
		else if (m_scanline > c_maxScanline)
		{
			m_scanline = -1;
			m_ppuStatusFlags.InVBlank = false;
			m_ppuStatusFlags.SpriteZeroHit = false;
			//m_ppuStatusFlags.SPRITEOVERFLOW = false;
		}

		if (m_scanline >= 0 && m_scanline < 240)
		{
			RenderScanline(m_scanline);
		}
	}
}

uint32_t Ppu::GetCycles() const
{
	return m_cycleCount;
}

uint32_t Ppu::GetScanline() const
{
	return m_scanline;
}


bool Ppu::ShouldRender()
{
	bool shouldRender = m_shouldRender;
	if (shouldRender)
		m_shouldRender = false;
	return shouldRender;
}

const ppuDisplayBuffer_t& Ppu::GetDisplayBuffer() const
{
	return m_screenPixels;
}



uint8_t GetHighOrderColorFromAttributeEntry(uint8_t attributeData, int iRow, int iColumn)
{
	auto bitOffset = ((iRow & 2) << 1) | (iColumn & 2);
	return ((attributeData >> bitOffset) & 0x3) << 2;
}


uint16_t Ppu::GetBaseNametableOffset() const
{
	uint16_t offset = 0x2000 + (m_ppuCtrlFlags.nametableBaseAddress * 0x0400);

	if (offset > 0x3000)
		std::runtime_error("hrm");

	return offset;
}


uint16_t Ppu::GetSpriteNametableOffset() const
{
	return m_ppuCtrlFlags.spritePatternTableAddress == 0 ? 0x0000 : 0x1000;
}


void Ppu::DrawBkgTile(uint8_t tileNumber, uint8_t highOrderPixelData, int iRow, int iColumn, uint16_t patternTableOffset, ppuDisplayBuffer_t displayBuffer, ppuPixelOutputTypeBuffer_t outputTypeBuffer)
{
	const uint16_t tileOffsetBase = patternTableOffset + (tileNumber << 4);

	for (uint16_t iPixelRow = 0; iPixelRow != 8; ++iPixelRow)
	{
		if (iRow + iPixelRow < 0)
			continue;

		if (iRow + iPixelRow >= c_displayHeight)
			break;

		DrawBkgTile(tileNumber, highOrderPixelData, iRow, iColumn, iPixelRow, patternTableOffset, displayBuffer, outputTypeBuffer);
	}
}

void Ppu::DrawBkgTile(uint8_t tileNumber, uint8_t highOrderPixelData, int iRow, int iColumn, int iPixelRow, uint16_t patternTableOffset, ppuDisplayBuffer_t displayBuffer, ppuPixelOutputTypeBuffer_t outputTypeBuffer)
{
	const uint16_t tileOffsetBase = patternTableOffset + (tileNumber << 4);

	if (iRow + iPixelRow < 0)
		return;

	if (iRow + iPixelRow >= c_displayHeight)
		return;

	for (uint16_t iPixelColumn = 0; iPixelColumn != 8; ++iPixelColumn)
	{
		if (iColumn + iPixelColumn < 0)
			continue;

		if (iColumn + iPixelColumn >= c_displayWidth)
			break;

		const uint8_t colorByte1 = ReadMemory8(tileOffsetBase + iPixelRow);
		const uint8_t colorByte2 = ReadMemory8(tileOffsetBase + iPixelRow + 8);
		const uint8_t lowOrderColorBytes = ((colorByte1 & (1 << (7-iPixelColumn))) >> (7-iPixelColumn))
										 + ((colorByte2 & (1 << (7-iPixelColumn))) >> (7-iPixelColumn) << 1);
	
		// All background colors should map to 3F00
		const uint8_t fullPixelBytes = (lowOrderColorBytes == 0) ? 0 : (lowOrderColorBytes | highOrderPixelData);

		const uint8_t colorDataOffset = ReadMemory8(c_paletteBkgOffset + fullPixelBytes);

		displayBuffer[iRow + iPixelRow][iColumn + iPixelColumn] = c_nesRgbColorTable[colorDataOffset];
		
		if (lowOrderColorBytes != 0)
			outputTypeBuffer[iRow + iPixelRow][iColumn + iPixelColumn] = PixelOutputType::Background;
	}
}



// Handle the pattern table access logic differences between 8x8 sprites and 8x16 sprites
uint16_t Ppu::GetSpriteTileOffset(uint8_t tileNumber, bool is8x8Sprite) const
{
	if (is8x8Sprite)
	{
		return GetSpriteNametableOffset() + (tileNumber << 4);  // Each sprite gets 16 bits of data
	}
	else
	{
		const uint16_t patternTable = ((tileNumber % 2) == 0) ? 0x0000 : 0x1000;
		return patternTable + ((tileNumber/2) << 5);  // Each sprite gets 32 bits of data
	}
}


bool Ppu::DrawSprTile(uint8_t tileNumber, uint8_t highOrderPixelData, int iRow, int iColumn, int iPixelRow, bool foregroundSprite, bool flipHorizontally, bool flipVertically, ppuDisplayBuffer_t displayBuffer, ppuPixelOutputTypeBuffer_t outputTypeBuffer)
{
	const uint16_t tileOffsetBase = GetSpriteTileOffset(tileNumber, m_ppuCtrlFlags.spriteSize == SpriteSize::Size8x8);
	const int totalPixelRows = (m_ppuCtrlFlags.spriteSize == SpriteSize::Size8x16) ? 16 : 8;

	const uint16_t totalTiles = (m_ppuCtrlFlags.spriteSize == SpriteSize::Size8x16) ? 2 : 1;
	uint16_t iTile = iPixelRow / 8;
	uint16_t iTileRow = iPixelRow % 8;

	const int iPixelRowOffset = flipVertically ? (totalPixelRows - iPixelRow - 1) : (iPixelRow);
	if (iRow + iPixelRowOffset >= c_displayHeight)
		return false;

	bool spriteHit = false;
	for (uint16_t iPixelColumn = 0; iPixelColumn != 8; ++iPixelColumn)
	{
		const int iPixelColumnOffset = flipHorizontally ? (7 - iPixelColumn) : iPixelColumn;
		if (iColumn + iPixelColumnOffset >= c_displayWidth)
			continue;

		const uint16_t c_bytesPerTile = 16;
		const uint8_t colorByte1 = ReadMemory8(tileOffsetBase + (iTile * c_bytesPerTile) + iTileRow);
		const uint8_t colorByte2 = ReadMemory8(tileOffsetBase + (iTile * c_bytesPerTile) + iTileRow + 8);
		const uint8_t lowOrderColorBytes = ((colorByte1 & (1 << (7 - iPixelColumn))) >> (7 - iPixelColumn))
			+ ((colorByte2 & (1 << (7 - iPixelColumn))) >> (7 - iPixelColumn) << 1);

		const uint8_t fullPixelBytes = lowOrderColorBytes | highOrderPixelData;
		const uint8_t colorDataOffset = ReadMemory8(c_paletteSprOffset + fullPixelBytes);

		// TODO: Need to emulate the sprite priority 'bug':  http://wiki.nesdev.com/w/index.php/PPU_sprite_priority
		if (lowOrderColorBytes != 0 && (outputTypeBuffer[iRow + iPixelRowOffset][iColumn + iPixelColumnOffset] == PixelOutputType::Background))
		{
			spriteHit = true;
		}

		if (lowOrderColorBytes != 0
			&& ((outputTypeBuffer[iRow + iPixelRowOffset][iColumn + iPixelColumnOffset] == PixelOutputType::None)
				|| (foregroundSprite && (outputTypeBuffer[iRow + iPixelRowOffset][iColumn + iPixelColumnOffset] == PixelOutputType::Background))))
		{
			displayBuffer[iRow + iPixelRowOffset][iColumn + iPixelColumnOffset] = c_nesRgbColorTable[colorDataOffset];
			outputTypeBuffer[iRow + iPixelRowOffset][iColumn + iPixelColumnOffset] = PixelOutputType::Sprite;
		}
	}

	return spriteHit;
}

bool Ppu::DrawSprTile(uint8_t tileNumber, uint8_t highOrderPixelData, int iRow, int iColumn, bool foregroundSprite, bool flipHorizontally, bool flipVertically, ppuDisplayBuffer_t displayBuffer, ppuPixelOutputTypeBuffer_t outputTypeBuffer)
{
	bool spriteHit = false;
	const uint16_t tileOffsetBase = GetSpriteTileOffset(tileNumber, m_ppuCtrlFlags.spriteSize == SpriteSize::Size8x8);
	const int totalPixelRows = (m_ppuCtrlFlags.spriteSize == SpriteSize::Size8x16) ? 16 : 8;

	const uint16_t totalTiles = (m_ppuCtrlFlags.spriteSize == SpriteSize::Size8x16) ? 2 : 1;
	for (uint16_t iTile = 0; iTile != totalTiles; ++iTile)
	{
		for (uint16_t iPixelRow = 0; iPixelRow != 8; ++iPixelRow)
		{
			const int iPixelRowOffset = flipVertically ? (totalPixelRows - iPixelRow - iTile * 8 - 1) : (iPixelRow + iTile * 8);
			if (iRow + iPixelRowOffset >= c_displayHeight)
				continue;

			for (uint16_t iPixelColumn = 0; iPixelColumn != 8; ++iPixelColumn)
			{
				const int iPixelColumnOffset = flipHorizontally ? (7 - iPixelColumn) : iPixelColumn;
				if (iColumn + iPixelColumnOffset >= c_displayWidth)
					continue;

				const uint16_t c_bytesPerTile = 16;
				const uint8_t colorByte1 = ReadMemory8(tileOffsetBase + (iTile * c_bytesPerTile) + iPixelRow);
				const uint8_t colorByte2 = ReadMemory8(tileOffsetBase + (iTile * c_bytesPerTile) + iPixelRow + 8);
				const uint8_t lowOrderColorBytes = ((colorByte1 & (1 << (7-iPixelColumn))) >> (7-iPixelColumn))
												 + ((colorByte2 & (1 << (7-iPixelColumn))) >> (7-iPixelColumn) << 1);
			
				const uint8_t fullPixelBytes = lowOrderColorBytes | highOrderPixelData;
				const uint8_t colorDataOffset = ReadMemory8(c_paletteSprOffset + fullPixelBytes);

				// TODO: Need to emulate the sprite priority 'bug':  http://wiki.nesdev.com/w/index.php/PPU_sprite_priority
				if (lowOrderColorBytes != 0 && (outputTypeBuffer[iRow + iPixelRowOffset][iColumn + iPixelColumnOffset] == PixelOutputType::Background))
				{
					spriteHit = true;
				}

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

	return spriteHit;
}

void Ppu::RenderToBuffer(const RenderOptions& options)
{
	RenderToBuffer(m_screenPixels, options);
}

void Ppu::RenderToBuffer(ppuDisplayBuffer_t displayBuffer, const RenderOptions& options)
{
	const uint16_t nameTableOffset = GetBaseNametableOffset();
	const uint16_t patternTableOffset = GetPatternTableOffset();

	const int c_rows = 30;
	const int c_columnsPerRow = 32;

	ppuPixelOutputTypeBuffer_t pixelOutputTypeBuffer;
	memset(pixelOutputTypeBuffer, 0, sizeof(pixelOutputTypeBuffer));

	const int iRowPixelOffset = -(m_verticalScrollOffset % c_tileSize);
	const int iColPixelOffset = -(m_horizontalScrollOffset % c_tileSize);

	// Clear the SpriteZero hit flag at the beginning of rendering
	m_ppuStatusFlags.SpriteZeroHit = false;

	if (m_ppuMaskFlags.showBackground)
	{
		// Render background tiles
		for (int iRow = 0; iRow != c_rows + 1; ++iRow)
		{
			const uint16_t iRowTile = (iRow + (m_verticalScrollOffset / c_tileSize)) % c_rows;
			const bool rowOverflow = (iRow + (m_verticalScrollOffset / c_tileSize)) > c_rows;

			for (int iColumn = 0; iColumn != c_columnsPerRow + 1; ++iColumn)
			{
				const uint16_t iColumnTile = (iColumn + (m_horizontalScrollOffset / c_tileSize)) % c_columnsPerRow;
				const bool columnOverflow = (iColumn + (m_horizontalScrollOffset / c_tileSize)) > c_columnsPerRow;
				const uint16_t xNametable = nameTableOffset + (columnOverflow ? 0x400 : 0) + (rowOverflow ? 0x800 : 0) & 0x2FFF;

				const uint8_t tileNumber = ReadMemory8(xNametable + iRowTile * c_columnsPerRow + iColumnTile);
				const uint8_t attributeIndex = static_cast<uint8_t>((iRowTile / 4) * 8 + (iColumnTile / 4));
				const uint8_t attributeData = ReadMemory8(xNametable + (c_rows * c_columnsPerRow) + attributeIndex);
				const uint8_t highOrderColorBits = GetHighOrderColorFromAttributeEntry(attributeData, iRowTile, iColumnTile);

				DrawBkgTile(tileNumber, highOrderColorBits, iRow * 8 + iRowPixelOffset, iColumn * 8 + iColPixelOffset, patternTableOffset, displayBuffer, pixelOutputTypeBuffer);

				if (options.fDrawBackgroundGrid)
					DrawRectangle(displayBuffer, c_nesColorGray, iColumn*8 + iColPixelOffset, (iColumn+1)*8 + iColPixelOffset,  iRow*8 + iRowPixelOffset, (iRow+1)*8 + iRowPixelOffset);
			}
		}
	}

	if (m_ppuMaskFlags.showSprites)
	{
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

			const bool spriteHit = DrawSprTile(tileNumber, highOrderColorBits, spriteY, spriteX, isForegroundSprite, flipHorizontally, flipVertically, displayBuffer, pixelOutputTypeBuffer);

			if (iSprite == 0 && spriteHit)
			{
				m_ppuStatusFlags.SpriteZeroHit = true;
			}

			if (options.fDrawSpriteOutline)
			{
				const int totalPixelRows = (m_ppuCtrlFlags.spriteSize == SpriteSize::Size8x16) ? 16 : 8;
				DrawRectangle(displayBuffer, c_nesColorRed, spriteX, spriteX + 8, spriteY, spriteY + totalPixelRows);
			}
		}
	}
}

void Ppu::RenderScanline(int scanline)
{
	const uint16_t nameTableOffset = GetBaseNametableOffset();
	const uint16_t patternTableOffset = GetPatternTableOffset();

	const int c_rows = 30;
	const int c_columnsPerRow = 32;

	for (uint32_t iColumn = 0; iColumn != c_displayWidth; ++iColumn)
	{
		m_screenPixelTypes[scanline][iColumn] = PixelOutputType::None;
	}

	const int iRowPixelOffset = -(m_verticalScrollOffset % c_tileSize);
	const int iColPixelOffset = -(m_horizontalScrollOffset % c_tileSize);

	if (m_ppuMaskFlags.showBackground)
	{
		const uint16_t iRowTile = ((scanline + m_verticalScrollOffset) / c_tileSize) % c_rows;
		const bool rowOverflow = ((scanline + m_verticalScrollOffset) / c_tileSize) >= c_rows;

		for (int iColumn = 0; iColumn != c_columnsPerRow + 1; ++iColumn)
		{
			const uint16_t iColumnTile = (iColumn + (m_horizontalScrollOffset / c_tileSize)) % c_columnsPerRow;
			const bool columnOverflow = (iColumn + (m_horizontalScrollOffset / c_tileSize)) >= c_columnsPerRow;
			const uint16_t xNametable = nameTableOffset + (columnOverflow ? 0x400 : 0) + (rowOverflow ? 0x800 : 0) & 0x2FFF;

			const uint8_t tileNumber = ReadMemory8(xNametable + iRowTile * c_columnsPerRow + iColumnTile);
			const uint8_t attributeIndex = static_cast<uint8_t>((iRowTile / 4) * 8 + (iColumnTile / 4));
			const uint8_t attributeData = ReadMemory8(xNametable + (c_rows * c_columnsPerRow) + attributeIndex);
			const uint8_t highOrderColorBits = GetHighOrderColorFromAttributeEntry(attributeData, iRowTile, iColumnTile);

			const int pixelRow = (scanline + m_verticalScrollOffset) % 8;

			DrawBkgTile(tileNumber, highOrderColorBits, scanline - pixelRow, iColumn * 8 + iColPixelOffset, pixelRow, patternTableOffset, m_screenPixels, m_screenPixelTypes);

			//if (m_renderOptions.fDrawBackgroundGrid)
			//	DrawRectangle(m_screenPixels, c_nesColorGray, iColumn*8 + iColPixelOffset, (iColumn+1)*8 + iColPixelOffset,  iRow*8 + iRowPixelOffset, (iRow+1)*8 + iRowPixelOffset);
		}
	}

	if (m_ppuMaskFlags.showSprites)
	{
		const int c_bytesPerSprite = 4;
		for (int iSprite = 0; iSprite != 64; ++iSprite)
		{
			const size_t spriteByteOffset = iSprite * c_bytesPerSprite;
			uint8_t spriteY = m_sprRam[spriteByteOffset + 0];
			if (spriteY >= PPU::c_displayHeight - 2)
				continue;

			const int totalPixelRows = (m_ppuCtrlFlags.spriteSize == SpriteSize::Size8x16) ? 16 : 8;
			if (spriteY > scanline || spriteY + totalPixelRows <= scanline)
				continue;

			uint8_t spriteX = m_sprRam[spriteByteOffset + 3];
			uint8_t tileNumber = m_sprRam[spriteByteOffset + 1];

			const uint8_t thirdByte = m_sprRam[spriteByteOffset + 2];
			const uint8_t highOrderColorBits = (thirdByte & 0x3) << 2;
			const bool isForegroundSprite = (thirdByte & 0x20) == 0;
			const bool flipHorizontally = (thirdByte & 0x40) != 0;
			const bool flipVertically = (thirdByte & 0x80) != 0;

			const bool spriteHit = DrawSprTile(tileNumber, highOrderColorBits, spriteY, spriteX, scanline - spriteY, isForegroundSprite, flipHorizontally, flipVertically, m_screenPixels, m_screenPixelTypes);

			if (iSprite == 0 && spriteHit)
			{
				m_ppuStatusFlags.SpriteZeroHit = true;
			}

			if (m_renderOptions.fDrawSpriteOutline)
			{
				DrawRectangle(m_screenPixels, c_nesColorRed, spriteX, spriteX + 8, spriteY, spriteY + totalPixelRows);
			}
		}
	}
}

} // namespace PPU


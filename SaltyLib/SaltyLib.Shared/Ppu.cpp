#include "pch.h"
#include "Ppu.h"
#include "NESRom.h"
#include "Cpu6502.h"

#include <Windows.h>

namespace PPU
{

Ppu::Ppu(CPU::Cpu6502& cpu)
	: m_cpu(cpu)
{
}


void Ppu::MapRomMemory(const NES::NESRom& rom)
{
	m_chrRom = rom.GetChrRom();

	// Copy pattern tables into vram
	memcpy_s(m_vram, _countof(m_vram), m_chrRom, rom.CbChrRomData());

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


//uint8_t Ppu::ReadCpuDataRegister()
//{
//	uint8_t result = ReadMemory8(m_cpuPpuAddr);
//	m_cpuPpuAddr++; // Auto-increment the ppu address register as reads/writes occur
//	return result;
//}

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

void Ppu::TriggerOamDMA(uint8_t* pData)
{
	// REVIEW: account for cycles taken: (513 or 514)
	memcpy_s(m_sprRam, 256, pData, 256);
}


uint8_t Ppu::ReadMemory8(uint16_t offset)
{
	uint16_t effectiveOffset = offset % c_cbVRAM;
	return m_vram[effectiveOffset];
}

void Ppu::WriteMemory8(uint16_t offset, uint8_t value)
{
	uint16_t effectiveOffset = offset % c_cbVRAM;
	m_vram[effectiveOffset] = value;
}


void Ppu::WriteControlRegister1(uint8_t value)
{
	m_ppuCtrl1 = value;
}

void Ppu::WriteControlRegister2(uint8_t value)
{
	m_ppuCtrl2 = value;
}


void Ppu::DoStuff()
{
	// We are going to do approximately ~3 cycles worth of work per 'DoStuff', which is 9 pixels of processing
	m_pixel += 12;
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

uint8_t CombinePixelData(uint8_t baseData, uint8_t attributeData, int iRow, int iColumn)
{
	auto bitOffset = ((iRow & 2) << 1) | (iColumn & 2);
	return baseData | ((attributeData >> bitOffset) & 0x3) << 2;
}

uint16_t Ppu::GetBaseNametableOffset() const
{
	uint16_t offset = 0x2000 + (m_ppuCtrl1 & 0x03) * 0x0400;

	if (offset > 0x2000)
		std::runtime_error("hrm");

	return offset;
}

void Ppu::RenderToBuffer(ppuDisplayBuffer_t displayBuffer)
{
	const uint16_t nameTableOffset = GetBaseNametableOffset();
	const uint16_t patternTableOffset = GetPatternTableOffset();

	const int c_rows = 30;
	const int c_columnsPerRow = 32;

	static const DWORD c_colorTable[64] = {
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

	for (int iRow = 0; iRow != c_rows; ++iRow)
	{
		for (int iColumn = 0; iColumn != c_columnsPerRow; ++iColumn)
		{
			uint8_t tileNumber = m_vram[nameTableOffset + iRow * c_columnsPerRow + iColumn];

			const int tileOffsetBase = patternTableOffset + (tileNumber << 4);

			for (int iPixelRow = 0; iPixelRow != 8; ++iPixelRow)
			{
				for (int iPixelColumn = 0; iPixelColumn != 8; ++iPixelColumn)
				{
					const uint8_t colorByte1 = m_vram[tileOffsetBase + iPixelRow];
					const uint8_t colorByte2 = m_vram[tileOffsetBase + iPixelRow + 8];
					const int colorIndex = ((colorByte1 & (1 << (7-iPixelColumn))) >> (7-iPixelColumn))
					                       + ((colorByte2 & (1 << (7-iPixelColumn))) >> (7-iPixelColumn) << 1);
				
					const uint8_t attributeIndex = static_cast<uint8_t>((iRow / 4) * 8 + (iColumn / 4));
					const uint8_t attributeData = m_vram[nameTableOffset + (c_rows * c_columnsPerRow) + attributeIndex];
					const uint8_t fullPixelData = CombinePixelData(colorIndex, attributeData, iRow, iColumn);

					const uint8_t colorDataOffset = m_vram[0x3F00 + fullPixelData];

					displayBuffer[iRow * 8 + iPixelRow][iColumn * 8 + iPixelColumn] = c_colorTable[colorDataOffset];
				}
			}
		}
	}
}



} // namespace PPU


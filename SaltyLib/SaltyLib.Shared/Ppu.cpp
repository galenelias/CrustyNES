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


uint8_t Ppu::ReadCpuDataRegister()
{
	uint8_t result = ReadMemory8(m_cpuPpuAddr);
	m_cpuPpuAddr++; // Auto-increment the ppu address register as reads/writes occur
	return result;
}


void Ppu::WriteCpuDataRegister(uint8_t value)
{
	WriteMemory8(m_cpuPpuAddr, value);
	m_cpuPpuAddr++; // Auto-increment the ppu address register as reads/writes occur
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

//void Ppu::RenderWin32(HDC hdc, const RECT /*bounds*/)
void Ppu::RenderToBuffer(ppuDisplayBuffer_t displayBuffer)
{
	const uint16_t nameTableOffset = GetBaseNametableOffset();
	const uint16_t patternTableOffset = GetPatternTableOffset();

	const int c_rows = 30;
	const int c_columnsPerRow = 32;

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
				
					COLORREF color = PPU_RGB(255,0,255);
					switch (colorIndex)
					{
					case 0:
						color = PPU_RGB(0,0,0);
						break;
					case 1:
						color = PPU_RGB(255,0,0);
						break;
					case 2:
						color = PPU_RGB(0,255,0);
						break;
					case 3:
						color = PPU_RGB(0,0,255);
						break;
					}

					displayBuffer[iRow * 8 + iPixelRow][iColumn * 8 + iPixelColumn] = color;
				}
			}
		}
	}
}



} // namespace PPU


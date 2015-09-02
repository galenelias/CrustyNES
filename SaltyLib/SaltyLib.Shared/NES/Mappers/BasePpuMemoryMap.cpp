#include "pch.h"

#include "BasePpuMemoryMap.h"
#include "../NESRom.h"

#include <stdexcept>

namespace NES
{

void BasePpuMemoryMap::LoadRomData(const NESRom& rom)
{
	m_mirroringMode = rom.GetMirroringMode();

}

uint16_t BasePpuMemoryMap::MapPpuAddress(uint16_t address) const
{
	if (address >= 0x4000)
		address &= 0x3FFF;

	if (address < 0x2000)
		throw std::runtime_error("Unexpected BasePpuMemoryMap address");

	if (address >= 0x3000 && address < 0x3F00)
		throw std::runtime_error("Unsupported mirrored address (NYI)");

	if (address >= 0x2000 && address < 0x3000)
	{
		int ramSlot;
		const int logicalTile = (address - 0x2000) / 0x400;
		if (m_mirroringMode == PPU::MirroringMode::HorizontalMirroring)
		{
			ramSlot = logicalTile / 2; // 0,1 -> 0;  2,3 -> 1
		}
		else if (m_mirroringMode == PPU::MirroringMode::VerticalMirroring)
		{
			ramSlot = logicalTile % 2; // 0,2 -> 0;  1,3 -> 1
		}
		else
		{
			throw std::runtime_error("Unsupported ppu mirroring mode");
		}
		return static_cast<uint16_t>(0x2000 + ramSlot * 1024 + (address & 0x03FF));
	}
	else
	{
		return address;
	}
}

uint8_t BasePpuMemoryMap::ReadMemory(uint16_t address) const
{
	uint16_t ramOffset = MapPpuAddress(address);
	if (ramOffset >= 0x3F20)
		throw std::runtime_error("Uh oh, unexpected mapping");
	else if (ramOffset >= 0x3F00)
		return m_paletteRam[ramOffset - 0x3F00];
	else
		return m_ciram[ramOffset - 0x2000];
}

void BasePpuMemoryMap::WriteMemory(uint16_t address, uint8_t value)
{
	uint16_t ramOffset = MapPpuAddress(address);
	if (ramOffset >= 0x3F20)
		throw std::runtime_error("Uh oh, unexpected mapping");
	else if (ramOffset >= 0x3F00)
		m_paletteRam[ramOffset - 0x3F00] = value;
	else
		m_ciram[ramOffset - 0x2000] = value;

}

}

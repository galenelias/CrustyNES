#pragma once

#include <stdint.h>
#include <array>

// Provides the basic support for accessing PPU addresses $2000-$4000
//  handling access to the CIRAM and the various nametable mirroring modes

namespace PPU
{
	enum class MirroringMode;
}

namespace NES
{
class NESRom;

class BasePpuMemoryMap
{
public:
	void LoadRomData(const NESRom& rom);

	uint8_t ReadMemory(uint16_t address) const;
	void WriteMemory(uint16_t address, uint8_t value);

private:
	uint16_t MapPpuAddress(uint16_t address) const;

	std::array<uint8_t,2 * 1024> m_ciram;
	std::array<uint8_t,32> m_paletteRam;
	
	PPU::MirroringMode m_mirroringMode;
};

}
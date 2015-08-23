#pragma once

#include <stdint.h>

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

	uint8_t m_ciram[2 * 1024];
	
	PPU::MirroringMode m_mirroringMode;
};

}
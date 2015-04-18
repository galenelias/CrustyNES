#include "pch.h"
#include "Ppu.h"

namespace PPU
{

Ppu::Ppu()
{
}


Ppu::~Ppu()
{
}

uint8_t Ppu::ReadMemory8(uint16_t offset)
{
	uint16_t effectiveOffset = offset % c_cbVRAM;
	return m_vram[effectiveOffset];
}



} // namespace PPU


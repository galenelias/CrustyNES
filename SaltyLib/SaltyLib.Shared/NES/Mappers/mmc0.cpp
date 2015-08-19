#include "pch.h"

#include "../IMapper.h"
#include "../NESRom.h"

#include <stdexcept>

namespace NES
{

class MMC0Mapper : public IMapper
{
public:
	virtual void LoadFromRom(const NESRom& rom) override
	{
		// Copy pattern tables into vram
		const byte* pChrRom = rom.GetChrRom();
		memcpy_s(m_vram, _countof(m_vram), pChrRom, rom.CbChrRomData());
	}

	virtual void WriteAddress(uint16_t address, uint8_t value) override
	{
		throw std::runtime_error("Unexpected mapper write");
	}

	virtual uint8_t ReadAddress(uint16_t address) override
	{
		return 0;
	}

	virtual void WriteChrAddress(uint16_t address, uint8_t value) override
	{
		uint16_t effectiveOffset = address % c_cbVRAM;
		m_vram[effectiveOffset] = value;
	}

	virtual uint8_t ReadChrAddress(uint16_t address) override
	{
		uint16_t effectiveOffset = address % c_cbVRAM;
		return m_vram[effectiveOffset];
	}


private:

	static const uint16_t c_cbVRAM = 16*1024; // 0x4000
	uint8_t m_vram[c_cbVRAM]; // 16KB of video ram

};

MapperPtr CreateMMC0Mapper()
{
	return std::make_unique<MMC0Mapper>();
}

}

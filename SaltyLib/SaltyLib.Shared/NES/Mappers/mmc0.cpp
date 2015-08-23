#include "pch.h"

#include "../IMapper.h"
#include "../NESRom.h"
#include "BasePpuMemoryMap.h"

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
		const uint32_t cbChrRom = rom.CbChrRomData();
		if (cbChrRom > 0x2000)
			throw std::runtime_error("Mapper0 doesn't support > 8K memory");

		memcpy_s(m_vrom, _countof(m_vrom), pChrRom, cbChrRom);

		m_cbPrgRom = rom.GetCbPrgRom();
		m_prgRom = rom.GetPrgRom();

	}

	virtual void WriteAddress(uint16_t address, uint8_t value) override
	{
		throw std::runtime_error("Unexpected mapper write");
	}

	virtual uint8_t ReadAddress(uint16_t offset) override
	{
		// For now, only support NROM mapper NES-NROM-128 and NES-NROM-256 (iNes Mapper 0)
		if (m_cbPrgRom == 32 * 1024)
		{
			return m_prgRom[offset - 0x8000];
		}
		else if (m_cbPrgRom == 16 * 1024)
		{
			if (offset >= 0xC000)
				return m_prgRom[offset - 0xC000];
			else
				return m_prgRom[offset - 0x8000];
		}
		else
		{
			throw std::runtime_error("Unexpected read location");
		}
	}

	virtual void WriteChrAddress(uint16_t address, uint8_t value) override
	{
		if (address < 0x2000)
			m_vrom[address] = value;
		else
			m_basePpuMemory.WriteMemory(address, value);
	}

	virtual uint8_t ReadChrAddress(uint16_t address) override
	{
		if (address < 0x2000)
			return m_vrom[address];
		else
			return m_basePpuMemory.ReadMemory(address);
	}


private:

	static const uint16_t c_cbVROM = 8*1024; // 0x2000
	uint8_t m_vrom[c_cbVROM]; // 16KB of video ram
	BasePpuMemoryMap m_basePpuMemory;

	const byte* m_prgRom;
	uint32_t m_cbPrgRom;
};

MapperPtr CreateMMC0Mapper()
{
	return std::make_unique<MMC0Mapper>();
}

}

#include "stdafx.h"

#include "../IMapper.h"
#include "../NESRom.h"
#include "BasePpuMemoryMap.h"
#include "BaseMapper.h"

#include <stdexcept>
#include <vector>

namespace NES
{

const uint32_t c_cbChrRomBank = (8 * 1024);

class CNROMMapper : public BaseMapper
{
public:
	CNROMMapper& operator=(const CNROMMapper& other) = delete;

	virtual void LoadFromRom(const NESRom& rom) override;
	virtual void WriteAddress(uint16_t address, uint8_t value) override;
	virtual uint8_t ReadAddress(uint16_t address) override;

	virtual void WriteChrAddress(uint16_t address, uint8_t value) override;
	virtual uint8_t ReadChrAddress(uint16_t address) override;

private:

	BasePpuMemoryMap m_basePpuMemory;

	const byte* m_chrRomActiveBank = nullptr;
	const byte* m_chrRomData = nullptr;
	uint32_t m_cbChrRomData = 0;

	const byte* m_prgRom;
	uint32_t m_cbPrgRom;

};


void CNROMMapper::LoadFromRom(const NESRom& rom)
{
	m_chrRomData = rom.GetChrRom();
	m_cbChrRomData = rom.CbChrRomData();

	m_cbPrgRom = rom.GetCbPrgRom();
	m_prgRom = rom.GetPrgRom();

	m_basePpuMemory.LoadRomData(rom);
}


void CNROMMapper::WriteAddress(uint16_t address, uint8_t value)
{
	if (address >= 0x8000)
	{
		m_chrRomActiveBank = m_chrRomData + c_cbChrRomBank * (value & 0x3);
	}
	else
	{
		// Hrm, why does anyone do this?  Maybe for Famicom Disk System integration?
		//throw std::runtime_error("Unexpected mapper address");
	}
}

uint8_t CNROMMapper::ReadAddress(uint16_t address)
{
	if (address >= 0x8000 && address <= 0xFFFF)
	{
		if (m_cbPrgRom == 32 * 1024)
		{
			return m_prgRom[address - 0x8000];
		}
		else if (m_cbPrgRom == 16 * 1024)
		{
			if (address >= 0xC000)
				return m_prgRom[address - 0xC000];
			else
				return m_prgRom[address - 0x8000];
		}
	}
	else
		throw std::runtime_error("Unexpected mapper address");
}


void CNROMMapper::WriteChrAddress(uint16_t address, uint8_t value)
{
	if (address >= 0x0000 && address < 0x2000)
		throw std::runtime_error("Unexpected mapper chr write");
	else
		m_basePpuMemory.WriteMemory(address, value);
}


uint8_t CNROMMapper::ReadChrAddress(uint16_t address)
{
	if (address >= 0x0000 && address < 0x2000)
		return m_chrRomActiveBank[address];
	else
		return m_basePpuMemory.ReadMemory(address);
}



MapperPtr CreateCNROMMapper()
{
	return std::make_unique<CNROMMapper>();
}


}

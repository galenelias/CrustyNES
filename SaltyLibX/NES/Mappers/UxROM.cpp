#include "stdafx.h"

#include "../IMapper.h"
#include "../NESRom.h"
#include "BasePpuMemoryMap.h"
#include "BaseMapper.h"

#include <stdexcept>

namespace NES
{

const uint32_t c_cb16RomBank = (16 * 1024);
const uint32_t c_cbChrRam = (8 * 1024);

class UxROM : public BaseMapper
{
public:
	virtual void LoadFromRom(const NESRom& rom) override;
	virtual void WriteAddress(uint16_t address, uint8_t value) override;
	virtual uint8_t ReadAddress(uint16_t address) override;

	virtual void WriteChrAddress(uint16_t address, uint8_t value) override;
	virtual uint8_t ReadChrAddress(uint16_t address) override;

private:
	const byte* m_prgRom;
	uint32_t m_cbPrgRom;

	const uint8_t* m_pBank1Rom = nullptr;
	const uint8_t* m_pBank2Rom = nullptr;

	uint8_t m_chrRAM[c_cbChrRam];

	BasePpuMemoryMap m_basePpuMemory;
};


void UxROM::LoadFromRom(const NESRom& rom)
{
	// Copy pattern tables into vram
	uint32_t cbVRAM = rom.CbChrRomData();
	
	const byte* pChrRom = rom.GetChrRom();
	memcpy_s(m_chrRAM, c_cbChrRam, pChrRom, cbVRAM);

	m_cbPrgRom = rom.GetCbPrgRom();
	m_prgRom = rom.GetPrgRom();

	m_pBank1Rom = m_prgRom;
	m_pBank2Rom = m_prgRom + (m_cbPrgRom - c_cb16RomBank);
}

void UxROM::WriteAddress(uint16_t address, uint8_t value)
{
	if (address >= 0x8000)
	{
		//uint8_t selectorValue1 = address & 0xF;
		//uint8_t selectorValue2 = address & 0x7;

		//if (selectorValue1 != value || selectorValue2 != value)
		//	throw std::runtime_error("These seem to match.  Which one to trust?");

		//m_pBank1Rom = m_prgRom + (c_cb16RomBank * selectorValue1);

		m_pBank1Rom = m_prgRom + (c_cb16RomBank * (value & 0x7));
	}
	else if (address >= 4020 && address < 0x6000)
	{
		// Nothing here, just no-op
	}
	else if (address >= 0x6000 && address < 0x8000)
	{
		throw std::runtime_error("Cartridge RAM currently unsupported");
	}
	else
	{
		// Hrm, why does anyone do this?
		throw std::runtime_error("Unexpected mapper address");
	}
}

uint8_t UxROM::ReadAddress(uint16_t address)
{
	if (address >= 0xC000)
		return m_pBank2Rom[address - 0xC000];
	else if (address >= 0x8000)
		return m_pBank1Rom[address - 0x8000];
	else if (address >= 0x6000)
		throw std::runtime_error("Cartridge RAM currently unsupported");
	else
		return 0;
}


void UxROM::WriteChrAddress(uint16_t address, uint8_t value)
{
	if (address >= 0x0000 && address < 0x2000)
		m_chrRAM[address - 0x0000] = value;
	else
		m_basePpuMemory.WriteMemory(address, value);
}


uint8_t UxROM::ReadChrAddress(uint16_t address)
{
	if (address >= 0x0000 && address < 0x2000)
		return m_chrRAM[address - 0x0000];
	else
		return m_basePpuMemory.ReadMemory(address);
}


MapperPtr CreateUxROMMapper()
{
	return std::make_unique<UxROM>();
}


}

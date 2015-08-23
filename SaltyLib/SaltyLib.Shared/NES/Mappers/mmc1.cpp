#include "pch.h"

#include "../IMapper.h"
#include "../NESRom.h"
#include "BasePpuMemoryMap.h"

#include <stdexcept>

namespace NES
{

enum class PrgRomBankMode : uint8_t
{
	//Switch32K = 0x00, 0x01,
	FixLower16k = 0x2,
	FixUpper16k = 0x3,
};

struct MMC0ControlFlags
{
	uint8_t mirroringMode:2;
	PrgRomBankMode prgRomBankMode:2;
	uint8_t chrRomBankMode:1;
};

const uint32_t c_cb16RomBank = (16 * 1024);

class MMC1Mapper : public IMapper
{
public:
	virtual void LoadFromRom(const NESRom& rom) override;
	virtual void WriteAddress(uint16_t address, uint8_t value) override;
	virtual uint8_t ReadAddress(uint16_t address) override;

	virtual void WriteChrAddress(uint16_t address, uint8_t value) override;
	virtual uint8_t ReadChrAddress(uint16_t address) override;

private:
	void SetRegister(uint16_t address, uint8_t value);

	std::unique_ptr<byte[]> m_spVRAM;
	uint32_t m_cbVRAM = 0;

	byte m_ciram[2 * 16 * 1024];

	const byte* m_prgRom;
	uint32_t m_cbPrgRom;

	uint8_t m_shiftRegister = 0x10;
	uint8_t m_programBankThingy = 0;

	union
	{
		uint8_t m_regControl;
		MMC0ControlFlags m_controlFlags;
	};

	//uint8_t m_regControl = 0;
	uint8_t m_register2 = 0;
	uint8_t m_register3 = 0;
	uint8_t m_register4 = 0;

	const uint8_t* m_pBank1Rom = nullptr;
	const uint8_t* m_pBank2Rom = nullptr;
	uint8_t* m_pChrBank1 = nullptr;
	uint8_t* m_pChrBank2 = nullptr;

	BasePpuMemoryMap m_basePpuMemory;
};


void MMC1Mapper::LoadFromRom(const NESRom& rom)
{
	// Copy pattern tables into vram
	m_cbVRAM = rom.CbChrRomData();
	m_spVRAM = std::make_unique<byte[]>(m_cbVRAM);
	
	const byte* pChrRom = rom.GetChrRom();
	memcpy_s(m_spVRAM.get(), m_cbVRAM, pChrRom, m_cbVRAM);

	m_pChrBank1 = m_spVRAM.get();
	//m_pChrBank2 = m_spVRAM.get() + c_cb16RomBank;

	m_cbPrgRom = rom.GetCbPrgRom();
	m_prgRom = rom.GetPrgRom();

	m_pBank1Rom = m_prgRom;
	m_pBank2Rom = m_prgRom + (m_cbPrgRom - c_cb16RomBank);
}

void MMC1Mapper::WriteAddress(uint16_t address, uint8_t value)
{
	if (address >= 0x8000)
	{
		bool shouldTransfer = (m_shiftRegister & 0x01) != 0;
		m_shiftRegister >>= 1;

		if ((value & 0x80) != 0)
		{
			m_shiftRegister = 0x10;
		}
		else
		{
			if ((value & 0x01) != 0)
			{
				m_shiftRegister |= 0x10;
			}

			if (shouldTransfer)
			{
				SetRegister(address, m_shiftRegister);
				m_shiftRegister = 0x10;
			}
		}
	}
}

uint8_t MMC1Mapper::ReadAddress(uint16_t address)
{
	if (address >= 0xC000)
		return m_pBank2Rom[address - 0xC000];
	else if (address >= 0x8000)
		return m_pBank1Rom[address - 0x8000];
	else
		throw std::runtime_error("Unexpected mapper address");
}


void MMC1Mapper::WriteChrAddress(uint16_t address, uint8_t value)
{
	if (address >= 0x0000 && address < 0x1000)
		m_pChrBank1[address - 0x0000] = value;
	else if (address >= 0x1000 && address < 0x2000)
		m_pChrBank2[address - 0x1000] = value;
	else
		m_basePpuMemory.WriteMemory(address, value);
}


uint8_t MMC1Mapper::ReadChrAddress(uint16_t address)
{
	if (address >= 0x0000 && address < 0x1000)
		return m_pChrBank1[address - 0x0000];
	else if (address >= 0x1000 && address < 0x2000)
		return m_pChrBank2[address - 0x1000];
	else
		return m_basePpuMemory.ReadMemory(address);
}


void MMC1Mapper::SetRegister(uint16_t address, uint8_t value)
{
	uint16_t registerSelector = (address >> 13) & 0x3;
	if (registerSelector == 0)
	{
		m_regControl = value;
	}
	else if (registerSelector == 1)
	{
		m_register2 = value;
		m_pChrBank1 = m_spVRAM.get() + (value * c_cb16RomBank);
	}
	else if (registerSelector == 2)
	{
		m_register3 = value;
		m_pChrBank2 = m_spVRAM.get() + (value * c_cb16RomBank);
	}
	else if (registerSelector == 3)
	{
		m_register4 = value;

		uint8_t prgRomBank = value & 0xF;
		if (m_controlFlags.prgRomBankMode == PrgRomBankMode::FixLower16k)
		{
			m_pBank2Rom = m_prgRom + (prgRomBank * c_cb16RomBank);
		}
		else if (m_controlFlags.prgRomBankMode == PrgRomBankMode::FixUpper16k)
		{
			m_pBank1Rom = m_prgRom + (prgRomBank * c_cb16RomBank);
		}
		else
		{
			prgRomBank &= 0xE;
			m_pBank1Rom = m_prgRom + (prgRomBank * c_cb16RomBank);
			m_pBank2Rom = m_prgRom + (prgRomBank * c_cb16RomBank) + 1;
		}
	}
}

MapperPtr CreateMMC1Mapper()
{
	return std::make_unique<MMC1Mapper>();
}


}

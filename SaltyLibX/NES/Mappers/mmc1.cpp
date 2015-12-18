#include "stdafx.h"

#include "../IMapper.h"
#include "../NESRom.h"
#include "BasePpuMemoryMap.h"

#include <stdexcept>
#include <vector>

namespace NES
{

enum class PrgRomBankMode : uint8_t
{
	//Switch32K = 0x00, 0x01,
	FixLower16k = 0x2,
	FixUpper16k = 0x3,
};

enum class ChrRomBankMode : uint8_t
{
	Switch8K = 0x0,
	Switch4K = 0x1,
};


struct MMC0ControlFlags
{
	uint8_t mirroringMode:2;
	PrgRomBankMode prgRomBankMode:2;
	ChrRomBankMode chrRomBankMode:1;
};

const uint32_t c_cb16RomBank = (16 * 1024);
const uint32_t c_cbChrRomBank = (4 * 1024);

class MMC1Mapper : public IMapper
{
public:
	MMC1Mapper& operator=(const MMC1Mapper& other) = delete;

	virtual void LoadFromRom(const NESRom& rom) override;
	virtual void WriteAddress(uint16_t address, uint8_t value) override;
	virtual uint8_t ReadAddress(uint16_t address) override;

	virtual void WriteChrAddress(uint16_t address, uint8_t value) override;
	virtual uint8_t ReadChrAddress(uint16_t address) override;

private:
	void SetRegister(uint16_t address, uint8_t value);

	std::vector<byte> m_vram;
	bool m_isVRAM = false;

	uint32_t m_cbVROM = 0;
	const uint32_t c_cbVRAM = 8 * 1024;

	const byte* m_prgRom;
	uint32_t m_cbPrgRom;

	uint8_t m_shiftRegister = 0x10;

	union
	{
		uint8_t m_regControl;
		MMC0ControlFlags m_controlFlags;
	};

	const uint8_t* m_pPrgRomBank1 = nullptr;
	const uint8_t* m_pPrgRomBank2 = nullptr;
	uint8_t* m_pChrBank1 = nullptr;
	uint8_t* m_pChrBank2 = nullptr;

	uint8_t m_prgRam[8 * 1024]; // 8K (battery backed) RAM

	BasePpuMemoryMap m_basePpuMemory;
};


void MMC1Mapper::LoadFromRom(const NESRom& rom)
{
	// Copy pattern tables into vram
	m_cbVROM = rom.CbChrRomData();

	// No CHR ROM indicates CHR RAM
	if (m_cbVROM == 0)
	{
		// Just allocate 8k of RAM instead
		m_isVRAM = true;
		m_vram.resize(c_cbVRAM);
	}
	else
	{
		m_vram.resize(m_cbVROM);
		const byte* pChrRom = rom.GetChrRom();
		memcpy_s(&m_vram[0], m_vram.size(), pChrRom, m_cbVROM);
	}
	
	m_pChrBank1 = m_vram.data();
	m_pChrBank2 = m_vram.data() + c_cbChrRomBank;

	m_cbPrgRom = rom.GetCbPrgRom();
	m_prgRom = rom.GetPrgRom();

	m_pPrgRomBank1 = m_prgRom;
	m_pPrgRomBank2 = m_prgRom + (m_cbPrgRom - c_cb16RomBank);
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
	else if (address >= 4020 && address < 0x6000)
	{
		// Nothing here, just no-op
	}
	else if (address >= 0x6000 && address < 0x8000)
	{
		// Write to cartridge RAM
		m_prgRam[address - 0x6000] = value;
	}
	else
	{
		// Hrm, why does anyone do this?
		throw std::runtime_error("Unexpected mapper address");
	}
}

uint8_t MMC1Mapper::ReadAddress(uint16_t address)
{
	if (address >= 0xC000)
		return m_pPrgRomBank2[address - 0xC000];
	else if (address >= 0x8000)
		return m_pPrgRomBank1[address - 0x8000];
	else if (address >= 0x6000)
		return m_prgRam[address - 0x6000];
	else
		throw std::runtime_error("Unexpected mapper address");
}


void MMC1Mapper::WriteChrAddress(uint16_t address, uint8_t value)
{
	if (address >= 0x0000 && address < 0x1000)
	{
		if (m_pChrBank1 != nullptr)
			m_pChrBank1[address - 0x0000] = value;
	}
	else if (address >= 0x1000 && address < 0x2000)
	{
		if (m_pChrBank2 != nullptr)
			m_pChrBank2[address - 0x1000] = value;
	}
	else
	{
		m_basePpuMemory.WriteMemory(address, value);
	}
}


uint8_t MMC1Mapper::ReadChrAddress(uint16_t address)
{
	if (address >= 0x0000 && address < 0x1000)
	{
		if (m_pChrBank1 != nullptr)
			return m_pChrBank1[address - 0x0000];
		else
			return 0;
	}
	else if (address >= 0x1000 && address < 0x2000)
	{
		if (m_pChrBank2 != nullptr)
			return m_pChrBank2[address - 0x1000];
		else
			return 0;
	}
	else
	{
		return m_basePpuMemory.ReadMemory(address);
	}
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
		if (m_controlFlags.chrRomBankMode == ChrRomBankMode::Switch8K)
		{
			value &= 0xFE;

			if ((value+1) * c_cbChrRomBank < m_vram.size())
			{
				m_pChrBank1 = m_vram.data() + (value * c_cbChrRomBank);
				m_pChrBank2 = m_vram.data() + ((value + 1) * c_cbChrRomBank);
			}
			else
			{
				throw std::runtime_error("Switching to more ROM memory than we have");
			}
		}
		else // if (m_controlFlags.chrRomBankMode == ChrRomBankMode::Switch4K)
		{
			if (m_cbVROM == 0)
			{
				m_pChrBank1 = nullptr;
			}
			else if (value * c_cbChrRomBank < m_cbVROM)
			{
				m_pChrBank1 = m_vram.data() + (value * c_cbChrRomBank);
			}
			else
			{
				throw std::runtime_error("Switching to more ROM memory than we have");
			}
		}
	}
	else if (registerSelector == 2)
	{
		if (m_controlFlags.chrRomBankMode == ChrRomBankMode::Switch8K)
			throw std::runtime_error("Need to ignore this if it happens");

		if (m_cbVROM == 0)
		{
			m_pChrBank2 = nullptr;
		}
		else if (value * c_cbChrRomBank < m_cbVROM)
		{
			m_pChrBank2 = m_vram.data() + (value * c_cbChrRomBank);
		}
		else
		{
			throw std::runtime_error("Switching to more ROM memory than we have");
		}
	}
	else if (registerSelector == 3)
	{
		uint8_t prgRomBank = value & 0xF;
		if (m_controlFlags.prgRomBankMode == PrgRomBankMode::FixLower16k)
		{
			m_pPrgRomBank2 = m_prgRom + (prgRomBank * c_cb16RomBank);
		}
		else if (m_controlFlags.prgRomBankMode == PrgRomBankMode::FixUpper16k)
		{
			m_pPrgRomBank1 = m_prgRom + (prgRomBank * c_cb16RomBank);
		}
		else
		{
			prgRomBank &= 0xE;
			m_pPrgRomBank1 = m_prgRom + (prgRomBank * c_cb16RomBank);
			m_pPrgRomBank2 = m_prgRom + (prgRomBank * c_cb16RomBank) + 1;
		}
	}
}

MapperPtr CreateMMC1Mapper()
{
	return std::make_unique<MMC1Mapper>();
}


}

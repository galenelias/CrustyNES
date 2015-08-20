#include "pch.h"

#include "../IMapper.h"
#include "../NESRom.h"

#include <stdexcept>

namespace NES
{

struct MMC0ControlFlags
{
	uint8_t mirroringMode:2;
	uint8_t prgRomBankMode:2;
	uint8_t chrRomBankMode:1;
};



class MMC1Mapper : public IMapper
{
public:
	virtual void LoadFromRom(const NESRom& rom) override
	{
		// Copy pattern tables into vram
		m_cbVRAM = rom.CbChrRomData();
		m_spVRAM = std::make_unique<byte[]>(m_cbVRAM);
		
		const byte* pChrRom = rom.GetChrRom();
		memcpy_s(m_spVRAM.get(), m_cbVRAM, pChrRom, m_cbVRAM);

		m_cbPrgRom = rom.GetCbPrgRom();
		m_prgRom = rom.GetPrgRom();

		m_pBank1Rom = m_prgRom;
		m_pBank2Rom = m_prgRom + (m_cbPrgRom - (16 * 1024));
	}

	virtual void WriteAddress(uint16_t address, uint8_t value) override
	{
		bool shouldTransfer = (m_shiftRegister & 0x01) != 0;
		m_shiftRegister >>= 1;
		
		if ((value & 0x80) != 0)
		{
			m_shiftRegister = 0;
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

	virtual uint8_t ReadAddress(uint16_t address) override
	{
		if (address >= 0xC000)
			return m_pBank2Rom[address - 0xC000];
		else if (address >= 0x8000)
			return m_pBank1Rom[address - 0x8000];
		else
			throw std::runtime_error("Unexpected mapper address");
	}


	virtual void WriteChrAddress(uint16_t address, uint8_t value) override
	{
		uint16_t effectiveOffset = address;// % c_cbVRAM;
		m_spVRAM[effectiveOffset] = value;
	}

	virtual uint8_t ReadChrAddress(uint16_t address) override
	{
		uint16_t effectiveOffset = address;// % c_cbVRAM;
		return m_spVRAM[effectiveOffset];
	}


private:

	void SetRegister(uint16_t address, uint8_t value)
	{
		uint16_t registerSelector = (address >> 14);
		if (registerSelector == 0)
		{
			m_regControl = value;
		}
		else if (registerSelector == 1)
		{
			m_register2 = value;
		}
		else if (registerSelector == 2)
		{
			m_register3 = value;
		}
		else if (registerSelector == 3)
		{
			m_register4 = value;
		}
	}

	//static const uint16_t c_cbVRAM = 16*1024; // 0x4000
	//uint8_t m_vram[c_cbVRAM]; // 16KB of video ram
	std::unique_ptr<byte[]> m_spVRAM;
	uint32_t m_cbVRAM = 0;

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
};


MapperPtr CreateMMC1Mapper()
{
	return std::make_unique<MMC1Mapper>();
}


}

#include "pch.h"

#include "../IMapper.h"
#include "../NESRom.h"


namespace NES
{

class MMC1Mapper : public IMapper
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

	static const uint16_t c_cbVRAM = 16*1024; // 0x4000
	uint8_t m_vram[c_cbVRAM]; // 16KB of video ram


	uint8_t m_shiftRegister = 0x10;
	uint8_t m_programBankThingy = 0;

	uint8_t m_regControl = 0;
	uint8_t m_register2 = 0;
	uint8_t m_register3 = 0;
	uint8_t m_register4 = 0;
};


MapperPtr CreateMMC1Mapper()
{
	return std::make_unique<MMC1Mapper>();
}


}

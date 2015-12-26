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
	Mode0 = 0x0,
	Mode1 = 0x1,
	Mode2 = 0x2,
	Mode3 = 0x3,
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

class MMC5Mapper : public IMapper
{
public:
	MMC5Mapper& operator=(const MMC5Mapper& other) = delete;

	virtual void LoadFromRom(const NESRom& rom) override;
	virtual void WriteAddress(uint16_t address, uint8_t value) override;
	virtual uint8_t ReadAddress(uint16_t address) override;

	virtual void WriteChrAddress(uint16_t address, uint8_t value) override;
	virtual uint8_t ReadChrAddress(uint16_t address) override;

private:
	uint16_t MapPrgAddress(uint16_t address);

	PrgRomBankMode m_prgBankMode;
};


void MMC5Mapper::LoadFromRom(const NESRom& rom)
{

}

uint16_t MMC5Mapper::MapPrgAddress(uint16_t address)
{
	if (m_prgBankMode == PrgRomBankMode::Mode0)
	{
		if (address >= 0x6000 && address < 0x8000)
		{
			
		}
	}

	return address;
}


void MMC5Mapper::WriteAddress(uint16_t address, uint8_t value)
{
	if (address == 0x5100)
	{
		m_prgBankMode = static_cast<PrgRomBankMode>(value & 3);

		// // Select PRG mode
		//if ((value & 3) == 0)
		//{
		//	// 32KB bank
		//}
		//else if ((value & 3) == 1)
		//{
		//	// Two 16KB bank
		//}
		//else if ((value & 3) == 2)
		//{
		//	// One 16KB bank, 2 8KB banks
		//}
		//else if ((value & 3) == 3)
		//{
		//	// Four 8KB banks
		//}
	}
	else if (address == 0x5101)
	{
		// Select CHR mode
		if ((value & 3) == 0)
		{
			// 0-8KB CHR pages
		}
		else if ((value & 3) == 1)
		{
			// 1-4KB CHR pages
		}
		else if ((value & 3) == 2)
		{
			// 2-2KB CHR pages
		}
		else if ((value & 3) == 3)
		{
			// 3-1KB CHR pages
		}
	}
	else if (address == 0x5102)
	{
		// PRG RAM Protect 1
		// TODO, to write to PRG RAM, must be set to '10'
	}
	else if (address == 0x5103)
	{
		// PRG RAM Protect 2
		// TODO, to write to PRG RAM, must be set to '01'
	}
	else if (address == 0x5104)
	{
		// Extend RAM mode
		// TODO
	}
	else if (address == 0x5105)
	{
		// Nametable mapping
		// TODO
	}
	else if (address >= 0x8000)
	{
	}
	else
	{
		// Hrm, why does anyone do this?
		throw std::runtime_error("Unexpected mapper address");
	}
}

uint8_t MMC5Mapper::ReadAddress(uint16_t address)
{
	return 0;
}


void MMC5Mapper::WriteChrAddress(uint16_t address, uint8_t value)
{
}


uint8_t MMC5Mapper::ReadChrAddress(uint16_t address)
{
	return 0;
}



MapperPtr CreateMMC5Mapper()
{
	return std::make_unique<MMC5Mapper>();
}


}

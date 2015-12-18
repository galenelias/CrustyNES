#pragma once

#include <stdint.h>
#include <memory>

namespace NES
{

// Forward declarations
class NESRom;

class IMapper
{
public:
	virtual ~IMapper() {};

	virtual void LoadFromRom(const NESRom& rom) = 0;

	virtual void WriteAddress(uint16_t address, uint8_t value) = 0;
	virtual uint8_t ReadAddress(uint16_t address) = 0;

	virtual void WriteChrAddress(uint16_t address, uint8_t value) = 0;
	virtual uint8_t ReadChrAddress(uint16_t address) = 0;

};


typedef std::unique_ptr<IMapper> MapperPtr;

MapperPtr CreateMapper(uint32_t mapperNumber);

}
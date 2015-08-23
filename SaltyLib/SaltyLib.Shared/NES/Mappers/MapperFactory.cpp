#include "pch.h"

#include "../IMapper.h"
#include <stdexcept>

namespace NES
{

MapperPtr CreateMMC0Mapper();
MapperPtr CreateMMC1Mapper();

MapperPtr CreateMapper(uint32_t mapperNumber)
{

	switch (mapperNumber)
	{
	case 0:
		return CreateMMC0Mapper();
	case 1:
		return CreateMMC1Mapper();
	default:
		throw std::runtime_error("Unsupported mapper");
	}

}


}
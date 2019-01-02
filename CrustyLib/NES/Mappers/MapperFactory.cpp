#include "stdafx.h"

#include "../IMapper.h"
#include "../NES.h"
#include <stdexcept>

namespace NES
{

MapperPtr CreateMMC0Mapper();
MapperPtr CreateMMC1Mapper();
MapperPtr CreateUxROMMapper();
MapperPtr CreateMMC5Mapper();
MapperPtr CreateCNROMMapper();

MapperPtr CreateMapper(uint32_t mapperNumber)
{
	switch (mapperNumber)
	{
	case 0:
		return CreateMMC0Mapper();
	case 1:
		return CreateMMC1Mapper();
	case 2:
		return CreateUxROMMapper();
	case 3:
		return CreateCNROMMapper();
	//case 5:
	// Still under construction
	//	return CreateMMC5Mapper();
	default:
		throw unsupported_mapper(mapperNumber);
	}

	// TODO:
	//  4
	//    BladeBuster
	//    River city ransom

	// 9
	//   Punch Out
}


}
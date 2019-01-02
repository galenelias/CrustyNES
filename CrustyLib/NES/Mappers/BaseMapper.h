#pragma once

#include "../IMapper.h"

namespace NES
{

class BaseMapper : public IMapper
{
	virtual void SetTick(uint64_t /*tickCount*/) override {}

};

}
#pragma once

#include "APU.h"
#include <memory>

namespace NES { namespace APU { namespace blargg {

std::unique_ptr<IApu> CreateBlarggApu();

} } } // NES::APU::blargg

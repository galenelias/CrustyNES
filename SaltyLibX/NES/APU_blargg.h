#pragma once

#include <stdint.h>
#include <memory>

#include "APU.h"
#include "nes_apu/nes_apu.h"

namespace NES { namespace APU { namespace blargg {

std::unique_ptr<IApu> CreateBlarggApu();

} } } // NES::APU::blargg

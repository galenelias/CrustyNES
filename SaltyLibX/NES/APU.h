#pragma once

#include <stdint.h>
#include <memory>

#include "nes_apu/nes_apu.h"

namespace NES { namespace APU {

const int c_cpuFrequency = 1789773; // Hz (cycles/second), i.e. 1.789773 MHz

class IApu
{
public:
	virtual void AddCycles(uint32_t cpuCycles) = 0;
	virtual void WriteMemory8(uint16_t offset, uint8_t value) = 0;
	virtual uint8_t ReadStatus() = 0;
	virtual void PushAudio() = 0;

	virtual void EnableSound(bool isEnabled) = 0;
};

std::unique_ptr<IApu> CreateApu();


} } // NES::APU

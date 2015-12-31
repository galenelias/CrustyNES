#include "stdafx.h"
#include "Cpu6502.h"
#include "Ppu.h"
#include "NES.h"

#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace CPU
{

const uint16_t c_stackOffset = 0x100;

// BranchFlagSelector
enum BranchFlagSelector
{
	Negative = 0x0,
	Overflow = 0x1,
	Carry    = 0x2,
	Zero     = 0x3,
};


Cpu6502::Cpu6502(NES::NES& nes)
	: m_nes(nes)
	, m_ppu(nes.GetPpu())
	, m_apu(nes.GetApu())
{
}


void Cpu6502::GenerateNonMaskableInterrupt()
{
	PushValueOntoStack16(m_pc);
	PushValueOntoStack8(m_status);
	m_pc = ReadMemory16(static_cast<uint16_t>(0xFFFA));
}


void Cpu6502::SetRomMapper(NES::IMapper* pMapper)
{
	m_pMapper = pMapper;
}

uint16_t MapIoRegisterMemoryOffset(uint16_t offset)
{
	// $2008 - $4000 are all mapped to $2000-$2007, so adjust our offset
	return offset & 0x2007;
}

uint8_t Cpu6502::ReadMemory8(uint16_t offset) const
{
	if (offset >= 0x4020) // PRG ROM
	{
		return m_pMapper->ReadAddress(offset);
	}
	else if (offset < 0x800) // CPU RAM
	{
		return m_cpuRam[offset];
	}
	else if (offset >= 0x2000 && offset < 0x4000)
	{
		// 0x2008-0x3FFF are all a mirror of 0x2000-0x2007
		//offset &= 0x2007;

		// PPU I/O registeres
		uint16_t mappedOffset = MapIoRegisterMemoryOffset(offset);
		if (mappedOffset == 0x2002)
		{
			return m_ppu.ReadPpuStatus();
		}
		else if (mappedOffset == 0x2004)
		{
			return m_ppu.ReadOamData();
		}
		else if (mappedOffset == 0x2005 || mappedOffset == 0x2006)
		{
			throw std::runtime_error("Unexpected read of VRAM Address register");
		}
		else if (mappedOffset == 0x2007)
		{
			return m_ppu.ReadCpuDataRegister();
		}
		else
		{
			throw std::runtime_error("Unexpected read location");
		}
	}
	else if (offset == 0x4016)
	{
		return m_nes.UseController1().ReadData();
	}
	else if (offset == 0x4017)
	{
		// REVIEW: Control pad 2... TODO, ignore for now
		return 0;
	}
	else if (offset == 0x4015)
	{
		return m_apu.ReadStatus();
	}
	else
	{
		throw std::runtime_error("Unexpected read location");
	}
}


void Cpu6502::WriteMemory8(uint16_t offset, uint8_t value)
{
	if (offset < 0x800) // CPU RAM
	{
		m_cpuRam[offset] = value;
	}
	else if (offset >= 0x2000 && offset < 0x4000)
	{
		// PPU I/O registeres
		uint16_t mappedOffset = MapIoRegisterMemoryOffset(offset);
		if (mappedOffset == 0x2000)
		{
			// Talk to PPU, or some memory bus abstraction?
			m_ppu.WriteControlRegister1(value);
		}
		else if (mappedOffset == 0x2001)
		{
			m_ppu.WriteControlRegister2(value);
		}
		else if (mappedOffset == 0x2002)
		{
			// Arkanoid apparently tries to do this... not sure what the expected result is.  Lets try skipping it for now
			//throw std::runtime_error("Unexpected write of PPU status register");
		}
		else if (mappedOffset == 0x2003)
		{
			m_ppu.WriteOamAddress(value);
		}
		else if (mappedOffset == 0x2004)
		{
			m_ppu.WriteOamData(value);
		}
		else if (mappedOffset == 0x2005)
		{
			m_ppu.WriteScrollRegister(value);
		}
		else if (mappedOffset == 0x2006)
		{
			m_ppu.WriteCpuAddressRegister(value);
		}
		else if (mappedOffset == 0x2007)
		{
			m_ppu.WriteCpuDataRegister(value);
		}
		else
		{
			throw std::runtime_error("Not handled yet");
		}
	}
	else if (offset >= 0x4000 && offset < 0x4014)
	{
		m_apu.WriteMemory8(offset, value);
	}
	else if (offset == 0x4014)
	{
		// PPU sprite DMA (OAMDMA)

		uint16_t baseOffset = static_cast<uint16_t>(value) << 8;
		if (baseOffset < 0x800) // CPU RAM
		{
			m_ppu.TriggerOamDMA(m_cpuRam + baseOffset);
		}
		else
		{
			// REVIEW: Get real contiguous byte*?
			uint16_t baseOffset = static_cast<uint16_t>(value) << 8;
			uint8_t dmaData[256];
			for (uint16_t i=0; i != 256; ++i)
			{
				dmaData[i] = ReadMemory8(baseOffset + i);
			}

			m_ppu.TriggerOamDMA(dmaData);
		}
	}
	else if (offset == 0x4015)
	{
		// pAPU Sound / Vertical Clock Signal Register
		m_apu.WriteMemory8(offset, value);
	}
	else if (offset == 0x4016)
	{
		m_nes.UseController1().WriteData(value);
	}
	else if (offset == 0x4017)
	{
		// 0x4017 for writes is actually mapped to the APU
		m_apu.WriteMemory8(offset, value);
	}
	else if (offset >= 0x4020)
	{
		m_pMapper->WriteAddress(offset, value);
	}
	else
	{
		throw std::runtime_error("Not handled yet");
	}
}

uint16_t Cpu6502::ReadMemory16(uint16_t offset) const
{
	const uint16_t lowByte = ReadMemory8(offset);
	const uint16_t highByte = ReadMemory8(offset+1);

	return (highByte << 8) | lowByte;
}


byte* Cpu6502::MapWritableMemoryOffset(uint16_t offset)
{
	// This function only supports non memory mapped values

	if (offset < 0x800) // CPU RAM
	{
		return m_cpuRam + offset;
	}
	else
	{
		return nullptr;
	}

}


void Cpu6502::Reset()
{
	// Jump to code offset specified by the RESET thingy
	m_pc = ReadMemory16(static_cast<uint16_t>(0xFFFC));
	m_sp = 0xFD; // REVIEW: Figure out why normal NES starts at FD, not FF.  Initial push of status flags?

	// REVIEW: Push something on to stack?  Initial stack pointer should be FD
	
	m_status = static_cast<uint8_t>(CpuStatusFlag::Bit5 | CpuStatusFlag::InterruptDisabled); // Bit 5 doesn't exist, so pin it in the '1' state.  Not sure why interrupts start disabled

	m_totalCycles = 0;
	m_cyclesRemaining = 0;

	// Override to allow for execution of code segment of nestest
	//m_pc = 0xc000;
}

uint8_t Cpu6502::ReadUInt8(AddressingMode mode)
{
	if (mode == AddressingMode::IMM)
	{
		return ReadMemory8(m_pc++);
	}
	else if (mode == AddressingMode::ACC)
	{
		return m_acc;
	}
	else
	{
		return ReadMemory8(GetAddressingModeOffset_Read(mode));
	}
}

uint16_t Cpu6502::GetIndexedIndirectOffset()
{
	uint8_t zpOffset = ReadMemory8(m_pc++);
	zpOffset += m_x;

	// We have to make sure to continue zero page indexing for the 16-bit offset so we wrap at $00FF
	uint8_t indirectOffsetLow = ReadMemory8(zpOffset++);
	uint8_t indirectOffsetHigh = ReadMemory8(zpOffset);
	uint16_t indirectOffset = (indirectOffsetHigh << 8) | indirectOffsetLow;
	return indirectOffset;
}

uint16_t Cpu6502::GetIndirectIndexedOffset_Read()
{
	uint8_t zpOffset = ReadMemory8(m_pc++);

	// We have to make sure to continue zero page indexing for the 16-bit offset so we wrap at $00FF
	uint8_t indirectOffsetLow = ReadMemory8(zpOffset++);
	uint8_t indirectOffsetHigh = ReadMemory8(zpOffset);
	uint16_t indirectOffset = (indirectOffsetHigh << 8) | indirectOffsetLow;
	uint16_t indirectOffsetAdjusted = indirectOffset + m_y;

	if (((indirectOffset ^ indirectOffsetAdjusted) & 0x100) != 0)
	{
		// When the addressing crosses pages, we need an additional cycle to handle the
		// add on the most significant byte
		AddCycles(1);
	}
	return indirectOffsetAdjusted;
}


uint16_t Cpu6502::GetIndirectIndexedOffset_ReadWrite()
{
	uint8_t zpOffset = ReadMemory8(m_pc++);

	// We have to make sure to continue zero page indexing for the 16-bit offset so we wrap at $00FF
	uint8_t indirectOffsetLow = ReadMemory8(zpOffset++);
	uint8_t indirectOffsetHigh = ReadMemory8(zpOffset);
	uint16_t indirectOffset = (indirectOffsetHigh << 8) | indirectOffsetLow;
	uint16_t indirectOffsetAdjusted = indirectOffset + m_y;
	return indirectOffsetAdjusted;
}

uint16_t Cpu6502::ReadUInt16(AddressingMode mode)
{
	if (mode == AddressingMode::ABS)
	{
		uint16_t value = ReadMemory16(m_pc);
		m_pc += 2;
		return value;
	}
	else if (mode == AddressingMode::ZPX)
	{
		return ReadMemory16(GetAddressingModeOffset_Read(mode));
	}
	else if (mode == AddressingMode::ZP)
	{
		return ReadMemory16(GetAddressingModeOffset_Read(mode));
	}
	else
	{
		throw UnhandledInstruction(0);
	}
}

template <typename Func>
void Cpu6502::ReadModifyWriteUint8(AddressingMode mode, Func func)
{
	uint8_t value;
	uint16_t address;

	if (mode == AddressingMode::ACC)
	{
		value = m_acc;
	}
	else
	{
		address = GetAddressingModeOffset_ReadWrite(mode);
		value = ReadMemory8(address);

		// Read-Modify-Write operations actually do a write back of the original value,
		// this is important for suppressing multiple writes to certain mappers (MMC1)
		WriteMemory8(address, value);
	}

	func(value);

	if (mode == AddressingMode::ACC)
		m_acc = value;
	else
		WriteMemory8(address, value);

}

uint16_t Cpu6502::GetAddressingModeOffset_Read(AddressingMode mode)
{
	if (mode == AddressingMode::ABS)
	{
		uint16_t value = ReadMemory16(m_pc);
		m_pc += 2;
		return value;
	}
	else if (mode == AddressingMode::ABSX)
	{
		uint16_t value = ReadMemory16(m_pc);
		m_pc += 2;
		uint16_t adjustedValue = value + m_x;
		if (((value ^ adjustedValue) & 0x100) != 0)
			AddCycles(1); // Reads which have to adjust the most significant bit of the read address incur an additional cycle
		return adjustedValue;
	}
	else if (mode == AddressingMode::ABSY)
	{
		uint16_t value = ReadMemory16(m_pc);
		m_pc += 2;
		uint16_t adjustedValue = value + m_y;
		if (((value ^ adjustedValue) & 0x100) != 0)
			AddCycles(1); // Reads which have to adjust the most significant bit of the read address incur an additional cycle
		return adjustedValue;
	}
	else if (mode == AddressingMode::ZP)
	{
		uint8_t zpOffset = ReadMemory8(m_pc++);
		return static_cast<uint16_t>(zpOffset);
	}
	else if (mode == AddressingMode::ZPX)
	{
		uint8_t zpOffset = ReadMemory8(m_pc++);
		uint8_t memoryOffset = zpOffset + m_x;
		return memoryOffset;
	}
	else if (mode == AddressingMode::ZPY)
	{
		uint8_t zpOffset = ReadMemory8(m_pc++);
		uint8_t memoryOffset = zpOffset + m_y;
		return memoryOffset;
	}
	else if (mode == AddressingMode::_ZPX_)
	{
		return GetIndexedIndirectOffset();
	}
	else if (mode == AddressingMode::_ZP_Y)
	{
		return GetIndirectIndexedOffset_Read();
	}
	else if (mode == AddressingMode::IMM)
	{
		throw InvalidInstruction(0);  // Found in nestest.rom, invalid addressing mode for the given instructions
	}
	else
	{
		throw UnhandledInstruction(0);
	}
}

uint16_t Cpu6502::GetAddressingModeOffset_ReadWrite(AddressingMode mode)
{
	if (mode == AddressingMode::ABS)
	{
		uint16_t value = ReadMemory16(m_pc);
		m_pc += 2;
		return value;
	}
	else if (mode == AddressingMode::ABSX)
	{
		uint16_t value = ReadMemory16(m_pc);
		m_pc += 2;
		value += m_x;
		return value;
	}
	else if (mode == AddressingMode::ABSY)
	{
		uint16_t value = ReadMemory16(m_pc);
		m_pc += 2;
		value += m_y;
		return value;
	}
	else if (mode == AddressingMode::ZPX)
	{
		uint8_t zpOffset = ReadMemory8(m_pc++);
		uint8_t memoryOffset = zpOffset + m_x;
		return memoryOffset;
	}
	else if (mode == AddressingMode::ZPY)
	{
		uint8_t zpOffset = ReadMemory8(m_pc++);
		uint8_t memoryOffset = zpOffset + m_y;
		return memoryOffset;
	}
	else if (mode == AddressingMode::ZP)
	{
		uint8_t zpOffset = ReadMemory8(m_pc++);
		return static_cast<uint16_t>(zpOffset);
	}
	else if (mode == AddressingMode::_ZPX_)
	{
		return GetIndexedIndirectOffset();
	}
	else if (mode == AddressingMode::_ZP_Y)
	{
		return GetIndirectIndexedOffset_ReadWrite();
	}
	else if (mode == AddressingMode::IMM)
	{
		throw InvalidInstruction(0);  // Found in nestest.rom, invalid addressing mode for the given instructions
	}
	else
	{
		throw UnhandledInstruction(0);
	}
}


bool IsNegative(uint8_t val)
{
	return ((val & 0x80) != 0);
}

void Cpu6502::SetStatusFlagsFromValue(uint8_t value)
{
	// Set the zero and negative flags of the cpu
	
	// Mask off the status bits we're setting, and or in the new status bits
	CpuStatusFlag newFlags = CpuStatusFlag::None;
	
	if (IsNegative(value))
		newFlags = newFlags | CpuStatusFlag::Negative;
	else if (value == 0)
		newFlags = newFlags | CpuStatusFlag::Zero;

	SetStatusFlags(newFlags, (CpuStatusFlag::Negative | CpuStatusFlag::Zero));
}

void Cpu6502::SetStatusFlags(CpuStatusFlag flags, CpuStatusFlag mask)
{
	m_status = (m_status & ~static_cast<uint8_t>(mask)) | static_cast<uint8_t>(flags);

}

static uint8_t AddressingModeFromInstruction(uint8_t instruction)
{
	return (instruction & 0x1C) >> 2;
}


static AddressingMode GetClass0AddressingMode(uint8_t instruction)
{
	switch (AddressingModeFromInstruction(instruction))
	{
		case 0: return AddressingMode::IMM;
		case 1: return AddressingMode::ZP;
		case 3: return AddressingMode::ABS;
		case 5: return AddressingMode::ZPX;
		case 7: return AddressingMode::ABSX;
		default: throw InvalidInstruction(instruction);
	}
}

static AddressingMode GetClass1AddressingMode(uint8_t instruction)
{
	switch (AddressingModeFromInstruction(instruction))
	{
		case 0: return AddressingMode::_ZPX_;
		case 1: return AddressingMode::ZP;
		case 2: return AddressingMode::IMM;
		case 3: return AddressingMode::ABS;
		case 4: return AddressingMode::_ZP_Y;
		case 5: return AddressingMode::ZPX;
		case 6: return AddressingMode::ABSY;
		case 7: return AddressingMode::ABSX;
		default: throw InvalidInstruction(instruction);
	}
}

static AddressingMode GetClass2AddressingMode(uint8_t instruction)
{
	switch (AddressingModeFromInstruction(instruction))
	{
		case 0: return AddressingMode::IMM;
		case 1: return AddressingMode::ZP;
		case 2: return AddressingMode::ACC;
		case 3: return AddressingMode::ABS;
		case 5: return AddressingMode::ZPX;
		case 7: return AddressingMode::ABSX;
		default: throw InvalidInstruction(instruction);
	}
}


void Cpu6502::PushValueOntoStack8(uint8_t val)
{
	m_cpuRam[c_stackOffset + m_sp--] = val;
}


void Cpu6502::PushValueOntoStack16(uint16_t val)
{
	PushValueOntoStack8(val >> 8); // Store high word
	PushValueOntoStack8(static_cast<uint8_t>(val)); // Store high word
}


uint8_t Cpu6502::ReadValueFromStack8()
{
	return m_cpuRam[c_stackOffset + (++m_sp)];
}


uint16_t Cpu6502::ReadValueFromStack16()
{
	uint8_t lowWord = ReadValueFromStack8();
	uint8_t highWord = ReadValueFromStack8();
	return (highWord << 8) | lowWord;
}


const char* Cpu6502::GetDebugState() const
{
	static char s_debugStateBuffer[128];
	const uint8_t instruction = ReadMemory8(m_pc);

	sprintf_s(s_debugStateBuffer, _countof(s_debugStateBuffer), "%04hX  %02hhX A:%02hhX X:%02hhX Y:%02hhX P:%02hhX SP:%02hhX CYC:%3d SL:%d\n", m_pc, instruction,
		m_acc, m_x, m_y, m_status, m_sp, m_ppu.GetCycles(), m_ppu.GetScanline());

	return s_debugStateBuffer;
}


void Cpu6502::CompareValues(uint8_t minuend, uint8_t subtrahend)
{
	CpuStatusFlag newFlags = CpuStatusFlag::None;

	if (minuend >= subtrahend) // Carry flags
		newFlags = newFlags | CpuStatusFlag::Carry;
	if (minuend == subtrahend)
		newFlags = newFlags | CpuStatusFlag::Zero;
	if (((minuend - subtrahend) & 0x80) != 0)
		newFlags = newFlags | CpuStatusFlag::Negative;

	SetStatusFlags(newFlags, CpuStatusFlag::Carry | CpuStatusFlag::Zero | CpuStatusFlag::Negative);
}

/*-----------------------------------------------------------------------------
	Cpu6502::AddWithCarry

	Helper for performing an add with carry.  This is used by both ADC and SBC instructions
-------------------------------------------------------------------------------*/
void Cpu6502::AddWithCarry(uint8_t x, uint8_t y)
{
	const int8_t signedX = static_cast<int8_t>(x);
	const int8_t signedY = static_cast<int8_t>(y);
	const bool isCarryBitSet = ((m_status & static_cast<uint8_t>(CpuStatusFlag::Carry)) != 0);

	// Check for overflow by performing wide math
	int wideSignedResult = static_cast<int>(signedX) + static_cast<int>(signedY) + (isCarryBitSet ? 1 : 0);
	unsigned int wideUnsignedResult = static_cast<unsigned int>(x) + static_cast<unsigned int>(y) + (isCarryBitSet ? 1 : 0);

	// Calculate the actual result by performing 8-bit addition
	uint8_t result = x + y + (isCarryBitSet ? 1 : 0);
	SetStatusFlagsFromValue(result);
	
	CpuStatusFlag newStatusFlags = CpuStatusFlag::None;

	if (wideSignedResult < -128 || wideSignedResult > 127) // Signed overflow
		newStatusFlags |= CpuStatusFlag::Overflow;
	if (IsNegative(result))
		newStatusFlags |= CpuStatusFlag::Negative;
	if (result == 0)
		newStatusFlags |= CpuStatusFlag::Zero;

	if (wideUnsignedResult >= 256)
	{
		// Carry happens if two negative numbers go super negative, or if a negative number and a positive become positive
		newStatusFlags |= CpuStatusFlag::Carry;
	}

	SetStatusFlags(newStatusFlags, CpuStatusFlag::Overflow | CpuStatusFlag::Carry | CpuStatusFlag::Negative | CpuStatusFlag::Zero);
	m_acc = result;
}

Cpu6502::OpCodeTableEntry* Cpu6502::DoOpcodeStuff(uint8_t opCode)
{
	static Cpu6502::OpCodeTableEntry s_opCodeTable[] = {
		{ 0x00 /*BRK*/, 7, &Cpu6502::Instruction_Break, AddressingMode::IMP},
		{ 0x01 /*ORA*/, 6, &Cpu6502::Instruction_OrWithAccumulator, AddressingMode::_ZPX_},
		{ 0x05 /*ORA*/, 3, &Cpu6502::Instruction_OrWithAccumulator, AddressingMode::ZP},
		{ 0x06 /*ASL*/, 5, &Cpu6502::Instruction_ArithmeticShiftLeft, AddressingMode::ZP},
		{ 0x08 /*PHP*/, 3, &Cpu6502::Instruction_PushProcessorStatus, AddressingMode::IMP},
		{ 0x09 /*ORA*/, 2, &Cpu6502::Instruction_OrWithAccumulator, AddressingMode::IMM},
		{ 0x0A /*ASL*/, 2, &Cpu6502::Instruction_ArithmeticShiftLeft, AddressingMode::ACC},
		{ 0x0D /*ORA*/, 4, &Cpu6502::Instruction_OrWithAccumulator, AddressingMode::ABS},
		{ 0x0E /*ASL*/, 6, &Cpu6502::Instruction_ArithmeticShiftLeft, AddressingMode::ABS},
		{ 0x10 /*BPL*/, 2, &Cpu6502::Instruction_BranchOnPlus, AddressingMode::IMP},
		{ 0x11 /*ORA*/, 5, &Cpu6502::Instruction_OrWithAccumulator, AddressingMode::_ZP_Y},
		{ 0x15 /*ORA*/, 4, &Cpu6502::Instruction_OrWithAccumulator, AddressingMode::ZPX},
		{ 0x16 /*ASL*/, 6, &Cpu6502::Instruction_ArithmeticShiftLeft, AddressingMode::ZPX},
		{ 0x18 /*CLC*/, 2, &Cpu6502::Instruction_ClearCarry, AddressingMode::IMP},
		{ 0x19 /*ORA*/, 4, &Cpu6502::Instruction_OrWithAccumulator, AddressingMode::ABSY},
		{ 0x1D /*ORA*/, 4, &Cpu6502::Instruction_OrWithAccumulator, AddressingMode::ABSX},
		{ 0x1E /*ASL*/, 7, &Cpu6502::Instruction_ArithmeticShiftLeft, AddressingMode::ABSX},
		{ 0x20 /*JSR*/, 6, &Cpu6502::Instruction_JumpToSubroutine, AddressingMode::ABS},
		{ 0x21 /*AND*/, 6, &Cpu6502::Instruction_And, AddressingMode::_ZPX_},
		{ 0x24 /*BIT*/, 3, &Cpu6502::Instruction_TestBits, AddressingMode::ZP},
		{ 0x25 /*AND*/, 3, &Cpu6502::Instruction_And, AddressingMode::ZP},
		{ 0x26 /*ROL*/, 5, &Cpu6502::Instruction_RotateLeft, AddressingMode::ZP},
		{ 0x28 /*PLP*/, 4, &Cpu6502::Instruction_PullProcessorStatus, AddressingMode::IMP},
		{ 0x29 /*AND*/, 2, &Cpu6502::Instruction_And, AddressingMode::IMM},
		{ 0x2A /*ROL*/, 2, &Cpu6502::Instruction_RotateLeft, AddressingMode::ACC},
		{ 0x2C /*BIT*/, 4, &Cpu6502::Instruction_TestBits, AddressingMode::ABS},
		{ 0x2D /*AND*/, 4, &Cpu6502::Instruction_And, AddressingMode::ABS},
		{ 0x2E /*ROL*/, 6, &Cpu6502::Instruction_RotateLeft, AddressingMode::ABS},
		{ 0x30 /*BMI*/, 2, &Cpu6502::Instruction_BranchOnMinus, AddressingMode::IMP},
		{ 0x31 /*AND*/, 5, &Cpu6502::Instruction_And, AddressingMode::_ZP_Y},
		{ 0x35 /*AND*/, 4, &Cpu6502::Instruction_And, AddressingMode::ZPX},
		{ 0x36 /*ROL*/, 6, &Cpu6502::Instruction_RotateLeft, AddressingMode::ZPX},
		{ 0x38 /*SEC*/, 2, &Cpu6502::Instruction_SetCarry, AddressingMode::IMP},
		{ 0x39 /*AND*/, 4, &Cpu6502::Instruction_And, AddressingMode::ABSY},
		{ 0x3D /*AND*/, 4, &Cpu6502::Instruction_And, AddressingMode::ABSX},
		{ 0x3E /*ROL*/, 7, &Cpu6502::Instruction_RotateLeft, AddressingMode::ABSX},
		{ 0x40 /*RTI*/, 6, &Cpu6502::Instruction_ReturnFromInterrupt, AddressingMode::IMP},
		{ 0x41 /*EOR*/, 6, &Cpu6502::Instruction_ExclusiveOr, AddressingMode::_ZPX_},
		{ 0x45 /*EOR*/, 3, &Cpu6502::Instruction_ExclusiveOr, AddressingMode::ZP},
		{ 0x46 /*LSR*/, 5, &Cpu6502::Instruction_LogicalShiftRight, AddressingMode::ZP},
		{ 0x48 /*PHA*/, 3, &Cpu6502::Instruction_PushAccumulator, AddressingMode::IMP},
		{ 0x49 /*EOR*/, 2, &Cpu6502::Instruction_ExclusiveOr, AddressingMode::IMM},
		{ 0x4A /*LSR*/, 2, &Cpu6502::Instruction_LogicalShiftRight, AddressingMode::ACC},
		{ 0x4C /*JMP*/, 3, &Cpu6502::Instruction_Jump, AddressingMode::ABS},
		{ 0x4D /*EOR*/, 4, &Cpu6502::Instruction_ExclusiveOr, AddressingMode::ABS},
		{ 0x4E /*LSR*/, 6, &Cpu6502::Instruction_LogicalShiftRight, AddressingMode::ABS},
		{ 0x50 /*BVC*/, 2, &Cpu6502::Instruction_BranchOnOverflowClear, AddressingMode::IMP},
		{ 0x51 /*EOR*/, 5, &Cpu6502::Instruction_ExclusiveOr, AddressingMode::_ZP_Y},
		{ 0x55 /*EOR*/, 4, &Cpu6502::Instruction_ExclusiveOr, AddressingMode::ZPX},
		{ 0x56 /*LSR*/, 6, &Cpu6502::Instruction_LogicalShiftRight, AddressingMode::ZPX},
		{ 0x58 /*CLI*/, 2, &Cpu6502::Instruction_ClearInterrupt, AddressingMode::IMP},
		{ 0x59 /*EOR*/, 4, &Cpu6502::Instruction_ExclusiveOr, AddressingMode::ABSY},
		{ 0x5D /*EOR*/, 4, &Cpu6502::Instruction_ExclusiveOr, AddressingMode::ABSX},
		{ 0x5E /*LSR*/, 7, &Cpu6502::Instruction_LogicalShiftRight, AddressingMode::ABSX},
		{ 0x60 /*RTS*/, 6, &Cpu6502::Instruction_ReturnFromSubroutine, AddressingMode::IMP},
		{ 0x61 /*ADC*/, 6, &Cpu6502::Instruction_AddWithCarry, AddressingMode::_ZPX_},
		{ 0x65 /*ADC*/, 3, &Cpu6502::Instruction_AddWithCarry, AddressingMode::ZP},
		{ 0x66 /*ROR*/, 5, &Cpu6502::Instruction_RotateRight, AddressingMode::ZP},
		{ 0x68 /*PLA*/, 4, &Cpu6502::Instruction_PullAccumulator, AddressingMode::IMP},
		{ 0x69 /*ADC*/, 2, &Cpu6502::Instruction_AddWithCarry, AddressingMode::IMM},
		{ 0x6A /*ROR*/, 2, &Cpu6502::Instruction_RotateRight, AddressingMode::ACC},
		{ 0x6C /*JMP*/, 5, &Cpu6502::Instruction_JumpIndirect, AddressingMode::ABS},
		{ 0x6D /*ADC*/, 4, &Cpu6502::Instruction_AddWithCarry, AddressingMode::ABS},
		{ 0x6E /*ROR*/, 6, &Cpu6502::Instruction_RotateRight, AddressingMode::ABS},
		{ 0x70 /*BVS*/, 2, &Cpu6502::Instruction_BranchOnOverflowSet, AddressingMode::IMP},
		{ 0x71 /*ADC*/, 5, &Cpu6502::Instruction_AddWithCarry, AddressingMode::_ZP_Y},
		{ 0x75 /*ADC*/, 4, &Cpu6502::Instruction_AddWithCarry, AddressingMode::ZPX},
		{ 0x76 /*ROR*/, 6, &Cpu6502::Instruction_RotateRight, AddressingMode::ZPX},
		{ 0x78 /*SEI*/, 2, &Cpu6502::Instruction_SetInterrupt, AddressingMode::IMP},
		{ 0x79 /*ADC*/, 4, &Cpu6502::Instruction_AddWithCarry, AddressingMode::ABSY},
		{ 0x7D /*ADC*/, 4, &Cpu6502::Instruction_AddWithCarry, AddressingMode::ABSX},
		{ 0x7E /*ROR*/, 7, &Cpu6502::Instruction_RotateRight, AddressingMode::ABSX},
		{ 0x81 /*STA*/, 6, &Cpu6502::Instruction_StoreAccumulator, AddressingMode::_ZPX_},
		{ 0x84 /*STY*/, 3, &Cpu6502::Instruction_StoreY, AddressingMode::ZP},
		{ 0x85 /*STA*/, 3, &Cpu6502::Instruction_StoreAccumulator, AddressingMode::ZP},
		{ 0x86 /*STX*/, 3, &Cpu6502::Instruction_StoreX, AddressingMode::ZP},
		{ 0x88 /*DEY*/, 2, &Cpu6502::Instruction_DecrementY, AddressingMode::IMP},
		{ 0x8A /*TXA*/, 2, &Cpu6502::Instruction_TransferXtoA, AddressingMode::IMP},
		{ 0x8C /*STY*/, 4, &Cpu6502::Instruction_StoreY, AddressingMode::ABS},
		{ 0x8D /*STA*/, 4, &Cpu6502::Instruction_StoreAccumulator, AddressingMode::ABS},
		{ 0x8E /*STX*/, 4, &Cpu6502::Instruction_StoreX, AddressingMode::ABS},
		{ 0x90 /*BCC*/, 2, &Cpu6502::Instruction_BranchOnCarryClear, AddressingMode::IMP},
		{ 0x91 /*STA*/, 6, &Cpu6502::Instruction_StoreAccumulator, AddressingMode::_ZP_Y},
		{ 0x94 /*STY*/, 4, &Cpu6502::Instruction_StoreY, AddressingMode::ZPX},
		{ 0x95 /*STA*/, 4, &Cpu6502::Instruction_StoreAccumulator, AddressingMode::ZPX},
		{ 0x96 /*STX*/, 4, &Cpu6502::Instruction_StoreX, AddressingMode::ZPY},
		{ 0x98 /*TYA*/, 2, &Cpu6502::Instruction_TransferYtoA, AddressingMode::IMP},
		{ 0x99 /*STA*/, 5, &Cpu6502::Instruction_StoreAccumulator, AddressingMode::ABSY},
		{ 0x9A /*TXS*/, 2, &Cpu6502::Instruction_TransferXToStack, AddressingMode::IMP},
		{ 0x9D /*STA*/, 5, &Cpu6502::Instruction_StoreAccumulator, AddressingMode::ABSX},
		{ 0xA0 /*LDY*/, 2, &Cpu6502::Instruction_LoadY, AddressingMode::IMM},
		{ 0xA1 /*LDA*/, 6, &Cpu6502::Instruction_LoadAccumulator, AddressingMode::_ZPX_},
		{ 0xA2 /*LDX*/, 2, &Cpu6502::Instruction_LoadX, AddressingMode::IMM},
		{ 0xA4 /*LDY*/, 3, &Cpu6502::Instruction_LoadY, AddressingMode::ZP},
		{ 0xA5 /*LDA*/, 3, &Cpu6502::Instruction_LoadAccumulator, AddressingMode::ZP},
		{ 0xA6 /*LDX*/, 3, &Cpu6502::Instruction_LoadX, AddressingMode::ZP},
		{ 0xA8 /*TAY*/, 2, &Cpu6502::Instruction_TransferAtoY, AddressingMode::IMP},
		{ 0xA9 /*LDA*/, 2, &Cpu6502::Instruction_LoadAccumulator, AddressingMode::IMM},
		{ 0xAA /*TAX*/, 2, &Cpu6502::Instruction_TransferAtoX, AddressingMode::IMP},
		{ 0xAC /*LDY*/, 4, &Cpu6502::Instruction_LoadY, AddressingMode::ABS},
		{ 0xAD /*LDA*/, 4, &Cpu6502::Instruction_LoadAccumulator, AddressingMode::ABS},
		{ 0xAE /*LDX*/, 4, &Cpu6502::Instruction_LoadX, AddressingMode::ABS},
		{ 0xB0 /*BCS*/, 2, &Cpu6502::Instruction_BranchOnCarrySet, AddressingMode::IMP},
		{ 0xB1 /*LDA*/, 5, &Cpu6502::Instruction_LoadAccumulator, AddressingMode::_ZP_Y},
		{ 0xB4 /*LDY*/, 4, &Cpu6502::Instruction_LoadY, AddressingMode::ZPX},
		{ 0xB5 /*LDA*/, 4, &Cpu6502::Instruction_LoadAccumulator, AddressingMode::ZPX},
		{ 0xB6 /*LDX*/, 4, &Cpu6502::Instruction_LoadX, AddressingMode::ZPY},
		{ 0xB8 /*CLV*/, 2, &Cpu6502::Instruction_ClearOverflow, AddressingMode::IMP},
		{ 0xB9 /*LDA*/, 4, &Cpu6502::Instruction_LoadAccumulator, AddressingMode::ABSY},
		{ 0xBA /*TSX*/, 2, &Cpu6502::Instruction_TransferStackToX, AddressingMode::IMP},
		{ 0xBC /*LDX*/, 4, &Cpu6502::Instruction_LoadY, AddressingMode::ABSX},
		{ 0xBD /*LDA*/, 4, &Cpu6502::Instruction_LoadAccumulator, AddressingMode::ABSX},
		{ 0xBE /*LDX*/, 4, &Cpu6502::Instruction_LoadX, AddressingMode::ABSY},
		{ 0xC0 /*CPY*/, 2, &Cpu6502::Instruction_CompareYRegister, AddressingMode::IMM},
		{ 0xC1 /*CMP*/, 6, &Cpu6502::Instruction_Compare, AddressingMode::_ZPX_},
		{ 0xC4 /*CPY*/, 3, &Cpu6502::Instruction_CompareYRegister, AddressingMode::ZP},
		{ 0xC5 /*CMP*/, 3, &Cpu6502::Instruction_Compare, AddressingMode::ZP},
		{ 0xC6 /*DEC*/, 5, &Cpu6502::Instruction_DecrementMemory, AddressingMode::ZP},
		{ 0xC8 /*INY*/, 2, &Cpu6502::Instruction_IncrementY, AddressingMode::IMP},
		{ 0xC9 /*CMP*/, 2, &Cpu6502::Instruction_Compare, AddressingMode::IMM},
		{ 0xCA /*DEY*/, 2, &Cpu6502::Instruction_DecrementX, AddressingMode::IMP},
		{ 0xCC /*CPY*/, 4, &Cpu6502::Instruction_CompareYRegister, AddressingMode::ABS},
		{ 0xCD /*CMP*/, 4, &Cpu6502::Instruction_Compare, AddressingMode::ABS},
		{ 0xCE /*DEC*/, 6, &Cpu6502::Instruction_DecrementMemory, AddressingMode::ABS},
		{ 0xD0 /*BNE*/, 2, &Cpu6502::Instruction_BranchOnNotEqual, AddressingMode::IMP},
		{ 0xD1 /*CMP*/, 5, &Cpu6502::Instruction_Compare, AddressingMode::_ZP_Y},
		{ 0xD5 /*CMP*/, 4, &Cpu6502::Instruction_Compare, AddressingMode::ZPX},
		{ 0xD6 /*DEC*/, 6, &Cpu6502::Instruction_DecrementMemory, AddressingMode::ZPX},
		{ 0xD8 /*CLD*/, 2, &Cpu6502::Instruction_ClearDecimal, AddressingMode::IMP},
		{ 0xD9 /*CMP*/, 4, &Cpu6502::Instruction_Compare, AddressingMode::ABSY},
		{ 0xDD /*CMP*/, 4, &Cpu6502::Instruction_Compare, AddressingMode::ABSX},
		{ 0xDE /*DEC*/, 7, &Cpu6502::Instruction_DecrementMemory, AddressingMode::ABSX},
		{ 0xE0 /*CPX*/, 2, &Cpu6502::Instruction_CompareXRegister, AddressingMode::IMM},
		{ 0xE1 /*SBC*/, 6, &Cpu6502::Instruction_SubtractWithCarry, AddressingMode::_ZPX_},
		{ 0xE4 /*CPX*/, 3, &Cpu6502::Instruction_CompareXRegister, AddressingMode::ZP},
		{ 0xE5 /*SBC*/, 3, &Cpu6502::Instruction_SubtractWithCarry, AddressingMode::ZP},
		{ 0xE6 /*INC*/, 5, &Cpu6502::Instruction_IncrementMemory, AddressingMode::ZP},
		{ 0xE8 /*INY*/, 2, &Cpu6502::Instruction_IncrementX, AddressingMode::IMP},
		{ 0xE9 /*SBC*/, 2, &Cpu6502::Instruction_SubtractWithCarry, AddressingMode::IMM},
		{ 0xEA /*NOP*/, 2, &Cpu6502::Instruction_Noop, AddressingMode::IMP},
		{ 0xEC /*CPX*/, 4, &Cpu6502::Instruction_CompareXRegister, AddressingMode::ABS},
		{ 0xED /*SBC*/, 4, &Cpu6502::Instruction_SubtractWithCarry, AddressingMode::ABS},
		{ 0xEE /*INC*/, 6, &Cpu6502::Instruction_IncrementMemory, AddressingMode::ABS},
		{ 0xF0 /*BEQ*/, 2, &Cpu6502::Instruction_BranchOnEqual, AddressingMode::IMP},
		{ 0xF1 /*SBC*/, 5, &Cpu6502::Instruction_SubtractWithCarry, AddressingMode::_ZP_Y},
		{ 0xF5 /*SBC*/, 4, &Cpu6502::Instruction_SubtractWithCarry, AddressingMode::ZPX},
		{ 0xF6 /*INC*/, 6, &Cpu6502::Instruction_IncrementMemory, AddressingMode::ZPX},
		{ 0xF8 /*SED*/, 2, &Cpu6502::Instruction_SetDecimal, AddressingMode::IMP},
		{ 0xF9 /*SBC*/, 4, &Cpu6502::Instruction_SubtractWithCarry, AddressingMode::ABSY},
		{ 0xFD /*SBC*/, 4, &Cpu6502::Instruction_SubtractWithCarry, AddressingMode::ABSX},
		{ 0xFE /*INC*/, 7, &Cpu6502::Instruction_IncrementMemory, AddressingMode::ABSX},
	};

	auto iter = std::lower_bound(std::begin(s_opCodeTable), std::end(s_opCodeTable), opCode, [](const OpCodeTableEntry& entry, uint8_t opCode)
	{
		return entry.opCode < opCode;
	});

	if (iter == std::end(s_opCodeTable) || iter->opCode != opCode)
		throw UnhandledInstruction(opCode);
	else
		return &*iter;
}


void Cpu6502::Instruction_Unhandled(AddressingMode /*addressingMode*/)
{
	throw InvalidInstruction(0);
}

void Cpu6502::Instruction_Noop(AddressingMode /*addressingMode*/)
{
	// no-op
}

void Cpu6502::Instruction_Break(AddressingMode /*addressingMode*/)
{
	m_pc++;
	GenerateNonMaskableInterrupt();
}

void Cpu6502::Instruction_LoadAccumulator(AddressingMode addressingMode)
{
	m_acc = ReadUInt8(addressingMode);
	SetStatusFlagsFromValue(m_acc);
}

void Cpu6502::Instruction_LoadX(AddressingMode addressingMode)
{
	m_x = ReadUInt8(addressingMode);
	SetStatusFlagsFromValue(m_x);
}

void Cpu6502::Instruction_LoadY(AddressingMode addressingMode)
{
	m_y = ReadUInt8(addressingMode);
	SetStatusFlagsFromValue(m_y);
}

void Cpu6502::Instruction_StoreAccumulator(AddressingMode addressingMode)
{
	uint16_t writeOffset = GetAddressingModeOffset_ReadWrite(addressingMode);
	WriteMemory8(writeOffset, m_acc);
}

void Cpu6502::Instruction_Compare(AddressingMode addressingMode)
{
	CompareValues(m_acc, ReadUInt8(addressingMode));
}

void Cpu6502::Instruction_CompareXRegister(AddressingMode addressingMode)
{
	CompareValues(m_x, ReadUInt8(addressingMode));
}

void Cpu6502::Instruction_CompareYRegister(AddressingMode addressingMode)
{
	CompareValues(m_y, ReadUInt8(addressingMode));
}

void Cpu6502::Instruction_TestBits(AddressingMode addressingMode)
{
	uint8_t val = ReadUInt8(addressingMode);
	CpuStatusFlag resultStatusFlags = CpuStatusFlag::None;
	if ((val & m_acc) == 0)
		resultStatusFlags |= CpuStatusFlag::Zero;

	if ((val & 0x80) != 0)
		resultStatusFlags |= CpuStatusFlag::Negative;
	if ((val & 0x40) != 0)
		resultStatusFlags |= CpuStatusFlag::Overflow;

	SetStatusFlags(resultStatusFlags, CpuStatusFlag::Zero | CpuStatusFlag::Negative | CpuStatusFlag::Overflow);
}


void Cpu6502::Instruction_And(AddressingMode addressingMode)
{
	const uint8_t memValue = ReadUInt8(addressingMode);
	m_acc = memValue & m_acc;
	SetStatusFlagsFromValue(m_acc);
}

void Cpu6502::Instruction_OrWithAccumulator(AddressingMode addressingMode)
{
	const uint8_t memValue = ReadUInt8(addressingMode);
	m_acc = memValue | m_acc;
	SetStatusFlagsFromValue(m_acc);
}

void Cpu6502::Instruction_ExclusiveOr(AddressingMode addressingMode)
{
	const uint8_t memValue = ReadUInt8(addressingMode);
	m_acc = memValue ^ m_acc;
	SetStatusFlagsFromValue(m_acc);
}

void Cpu6502::Instruction_AddWithCarry(AddressingMode addressingMode)
{
	const uint8_t memValue = ReadUInt8(addressingMode);
	AddWithCarry(m_acc, memValue);
}

void Cpu6502::Instruction_SubtractWithCarry(AddressingMode addressingMode)
{
	static bool shouldUsedOnesComplementAddition = true;

	if (shouldUsedOnesComplementAddition)
	{
		// Subtraction can be performed by taking the ones complement of the subtrahend and then performing an add
		const uint8_t m = ReadUInt8(addressingMode);
		AddWithCarry(m_acc, ~m);
	}
	else
	{
		// REVIEW: This whole method is also gross
		const uint8_t m = ReadUInt8(addressingMode);
		const int8_t signedM = static_cast<int8_t>(m);
		const int8_t signedAcc = static_cast<int8_t>(m_acc);

		int wideResult = static_cast<int>(signedAcc) - static_cast<int>(signedM) - (((m_status & static_cast<uint8_t>(CpuStatusFlag::Carry)) != 0) ? 0 : 1);
		//int narrowResult = signedM + signedAcc + (((m_status & static_cast<uint8_t>(CpuStatusFlag::Carry)) != 0) ? 1 : 0);

		m_acc = m_acc - m - (((m_status & static_cast<uint8_t>(CpuStatusFlag::Carry)) != 0) ? 0 : 1); // TODO: + Carry
		SetStatusFlagsFromValue(m_acc);

		// Borrow (!Carry) happens when the unsigned subtraction would become negative
		//bool hasBorrow

		CpuStatusFlag newStatusFlags = CpuStatusFlag::None;

		if (wideResult < -128 || wideResult > 127) // Oh shit, an overflow!
		{
			newStatusFlags |= CpuStatusFlag::Overflow;
		}

		// Carry happens if two negative numbers go super negative, or if a negative number and a positive become positive
		if (wideResult < -128
			|| ((signedM < 0 || signedAcc < 0) && wideResult >= 0))
		{
			newStatusFlags |= CpuStatusFlag::Carry;
		}

		SetStatusFlags(newStatusFlags, CpuStatusFlag::Overflow | CpuStatusFlag::Carry);
	}
}

void Cpu6502::Instruction_RotateLeft(AddressingMode addressingMode)
{
	ReadModifyWriteUint8(addressingMode, [this](uint8_t& value)
	{
		const bool oldCarrySet = (m_status & static_cast<uint8_t>(CpuStatusFlag::Carry)) != 0;
		const bool oldBit7Set = (value & 0x80) != 0;
		value <<= 1;

		if (oldCarrySet)
			value |= 0x01;

		SetStatusFlagsFromValue(value);
		SetStatusFlags(oldBit7Set ? CpuStatusFlag::Carry : CpuStatusFlag::None, CpuStatusFlag::Carry);
	});
}

void Cpu6502::Instruction_RotateRight(AddressingMode addressingMode)
{
	ReadModifyWriteUint8(addressingMode, [this](uint8_t& value)
	{
		const bool oldCarrySet = (m_status & static_cast<uint8_t>(CpuStatusFlag::Carry)) != 0;
		const bool oldBit0Set = (value & 0x01) != 0;
		value >>= 1;

		if (oldCarrySet)
			value |= 0x80;

		SetStatusFlagsFromValue(value);
		SetStatusFlags(oldBit0Set ? CpuStatusFlag::Carry : CpuStatusFlag::None, CpuStatusFlag::Carry);
	});
}

void Cpu6502::Instruction_ArithmeticShiftLeft(AddressingMode addressingMode)
{
	ReadModifyWriteUint8(addressingMode, [this](uint8_t& value)
	{
		bool oldBit7Set = (value & 0x80) != 0;
		value <<= 1;
		SetStatusFlagsFromValue(value);
		SetStatusFlags(oldBit7Set ? CpuStatusFlag::Carry : CpuStatusFlag::None, CpuStatusFlag::Carry);
	});
}

void Cpu6502::Instruction_LogicalShiftRight(AddressingMode addressingMode)
{
	ReadModifyWriteUint8(addressingMode, [this](uint8_t& value)
	{
		bool oldBit0Set = (value & 0x01) != 0;
		value >>= 1;
		SetStatusFlagsFromValue(value);
		SetStatusFlags(oldBit0Set ? CpuStatusFlag::Carry : CpuStatusFlag::None, CpuStatusFlag::Carry);
	});
}

void Cpu6502::Instruction_DecrementX(AddressingMode /*addressingMode*/)
{
	SetStatusFlagsFromValue(--m_x); // Doesn't touch overflow
}

void Cpu6502::Instruction_IncrementX(AddressingMode /*addressingMode*/)
{
	SetStatusFlagsFromValue(++m_x); // Doesn't touch overflow
}

void Cpu6502::Instruction_DecrementY(AddressingMode /*addressingMode*/)
{
	SetStatusFlagsFromValue(--m_y); // Doesn't touch overflow
}

void Cpu6502::Instruction_IncrementY(AddressingMode /*addressingMode*/)
{
	SetStatusFlagsFromValue(++m_y); // Doesn't touch overflow
}

void Cpu6502::Instruction_DecrementMemory(AddressingMode addressingMode)
{
	ReadModifyWriteUint8(addressingMode, [this](uint8_t& value)
	{
		--value;
		SetStatusFlagsFromValue(value);
	});
}

void Cpu6502::Instruction_IncrementMemory(AddressingMode addressingMode)
{
	ReadModifyWriteUint8(addressingMode, [this](uint8_t& value)
	{
		++value;
		SetStatusFlagsFromValue(value);
	});
}

void Cpu6502::Instruction_SetInterrupt(AddressingMode /*addressingMode*/)
{
	SetStatusFlags(CpuStatusFlag::InterruptDisabled, CpuStatusFlag::InterruptDisabled); // Set Interrupt Disable
}

void Cpu6502::Instruction_ClearInterrupt(AddressingMode /*addressingMode*/)
{
	SetStatusFlags(CpuStatusFlag::None, CpuStatusFlag::InterruptDisabled); // Clear Interrupt Disable
}

void Cpu6502::Instruction_SetDecimal(AddressingMode /*addressingMode*/)
{
	SetStatusFlags(CpuStatusFlag::DecimalMode, CpuStatusFlag::DecimalMode); // Set Decimal Mode
}

void Cpu6502::Instruction_ClearDecimal(AddressingMode /*addressingMode*/)
{
	SetStatusFlags(CpuStatusFlag::None, CpuStatusFlag::DecimalMode); // Clear Decimal Mode
}

void Cpu6502::Instruction_SetCarry(AddressingMode /*addressingMode*/)
{
	SetStatusFlags(CpuStatusFlag::Carry, CpuStatusFlag::Carry); // Set Carry Flag
}

void Cpu6502::Instruction_ClearCarry(AddressingMode /*addressingMode*/)
{
	SetStatusFlags(CpuStatusFlag::None, CpuStatusFlag::Carry); // Clear Carry Flag
}

void Cpu6502::Instruction_ClearOverflow(AddressingMode /*addressingMode*/)
{
	SetStatusFlags(CpuStatusFlag::None, CpuStatusFlag::Overflow); // Clear Overflow Flag
}

void Cpu6502::Instruction_TransferXToStack(AddressingMode /*addressingMode*/)
{
	m_sp = m_x;
}

void Cpu6502::Instruction_TransferStackToX(AddressingMode /*addressingMode*/)
{
	m_x = m_sp;
	SetStatusFlagsFromValue(m_x);
}

void Cpu6502::Instruction_PushAccumulator(AddressingMode /*addressingMode*/)
{
	PushValueOntoStack8(m_acc);
}

void Cpu6502::Instruction_PullAccumulator(AddressingMode /*addressingMode*/)
{
	m_acc = ReadValueFromStack8();
	SetStatusFlagsFromValue(m_acc);
}

void Cpu6502::Instruction_PushProcessorStatus(AddressingMode /*addressingMode*/)
{
	// PHP pushes the cpu status with the break status bit set (http://visual6502.org/wiki/index.php?title=6502_BRK_and_B_bit)
	PushValueOntoStack8(m_status | static_cast<uint8_t>(CpuStatusFlag::BreakCommand));
}

void Cpu6502::Instruction_PullProcessorStatus(AddressingMode /*addressingMode*/)
{
	const uint8_t statusLoadMask = static_cast<uint8_t>(CpuStatusFlag::BreakCommand | CpuStatusFlag::Bit5);
	const uint8_t statusFromStack = ReadValueFromStack8();
	m_status = (m_status & statusLoadMask) | (statusFromStack & ~statusLoadMask);
}

void Cpu6502::Instruction_BranchOnEqual(AddressingMode /*addressingMode*/)
{
	Helper_ExecuteBranch((m_status & static_cast<uint8_t>(CpuStatusFlag::Zero)) != 0);
}

void Cpu6502::Instruction_BranchOnNotEqual(AddressingMode /*addressingMode*/)
{
	Helper_ExecuteBranch((m_status & static_cast<uint8_t>(CpuStatusFlag::Zero)) == 0);
}

void Cpu6502::Instruction_BranchOnPlus(AddressingMode /*addressingMode*/)
{
	Helper_ExecuteBranch((m_status & static_cast<uint8_t>(CpuStatusFlag::Negative)) == 0);
}

void Cpu6502::Instruction_BranchOnMinus(AddressingMode /*addressingMode*/)
{
	Helper_ExecuteBranch((m_status & static_cast<uint8_t>(CpuStatusFlag::Negative)) != 0);
}

void Cpu6502::Instruction_BranchOnOverflowClear(AddressingMode /*addressingMode*/)
{
	Helper_ExecuteBranch((m_status & static_cast<uint8_t>(CpuStatusFlag::Overflow)) == 0);
}

void Cpu6502::Instruction_BranchOnOverflowSet(AddressingMode /*addressingMode*/)
{
	Helper_ExecuteBranch((m_status & static_cast<uint8_t>(CpuStatusFlag::Overflow)) != 0);
}

void Cpu6502::Instruction_BranchOnCarryClear(AddressingMode /*addressingMode*/)
{
	Helper_ExecuteBranch((m_status & static_cast<uint8_t>(CpuStatusFlag::Carry)) == 0);
}

void Cpu6502::Instruction_BranchOnCarrySet(AddressingMode /*addressingMode*/)
{
	Helper_ExecuteBranch((m_status & static_cast<uint8_t>(CpuStatusFlag::Carry)) != 0);
}

void Cpu6502::Helper_ExecuteBranch(bool shouldBranch)
{
	const int8_t relativeOffset = static_cast<int8_t>(ReadMemory8(m_pc++));
	if (shouldBranch)
	{
		AddCycles(1);

		// If our jump target is on another page, the instruction takes an additional cycle
		if ((m_pc + relativeOffset) >> 8 != m_pc >> 8)
			AddCycles(1);

		m_pc += relativeOffset;
	}
}

void Cpu6502::Instruction_JumpToSubroutine(AddressingMode addressingMode)
{
	uint16_t jumpAddress = ReadUInt16(addressingMode);
	PushValueOntoStack16(m_pc - 1);
	m_pc = jumpAddress;
}

void Cpu6502::Instruction_ReturnFromSubroutine(AddressingMode /*addressingMode*/)
{
	const uint16_t returnAddress = ReadValueFromStack16() + 1;
	m_pc = returnAddress;
}

void Cpu6502::Instruction_ReturnFromInterrupt(AddressingMode /*addressingMode*/)
{
	const uint8_t statusLoadMask = static_cast<uint8_t>(CpuStatusFlag::BreakCommand | CpuStatusFlag::Bit5);
	const uint8_t statusFromStack = ReadValueFromStack8();
	uint16_t returnAddress = ReadValueFromStack16();

	m_status = (m_status & statusLoadMask) | (statusFromStack & ~statusLoadMask);
	m_pc = returnAddress;
}

void Cpu6502::Instruction_Jump(AddressingMode addressingMode)
{
	m_pc = ReadUInt16(addressingMode);
}

void Cpu6502::Instruction_JumpIndirect(AddressingMode addressingMode)
{
	// REVIEW: eww, gross
	// Dealing with the fact that the 16-bit address can't cross pages, so we need some awkward 
	// modulo math
	uint16_t jumpAddressIndirect = ReadUInt16(addressingMode);
	uint16_t jumpAddress = ReadMemory8(jumpAddressIndirect);
	jumpAddressIndirect = (jumpAddressIndirect & 0xFF00) | ((jumpAddressIndirect + 1) & 0x00FF);
	jumpAddress = jumpAddress | (ReadMemory8(jumpAddressIndirect) << 8);
	m_pc = jumpAddress;
}

void Cpu6502::Instruction_StoreX(AddressingMode addressingMode)
{
	uint16_t writeOffset = GetAddressingModeOffset_ReadWrite(addressingMode);
	WriteMemory8(writeOffset, m_x);
}

void Cpu6502::Instruction_StoreY(AddressingMode addressingMode)
{
	uint16_t writeOffset = GetAddressingModeOffset_ReadWrite(addressingMode);
	WriteMemory8(writeOffset, m_y);
}

void Cpu6502::Instruction_TransferAtoX(AddressingMode /*addressingMode*/)
{
	m_x = m_acc;
	SetStatusFlagsFromValue(m_x);
}

void Cpu6502::Instruction_TransferXtoA(AddressingMode /*addressingMode*/)
{
	m_acc = m_x;
	SetStatusFlagsFromValue(m_acc);
}

void Cpu6502::Instruction_TransferAtoY(AddressingMode /*addressingMode*/)
{
	m_y = m_acc;
	SetStatusFlagsFromValue(m_y);
}

void Cpu6502::Instruction_TransferYtoA(AddressingMode /*addressingMode*/)
{
	m_acc = m_y;
	SetStatusFlagsFromValue(m_acc);
}

void Cpu6502::AddCycles(uint32_t cycles)
{
	m_currentInstructionCycleCount += cycles;
}


int64_t Cpu6502::GetElapsedCycles() const
{
	return m_totalCycles - m_cyclesRemaining;
}


uint32_t Cpu6502::RunInstructions(int targetCycles)
{
	m_cyclesRemaining += targetCycles;
	m_totalCycles += targetCycles;
	uint32_t totalRunCycles = 0;

	while (m_cyclesRemaining > 0)
	{
		m_pMapper->SetTick(m_totalCycles - m_cyclesRemaining);

		uint8_t instruction = ReadMemory8(m_pc++);

		auto opCodeEntry = DoOpcodeStuff(instruction);
		m_currentInstructionCycleCount = opCodeEntry->baseCycles;
		((*this).*(opCodeEntry->func))(opCodeEntry->addrMode);

		m_cyclesRemaining -= m_currentInstructionCycleCount;
		totalRunCycles += m_currentInstructionCycleCount;
	}

	return totalRunCycles;
}


uint32_t Cpu6502::RunNextInstruction()
{
	m_pMapper->SetTick(m_totalCycles);

	// Instruction is of the form aaabbbcc.  See: http://www.llx.com/~nparker/a2/opcodes.html
	uint8_t instruction = ReadMemory8(m_pc++);

	auto opCodeEntry = DoOpcodeStuff(instruction);
	m_currentInstructionCycleCount = opCodeEntry->baseCycles;
	((*this).*(opCodeEntry->func))(opCodeEntry->addrMode);

	m_totalCycles += m_currentInstructionCycleCount;

	return m_currentInstructionCycleCount;
}


} // namespace CPU
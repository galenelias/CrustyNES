#include "pch.h"
#include "Cpu6502.h"

#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace CPU
{

// BranchFlagSelector
enum BranchFlagSelector
{
	Negative = 0x0,
	Overflow = 0x1,
	Carry    = 0x2,
	Zero     = 0x3,
};

class InvalidInstruction : public std::runtime_error
{
public:
	InvalidInstruction(uint8_t instruction)
		: m_instruction(instruction)
		, std::runtime_error("Invalid Opcode")
	{ }

private:
	uint8_t m_instruction;
};

class UnhandledInstruction: public std::runtime_error
{
public:
	UnhandledInstruction(uint8_t instruction)
		: m_instruction(instruction)
		, std::runtime_error("Unhandled Opcode")
	{ }

private:
	uint8_t m_instruction;
};


Cpu6502::Cpu6502(const NES::NESRom& rom)
{
	m_prgRom = rom.GetPrgRom();

	// Set ourselves into perpetual v-blank mode:
	m_ppuStatusReg = static_cast<uint8_t>(PpuStatusFlag::InVBlank);
}

uint8_t Cpu6502::ReadMemory8(uint16_t offset) const
{
	return *MapMemoryOffset(offset);
}

uint16_t Cpu6502::ReadMemory16(uint16_t offset) const
{
	return *reinterpret_cast<const uint16_t*>(MapMemoryOffset(offset));
}

const byte* Cpu6502::MapMemoryOffset(uint16_t offset) const
{
	if (offset >= 0x8000) // PRG ROM
	{
		// For now, only support NROM mapper NES-NROM-128 (iNes Mapper 0)
		if (offset >= 0xC000)
			return m_prgRom + (offset - 0xC000);
		else
			return m_prgRom + (offset - 0x8000);
	}
	else if (offset < 0x800) // CPU RAM
	{
		return m_cpuRam + offset;
	}
	else if (offset == 0x2002)
	{
		return &m_ppuStatusReg;
	}
	else
	{
		return nullptr;
	}
}


byte* Cpu6502::MapWritableMemoryOffset(uint16_t offset)
{
	if (offset < 0x800) // CPU RAM
	{
		return m_cpuRam + offset;
	}
	else if (offset == 0x2000)
	{
		return &m_ppuCtrlReg1;
	}
	else if (offset == 0x2001)
	{
		return &m_ppuCtrlReg2;
	}
	//else if (offset == 0x2002)
	//{
	//	return &m_ppuStatusReg;
	//}
	else
	{
		return nullptr;
	}

}


void Cpu6502::WriteMemory8(uint16_t offset, uint8_t val)
{
	byte* pWritableMemory = MapWritableMemoryOffset(offset);
	*pWritableMemory = val;
}


void Cpu6502::Reset()
{
	// Jump to code offset specified by the RESET thingy
	m_pc = ReadMemory16(static_cast<uint16_t>(0xFFFC));
	m_sp = 0xFD; // REVIEW: Figure out why normal NES starts at FD, not FF.  Initial push of status flags?

	// REVIEW: Push something on to stack?  Initial stack pointer should be FD
	
	m_status = static_cast<uint8_t>(CpuStatusFlag::Bit5 | CpuStatusFlag::InterruptDisabled); // Bit 5 doesn't exist, so pin it in the '1' state.  Not sure why interrupts start disabled

	// This doesn't play nice with nestest for some reason, so force execution to start at $C000
	m_pc = 0xc000;
}

uint8_t Cpu6502::ReadUInt8(AddressingMode mode, uint8_t instruction)
{
	if (mode == AddressingMode::IMM)
	{
		return ReadMemory8(m_pc++);
	}
	else if (mode == AddressingMode::ABS)
	{
		uint16_t readOffset = ReadMemory16(m_pc);
		m_pc += 2;
		return ReadMemory8(readOffset);
	}
	else if (mode == AddressingMode::ABSX)
	{
		uint16_t readOffset = ReadMemory16(m_pc);
		m_pc += 2;
		readOffset += m_x;
		return ReadMemory8(readOffset);
	}
	else if (mode == AddressingMode::ABSY)
	{
		uint16_t readOffset = ReadMemory16(m_pc);
		m_pc += 2;
		readOffset += m_y;
		return ReadMemory8(readOffset);
	}
	else if (mode == AddressingMode::ZP)  // REVIEW: De-dupe with ReadUInt16
	{
		uint8_t zpOffset = ReadMemory8(m_pc++);
		return ReadMemory8(zpOffset);
	}
	else if (mode == AddressingMode::ZPX)
	{
		uint8_t zpOffset = ReadMemory8(m_pc++);
		zpOffset += m_x;
		return ReadMemory8(zpOffset);
	}
	else if (mode == AddressingMode::ZPY)
	{
		uint8_t zpOffset = ReadMemory8(m_pc++);
		zpOffset += m_y;
		return ReadMemory8(zpOffset);
	}
	else if (mode == AddressingMode::ACC)
	{
		return m_acc;
	}
	else if (mode == AddressingMode::_ZPX_)
	{
		uint16_t indirectOffset = GetIndexedIndirectOffset();
		return ReadMemory8(indirectOffset);
	}
	else if (mode == AddressingMode::_ZP_Y)
	{
		uint16_t indirectOffset = GetIndirectIndexedOffset();
		return ReadMemory8(indirectOffset);
	}
	else
	{
		throw UnhandledInstruction(instruction);
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

uint16_t Cpu6502::GetIndirectIndexedOffset()
{
	uint8_t zpOffset = ReadMemory8(m_pc++);

	// We have to make sure to continue zero page indexing for the 16-bit offset so we wrap at $00FF
	uint8_t indirectOffsetLow = ReadMemory8(zpOffset++);
	uint8_t indirectOffsetHigh = ReadMemory8(zpOffset);
	uint16_t indirectOffset = (indirectOffsetHigh << 8) | indirectOffsetLow;
	indirectOffset += m_y;
	return indirectOffset;
}


uint16_t Cpu6502::ReadUInt16(AddressingMode mode, uint8_t instruction)
{
	if (mode == AddressingMode::ABS)
	{
		uint16_t value = ReadMemory16(m_pc);
		m_pc += 2;
		return value;
	}
	else if (mode == AddressingMode::ZPX)
	{
		return ReadMemory16(GetAddressingModeOffset(mode, instruction));
	}
	else if (mode == AddressingMode::ZP)
	{
		return ReadMemory16(GetAddressingModeOffset(mode, instruction));
	}
	else
	{
		throw UnhandledInstruction(instruction);
	}
}

uint8_t* Cpu6502::GetReadWriteAddress(AddressingMode mode, uint8_t instruction)
{
	if (mode == AddressingMode::ACC)
	{
		return &m_acc;
	}
	else
	{
		uint16_t memoryOffset = GetAddressingModeOffset(mode, instruction);
		return MapWritableMemoryOffset(memoryOffset);
	}
}

uint16_t Cpu6502::GetAddressingModeOffset(AddressingMode mode, uint8_t instruction)
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
		return GetIndirectIndexedOffset();
	}
	else
	{
		throw UnhandledInstruction(instruction);
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
	const uint16_t c_stackOffset = 0x100;
	m_cpuRam[c_stackOffset + m_sp--] = val;
}


void Cpu6502::PushValueOntoStack16(uint16_t val)
{
	PushValueOntoStack8(val >> 8); // Store high word
	PushValueOntoStack8(static_cast<uint8_t>(val)); // Store high word
}


uint8_t Cpu6502::ReadValueFromStack8()
{
	const uint16_t c_stackOffset = 0x100;

	uint8_t result = m_cpuRam[c_stackOffset + (++m_sp)];
	return result;
}

uint16_t Cpu6502::ReadValueFromStack16()
{
	uint8_t lowWord = ReadValueFromStack8();
	uint8_t highWord = ReadValueFromStack8();
	return (highWord << 8) | lowWord;
}


std::wstring Cpu6502::GetDebugState() const
{
	uint8_t instruction = ReadMemory8(m_pc);

	std::wostringstream oss;  // note the 'w'

	oss << std::hex << std::setfill(L'0');
	oss << std::setw(4) << m_pc << "  ";
	oss << std::setw(2) << instruction << "   ";
	oss << L"A:" << std::setw(2) << m_acc << "  ";
	oss << L"X:" << std::setw(2) << m_x << "  ";
	oss << L"Y:" << std::setw(2) << m_y << "  ";
	oss << L"P:" << std::setw(2) << m_status << "  ";
	oss << L"SP:" << std::setw(2) << m_sp;// << "  ";
	//oss << L"SL:" << std::setw(6) << m_status;

	return oss.str();
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
	int wideResult = static_cast<int>(signedX) + static_cast<int>(signedY) + (isCarryBitSet ? 1 : 0);

	// Calculate the actual result by performing 8-bit addition
	uint8_t result = x + y + (isCarryBitSet ? 1 : 0);
	SetStatusFlagsFromValue(result);
	
	CpuStatusFlag newStatusFlags = CpuStatusFlag::None;

	if (wideResult < -128 || wideResult > 127) // Signed overflow
		newStatusFlags |= CpuStatusFlag::Overflow;
	if (IsNegative(result))
		newStatusFlags |= CpuStatusFlag::Negative;
	if (result == 0)
		newStatusFlags |= CpuStatusFlag::Zero;

	if (wideResult < -128 || ((signedX < 0 || signedY < 0) && wideResult >= 0))
	{
		// Carry happens if two negative numbers go super negative, or if a negative number and a positive become positive
		newStatusFlags |= CpuStatusFlag::Carry;
	}

	SetStatusFlags(newStatusFlags, CpuStatusFlag::Overflow | CpuStatusFlag::Carry | CpuStatusFlag::Negative | CpuStatusFlag::Zero);
	m_acc = result;
}



void Cpu6502::RunNextInstruction()
{
	// Instruction is of the form aaabbbcc.  See: http://www.llx.com/~nparker/a2/opcodes.html
	uint8_t instruction = ReadMemory8(m_pc++);

	// 6502 instructions are split into multiple distinct classes of instructions as indicated by the 'cc' component
	const uint8_t instructionClass = (instruction & 0x03);

	if (instructionClass == 0x01)  // REVIEW: Combine into single enum?
	{
		const AddressingMode addressingMode = GetClass1AddressingMode(instruction);
		const OpCode1 opCode = static_cast<OpCode1>((instruction & 0xE0) >> 5);

		switch (opCode)
		{
		case OpCode1::LDA:
			m_acc = ReadUInt8(addressingMode, instruction);
			SetStatusFlagsFromValue(m_acc);
			break;

		case OpCode1::STA: // Store accumulator
		{
			uint16_t writeOffset = GetAddressingModeOffset(addressingMode, instruction);
			WriteMemory8(writeOffset, m_acc);
			break;
		}

		case OpCode1::CMP: // Compare
		{
			CompareValues(m_acc, ReadUInt8(addressingMode, instruction));
			break;
		}
		case OpCode1::AND: // Logical AND
		{
			const uint8_t m = ReadUInt8(addressingMode, instruction);
			m_acc = m & m_acc;
			SetStatusFlagsFromValue(m_acc);
			break;
		}
		case OpCode1::ORA: // Logical OR
		{
			const uint8_t m = ReadUInt8(addressingMode, instruction);
			m_acc = m | m_acc;
			SetStatusFlagsFromValue(m_acc);
			break;
		}
		case OpCode1::EOR: // Logical OR
		{
			const uint8_t m = ReadUInt8(addressingMode, instruction);
			m_acc = m ^ m_acc;
			SetStatusFlagsFromValue(m_acc);
			break;
		}
		case OpCode1::ADC: // Add with carry
		{
			const uint8_t m = ReadUInt8(addressingMode, instruction);
			AddWithCarry(m_acc, m);

			break;
		}
		case OpCode1::SBC: // Subtract with carry
		{
			static bool shouldUsedOnesComplementAddition = true;

			if (!shouldUsedOnesComplementAddition)
			{
				// REVIEW: This whole method is also gross
				const uint8_t m = ReadUInt8(addressingMode, instruction);
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
			else
			{
				// Subtraction can be performed by taking the ones complement of the subtrahend and then performing an add
				const uint8_t m = ReadUInt8(addressingMode, instruction);
				AddWithCarry(m_acc, ~m);
			}

			break;
		}

		default:
			throw UnhandledInstruction(instruction);
		}
	}
	else if (instructionClass == 0x00)
	{
		// Branch operations
		if ((instruction & 0x1F) == 0x10)
		{
			// Conditional branch, of the form xxy1 0000
			const int8_t relativeOffset = static_cast<int8_t>(ReadMemory8(m_pc++));

			const BranchFlagSelector flagSelector = static_cast<BranchFlagSelector>(instruction >> 6); // xx
			bool flagVal = false;
			if (flagSelector == BranchFlagSelector::Negative)
				flagVal = (m_status & static_cast<uint8_t>(CpuStatusFlag::Negative)) != 0;
			else if (flagSelector == BranchFlagSelector::Overflow)
				flagVal = (m_status & static_cast<uint8_t>(CpuStatusFlag::Overflow)) != 0;
			else if (flagSelector == BranchFlagSelector::Carry)
				flagVal = (m_status & static_cast<uint8_t>(CpuStatusFlag::Carry)) != 0;
			else if (flagSelector == BranchFlagSelector::Zero)
				flagVal = (m_status & static_cast<uint8_t>(CpuStatusFlag::Zero)) != 0;

			// Expected value is stored in bit 6 of the instruction (y)
			const bool expectedValue = (instruction & 0x20) != 0;
			const bool shouldBranch = (flagVal == expectedValue);

			if (shouldBranch)
			{
				m_pc += relativeOffset;
			}
		}
		else if (instruction == SingleByteInstructions::INX)
		{
			SetStatusFlagsFromValue(++m_x); // Doesn't touch overflow
		}
		else if (instruction == SingleByteInstructions::INY)
		{
			SetStatusFlagsFromValue(++m_y); // Doesn't touch overflow
		}
		else if (instruction == SingleByteInstructions::DEY)
		{
			SetStatusFlagsFromValue(--m_y); // Doesn't touch overflow
		}
		else if (instruction == SingleByteInstructions::SEI)
		{
			SetStatusFlags(CpuStatusFlag::InterruptDisabled, CpuStatusFlag::InterruptDisabled); // Set Interrupt Disable
		}
		else if (instruction == SingleByteInstructions::CLI)
		{
			SetStatusFlags(CpuStatusFlag::None, CpuStatusFlag::InterruptDisabled); // Clear Interrupt Disable
		}
		else if (instruction == SingleByteInstructions::CLD)
		{
			SetStatusFlags(CpuStatusFlag::None, CpuStatusFlag::DecimalMode); // Clear Decimal Mode
		}
		else if (instruction == SingleByteInstructions::SED)
		{
			SetStatusFlags(CpuStatusFlag::DecimalMode, CpuStatusFlag::DecimalMode); // Set Decimal Mode
		}
		else if (instruction == SingleByteInstructions::CLC)
		{
			SetStatusFlags(CpuStatusFlag::None, CpuStatusFlag::Carry); // Clear Carry Flag
		}
		else if (instruction == SingleByteInstructions::SEC)
		{
			SetStatusFlags(CpuStatusFlag::Carry, CpuStatusFlag::Carry); // Set Carry Flag
		}
		else if (instruction == SingleByteInstructions::CLV)
		{
			SetStatusFlags(CpuStatusFlag::None, CpuStatusFlag::Overflow); // Clear Overflow Flag
		}
		else if (instruction == SingleByteInstructions::JSR)
		{
			uint16_t jumpAddress = ReadUInt16(AddressingMode::ABS, instruction);
			PushValueOntoStack16(m_pc - 1);
			m_pc = jumpAddress;
		}
		else if (instruction == SingleByteInstructions::RTS)
		{
			uint16_t returnAddress = ReadValueFromStack16() + 1;
			m_pc = returnAddress;
		}
		else if (instruction == SingleByteInstructions::PHP) // Push status
		{
			// PHP pushes the cpu status with the break status bit set (http://visual6502.org/wiki/index.php?title=6502_BRK_and_B_bit)
			PushValueOntoStack8(m_status | static_cast<uint8_t>(CpuStatusFlag::BreakCommand));
		}
		else if (instruction == SingleByteInstructions::PHA) // Push accumulator
		{
			PushValueOntoStack8(m_acc);
		}
		else if (instruction == SingleByteInstructions::PLA) // Pull accumulator
		{
			m_acc = ReadValueFromStack8();
			SetStatusFlagsFromValue(m_acc);
		}
		else if (instruction == SingleByteInstructions::PLP)
		{
			// PLP doesn't set the 'Break' status flag (http://visual6502.org/wiki/index.php?title=6502_BRK_and_B_bit)
			const uint8_t statusLoadMask = static_cast<uint8_t>(CpuStatusFlag::BreakCommand | CpuStatusFlag::Bit5);
			const uint8_t statusFromStack = ReadValueFromStack8();
			m_status = (m_status & statusLoadMask) | (statusFromStack & ~statusLoadMask);
		}
		else if (instruction == SingleByteInstructions::TAY)
		{
			m_y = m_acc;
			SetStatusFlagsFromValue(m_y);
		}
		else if (instruction == SingleByteInstructions::TYA)
		{
			m_acc = m_y;
			SetStatusFlagsFromValue(m_acc);
		}
		else if (instruction == SingleByteInstructions::RTI)
		{
			const uint8_t statusLoadMask = static_cast<uint8_t>(CpuStatusFlag::BreakCommand | CpuStatusFlag::Bit5);
			const uint8_t statusFromStack = ReadValueFromStack8();
			uint16_t returnAddress = ReadValueFromStack16();

			m_status = (m_status & statusLoadMask) | (statusFromStack & ~statusLoadMask);
			m_pc = returnAddress;
		}
		else
		{
			const AddressingMode addressingMode = GetClass0AddressingMode(instruction);
			const OpCode0 opCode = static_cast<OpCode0>((instruction & 0xE0) >> 5);

			switch (opCode)
			{
			case OpCode0::JMP:
			{
				m_pc = ReadUInt16(addressingMode, instruction);
				break;
			}
			case OpCode0::JMP_ABS:
			{
				// REVIEW: eww, gross
				// Dealing with the fact that the 16-bit address can't cross pages, so we need some awkward 
				// modulo math
				uint16_t jumpAddressIndirect = ReadUInt16(addressingMode, instruction);
				uint16_t jumpAddress = ReadMemory8(jumpAddressIndirect);
				jumpAddressIndirect = (jumpAddressIndirect & 0xFF00) | ((jumpAddressIndirect + 1) & 0x00FF);
				jumpAddress = jumpAddress | (ReadMemory8(jumpAddressIndirect) << 8);
				m_pc = jumpAddress;
				break;
			}
			case OpCode0::LDY:
			{
				m_y = ReadUInt8(addressingMode, instruction);
				SetStatusFlagsFromValue(m_y);
				break;
			}
			case OpCode0::STY:
			{
				uint16_t writeOffset = GetAddressingModeOffset(addressingMode, instruction);
				WriteMemory8(writeOffset, m_y);
				break;
			}
			case OpCode0::CPX:
			{
				CompareValues(m_x, ReadUInt8(addressingMode, instruction));
				break;
			}
			case OpCode0::CPY:
			{
				CompareValues(m_y, ReadUInt8(addressingMode, instruction));
				break;
			}
			case OpCode0::BIT:
			{
				uint8_t val = ReadUInt8(addressingMode, instruction);
				CpuStatusFlag resultStatusFlags = CpuStatusFlag::None;
				if ((val & m_acc) == 0)
					resultStatusFlags |= CpuStatusFlag::Zero;

				if ((val & 0x80) != 0)
					resultStatusFlags |= CpuStatusFlag::Negative;
				if ((val & 0x40) != 0)
					resultStatusFlags |= CpuStatusFlag::Overflow;

				SetStatusFlags(resultStatusFlags, CpuStatusFlag::Zero | CpuStatusFlag::Negative | CpuStatusFlag::Overflow);
				break;
			}
			default:
				throw UnhandledInstruction(instruction);
			}
		}
	}
	else if (instructionClass == 0x02)
	{
		if (instruction == (SingleByteInstructions::TXS))
		{
			m_sp = m_x;
		}
		else if (instruction == SingleByteInstructions::DEX)
		{
			SetStatusFlagsFromValue(--m_x); // Doesn't touch overflow
		}
		else if (instruction == SingleByteInstructions::TAX)
		{
			m_x = m_acc;
			SetStatusFlagsFromValue(m_x);
		}
		else if (instruction == SingleByteInstructions::TXA)
		{
			m_acc = m_x;
			SetStatusFlagsFromValue(m_acc);
		}
		else if (instruction == SingleByteInstructions::TSX)
		{
			m_x = m_sp;
			SetStatusFlagsFromValue(m_x);
		}
		else if (instruction == SingleByteInstructions::NOP)
		{
			// no-op
		}
		else
		{
			AddressingMode addressingMode = GetClass2AddressingMode(instruction);
			const OpCode2 opCode = static_cast<OpCode2>((instruction & 0xE0) >> 5);

			switch (opCode)
			{
			case OpCode2::LDX:
			{
				if (addressingMode == AddressingMode::ZPX)
					addressingMode = AddressingMode::ZPY;
				else if (addressingMode == AddressingMode::ABSX)
					addressingMode  = AddressingMode::ABSY;

				m_x = ReadUInt8(addressingMode, instruction);
				SetStatusFlagsFromValue(m_x);
				break;
			}
			case OpCode2::STX:
			{
				if (addressingMode == AddressingMode::ZPX)
					addressingMode = AddressingMode::ZPY;

				uint16_t writeOffset = GetAddressingModeOffset(addressingMode, instruction);
				WriteMemory8(writeOffset, m_x);
				break;
			}
			case OpCode2::LSR:  // Logical Shift Right
			{
				uint8_t& val = *GetReadWriteAddress(addressingMode, instruction);
				bool oldBit0Set = (val & 0x01) != 0;
				val >>= 1;
				SetStatusFlagsFromValue(val);
				SetStatusFlags(oldBit0Set ? CpuStatusFlag::Carry : CpuStatusFlag::None, CpuStatusFlag::Carry);
				break;
			}
			case OpCode2::ASL: // Arithmetic Shift Left
			{
				uint8_t& val = *GetReadWriteAddress(addressingMode, instruction);
				bool oldBit7Set = (val & 0x80) != 0;
				val <<= 1;
				SetStatusFlagsFromValue(val);
				SetStatusFlags(oldBit7Set ? CpuStatusFlag::Carry : CpuStatusFlag::None, CpuStatusFlag::Carry);
				break;
			}
			case OpCode2::ROR:
			{
				uint8_t& val = *GetReadWriteAddress(addressingMode, instruction);
				bool oldCarrySet = (m_status & static_cast<uint8_t>(CpuStatusFlag::Carry)) != 0;
				bool oldBit0Set = (val & 0x01) != 0;
				val >>= 1;

				if (oldCarrySet)
					val |= 0x80;

				SetStatusFlagsFromValue(val);
				SetStatusFlags(oldBit0Set ? CpuStatusFlag::Carry : CpuStatusFlag::None, CpuStatusFlag::Carry);
				break;
			}
			case OpCode2::ROL:
			{
				uint8_t& val = *GetReadWriteAddress(addressingMode, instruction);
				bool oldCarrySet = (m_status & static_cast<uint8_t>(CpuStatusFlag::Carry)) != 0;
				bool oldBit7Set = (val & 0x80) != 0;
				val <<= 1;

				if (oldCarrySet)
					val |= 0x01;

				SetStatusFlagsFromValue(val);
				SetStatusFlags(oldBit7Set ? CpuStatusFlag::Carry : CpuStatusFlag::None, CpuStatusFlag::Carry);
				break;
			}
			case OpCode2::INC:
			{
				uint8_t& val = *GetReadWriteAddress(addressingMode, instruction);
				++val;
				SetStatusFlagsFromValue(val);
				break;
			}
			case OpCode2::DEC:
			{
				uint8_t& val = *GetReadWriteAddress(addressingMode, instruction);
				--val;
				SetStatusFlagsFromValue(val);
				break;
			}
			default:
				throw UnhandledInstruction(instruction);
			}
		}
	}
	else
	{
		throw UnhandledInstruction(instruction);
	}

}


} // namespace CPU
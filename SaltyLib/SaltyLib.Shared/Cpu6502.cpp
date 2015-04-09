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
	m_pc = ReadMemory16(0xFFFC);
	m_sp = 0xFF;

	// REVIEW: Push something on to stack?  Initial stack pointer should be FD
	
	m_status = 0x24; // Set Zero & Overflow

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
	else if (mode == AddressingMode::ZP)  // REVIEW: De-dupe with ReadUInt16
	{
		uint8_t zpOffset = ReadMemory8(m_pc++);
		return ReadMemory8(zpOffset);
	}
	else
	{
		throw UnhandledInstruction(instruction);
	}

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

uint16_t Cpu6502::GetAddressingModeOffset(AddressingMode mode, uint8_t instruction)
{
	if (mode == AddressingMode::ABS)
	{
		uint16_t value = ReadMemory16(m_pc);
		m_pc += 2;
		return value;
	}
	else if (mode == AddressingMode::ZPX)
	{
		uint8_t zpOffset = ReadMemory8(m_pc++);
		uint8_t memoryOffset = zpOffset + m_x;
		return memoryOffset;
	}
	else if (mode == AddressingMode::ZP)
	{
		uint8_t zpOffset = ReadMemory8(m_pc++);
		return static_cast<uint16_t>(zpOffset);
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
	const uint16_t c_stackOffset = 0x100;
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
	oss << std::setw(2) << instruction << "  ";
	oss << L"A:" << std::setw(2) << m_acc << "  ";
	oss << L"X:" << std::setw(2) << m_x << "  ";
	oss << L"Y:" << std::setw(2) << m_y << "  ";
	oss << L"P:" << std::setw(2) << m_status << "  ";
	oss << L"SP:" << std::setw(2) << m_sp << "  ";
	//oss << L"SL:" << std::setw(6) << m_status;

	return oss.str();
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
			// REVIEW: A-M, how to set Carry/Negative flags?
			const int8_t m = static_cast<int8_t>(ReadUInt8(addressingMode, instruction));
			const int8_t a = static_cast<int8_t>(m_acc);

			CpuStatusFlag newFlags = CpuStatusFlag::None;

			if (a >= m) // Carry flags
				newFlags = newFlags | CpuStatusFlag::Carry;
			if (m == a)
				newFlags = newFlags | CpuStatusFlag::Zero;
			if (a - m < 0)
				newFlags = newFlags | CpuStatusFlag::Negative;

			SetStatusFlags(newFlags, CpuStatusFlag::Carry | CpuStatusFlag::Zero | CpuStatusFlag::Negative);
			break;
		}
		case OpCode1::AND: // Logical AND
		{
			const uint8_t m = ReadUInt8(addressingMode, instruction);
			const uint8_t r = m & m_acc;
			SetStatusFlagsFromValue(r);
			break;
		}
		case OpCode1::ORA: // Logical OR
		{
			const uint8_t m = ReadUInt8(addressingMode, instruction);
			const uint8_t r = m | m_acc;
			SetStatusFlagsFromValue(r);
			break;
		}
		case OpCode1::EOR: // Logical OR
		{
			const uint8_t m = ReadUInt8(addressingMode, instruction);
			const uint8_t r = m ^ m_acc;
			SetStatusFlagsFromValue(r);
			break;
		}
		case OpCode1::ADC: // Add with carry
		{
			const uint8_t m = ReadUInt8(addressingMode, instruction);
			const uint8_t r = m + m_acc; // TODO: + Carry
			m_acc = r;
			SetStatusFlagsFromValue(r);
			// TODO: Set carry
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
			bool flagVal;
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
			m_x++;
			SetStatusFlagsFromValue(m_x); // Doesn't touch overflow
		}
		else if (instruction == SingleByteInstructions::INY)
		{
			m_y++;
			SetStatusFlagsFromValue(m_y); // Doesn't touch overflow
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
		else if (instruction == SingleByteInstructions::PHP)
		{
			PushValueOntoStack8(m_status);
		}
		else if (instruction == SingleByteInstructions::PHA)
		{
			PushValueOntoStack8(m_acc);
		}
		else if (instruction == SingleByteInstructions::PLA)
		{
			m_acc = ReadValueFromStack8();
			SetStatusFlagsFromValue(m_acc);
		}
		else if (instruction == SingleByteInstructions::PLP)
		{
			m_status = ReadValueFromStack8();
		}
		else
		{
			const AddressingMode addressingMode = GetClass0AddressingMode(instruction);
			const OpCode0 opCode = static_cast<OpCode0>((instruction & 0xE0) >> 5);

			switch (opCode)
			{
			case OpCode0::JMP:
				m_pc = ReadUInt16(addressingMode, instruction);
				break;
			case OpCode0::STY:
			{
				uint16_t writeOffset = GetAddressingModeOffset(addressingMode, instruction);
				WriteMemory8(writeOffset, m_y);
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
		else if (instruction == SingleByteInstructions::NOP)
		{
			// no-op
		}
		else
		{
			const AddressingMode addressingMode = GetClass2AddressingMode(instruction);
			const OpCode2 opCode = static_cast<OpCode2>((instruction & 0xE0) >> 5);

			switch (opCode)
			{
			case OpCode2::LDX:
				m_x = ReadUInt8(addressingMode, instruction);
				SetStatusFlagsFromValue(m_x);
				break;
			case OpCode2::STX:
			{
				uint16_t writeOffset = GetAddressingModeOffset(addressingMode, instruction);
				WriteMemory8(writeOffset, m_x);
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
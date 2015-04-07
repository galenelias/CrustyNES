#include "pch.h"
#include "Cpu6502.h"

#include <stdexcept>

namespace CPU
{


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
}

uint8_t Cpu6502::ReadMemory8(uint16_t offset)
{
	return *MapMemoryOffset(offset);
}

uint16_t Cpu6502::ReadMemory16(uint16_t offset)
{
	return *reinterpret_cast<const uint16_t*>(MapMemoryOffset(offset));
}

const byte* Cpu6502::MapMemoryOffset(uint16_t offset)
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
	// Jump to code offset specified by 
	m_pc = ReadMemory16(0xFFFC);
}


void Cpu6502::RunNextInstruction()
{
	uint8_t instruction = ReadMemory8(m_pc++);

	const OpCode opCode = static_cast<OpCode>((instruction & 0xE0) >> 5);
	const AddressingMode addressingMode = static_cast<AddressingMode>((instruction & 0x1C) >> 2);

	if ((instruction & 0x03) != 0x01)
		throw UnhandledInstruction(instruction);

	switch (opCode)
	{
	case OpCode::LDA:

		if (addressingMode == AddressingMode::IMM)
		{
			m_accumulator = ReadMemory8(m_pc++);
		}
		else if (addressingMode == AddressingMode::ABS)
		{
			uint16_t readOffset = ReadMemory16(m_pc);
			m_pc += 2;
			m_accumulator = ReadMemory8(readOffset);
		}
		else
		{
			throw UnhandledInstruction(instruction);
		}

		break;

	case OpCode::STA: // Store accumulator

		if (addressingMode == AddressingMode::ABS)
		{
			uint16_t writeOffset = ReadMemory16(m_pc);
			m_pc += 2;

			WriteMemory8(writeOffset, m_accumulator);
		}
		else
		{
			throw UnhandledInstruction(instruction);
		}

		break;

	}

}




} // namespace CPU
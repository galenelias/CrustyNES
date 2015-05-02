#pragma once

#include <stdint.h>
#include <string>

#include "NESRom.h"
#include "CoreUtils.h"

namespace PPU
{
	class Ppu;
}

namespace NES
{
	class NES;
}

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


enum class SingleByteInstructions
{
	INY = 0xC8, // Increment Y
	INX = 0xE8, // Increment Y
	CLI = 0x58, // Clear Interrupt Disable
	SEI = 0x78, // Set Interrupt Disable
	CLC = 0x18, // Clear Carry Flag
	SEC = 0x38, // Set Carry Flag
	CLD = 0xD8, // Clear Decimal Mode
	SED = 0xF8, // Set Decimal Mode
	TXA = 0x8A, // Transfer X to Accumulator
	TXS = 0x9A, // Transfer X to Stack Pointer
	TAX = 0xAA, // Transfer Accumulator to X
	TSX = 0xBA, // Transfer Stack Pointer to X
	DEX = 0xCA, // Decrement X Register
	JSR = 0x20, // Jump to Subroutine
	RTS = 0x60, // Return from Subroutine
	PHP = 0x08, // Push Processor Status
	PLA = 0x68, // Pull Accumulator
	NOP = 0xEA, // No operation
	PHA = 0x48, // Push Accumulator
	PLP = 0x28, // Pull Process Status
	CLV = 0xB8, // Clear Overflow
	DEY = 0x88, // Decrement Y
	TAY = 0xA8, // Transfer Accumulator to Y
	TYA = 0x98, // Transfer Y to Accumulator
	RTI = 0x40, // Return from Interrupt
};

inline bool operator==(uint8_t left, SingleByteInstructions right)
{
	return left == static_cast<uint8_t>(right);
}


enum class OpCode0
{
	BIT     = 0x1, // 001
	JMP     = 0x2, // 010
	JMP_ABS = 0x3, // 011
	STY     = 0x4, // 100
	LDY     = 0x5, // 101
	CPY     = 0x6, // 110
	CPX     = 0x7, // 111
};


enum class OpCode1
{
	ORA = 0x0, // 000
	AND = 0x1, // 001
	EOR = 0x2, // 010
	ADC = 0x3, // 011
	STA = 0x4, // 100
	LDA = 0x5, // 101
	CMP = 0x6, // 110
	SBC = 0x7, // 111
};

enum class OpCode2
{
	ASL = 0x0, // 000
	ROL = 0x1, // 001
	LSR = 0x2, // 010 - Logical Shift Right
	ROR = 0x3, // 011
	STX = 0x4, // 100
	LDX = 0x5, // 101
	DEC = 0x6, // 110
	INC = 0x7, // 111
};


enum class AddressingMode
{
	IMM,   // Immediate
	ABS,   // Absolute
	ABSX,  // Absolute, X
	ABSY,  // Absolute, Y
	ZP,    // Zero Page
	ZPX,   // Zero Page, X
	ZPY,   // Zero Page, Y
	_ZPX_, // (Zero Page, X)
	_ZP_Y, // (Zero Page), Y
	ACC,   // Accumulator
};

enum class AddressingModeClass0
{
	_ZPX_ = 0x0, // 000  (Zero Page, X)
	ZP    = 0x1, // 001  Zero Page
	IMM   = 0x2, // 010  Immediate
	ABS   = 0x3, // 011  Absolute
	_ZP_Y = 0x4, // 100  (Zero Page), Y
	ZPX   = 0x5, // 101  Zero Page, X
	ABSY  = 0x6, // 110  Absolute, Y
	ABSX  = 0x7, // 111  Absolute, X
};

enum class AddressingModeClass2
{
	_ZPX_ = 0x0, // 000  (Zero Page, X)
	ZP    = 0x1, // 001  Zero Page
	IMM   = 0x2, // 010  Immediate
	ABS   = 0x3, // 011  Absolute
	_ZP_Y = 0x4, // 100  (Zero Page), Y
	ZPX   = 0x5, // 101  Zero Page, X
	ABSY  = 0x6, // 110  Absolute, Y
	ABSX  = 0x7, // 111  Absolute, X
};

enum class CpuStatusFlag
{
	None = 0x00,
	Carry = 0x01,
	Zero = 0x02,
	InterruptDisabled = 0x04,
	DecimalMode = 0x08,
	BreakCommand = 0x10,
	Bit5 = 0x20,
	Overflow = 0x40,
	Negative = 0x80,
};

DEFINE_ENUM_BITWISE_OPERANDS(CpuStatusFlag);


// Memory Regions
//  Interrupts ($FFFA-$FFFF)
//   - $FFFC-$FFFD - RESET

class Cpu6502
{
public:
	Cpu6502(NES::NES& nes); //REVIEW: Should cpu depend on ram, or abstract the PRG/CHR loading?
	//Cpu6502(PPU::Ppu& ppu); //REVIEW: Should cpu depend on ram, or abstract the PRG/CHR loading?
	Cpu6502(const Cpu6502&) = delete;
	Cpu6502& operator=(const Cpu6502&) = delete;

	void Reset();

	void MapRomMemory(const NES::NESRom& rom);

	void RunNextInstruction();

	std::string GetDebugState() const;

	uint16_t GetProgramCounter() const { return m_pc; }

	void GenerateNonMaskableInterrupt();

private:
	// Read stuff
	uint8_t ReadMemory8(uint16_t offset) const;
	uint16_t ReadMemory16(uint8_t /*offset*/) const { throw std::runtime_error("Oh shit"); }
	uint16_t ReadMemory16(uint16_t offset) const;

	// Addressing mode resolution
	byte* GetReadWriteAddress(AddressingMode mode, uint8_t instruction);

	uint8_t ReadUInt8(AddressingMode mode, uint8_t instruction);
	uint16_t ReadUInt16(AddressingMode mode, uint8_t instruction);
	uint16_t GetAddressingModeOffset(AddressingMode mode, uint8_t instruction);

	uint16_t GetIndexedIndirectOffset();
	uint16_t GetIndirectIndexedOffset();

	// Write stuff
	byte* MapWritableMemoryOffset(uint16_t offset);
	void WriteMemory8(uint16_t offset, uint8_t val);

	// Stack stuff
	void PushValueOntoStack8(uint8_t val);
	void PushValueOntoStack16(uint16_t val);
	uint16_t ReadValueFromStack16();
	uint8_t ReadValueFromStack8();

	// Status flag
	void SetStatusFlagsFromValue(uint8_t value);
	void SetStatusFlags(CpuStatusFlag flags, CpuStatusFlag mask);

	// Random instruction helpers
	void CompareValues(uint8_t minuend, uint8_t subtrahend);
	void AddWithCarry(uint8_t val1, uint8_t val2);

	void HandleInvalidInstruction(AddressingMode addressingMode, uint8_t instruction);

	// REVIEW: Simulate memory bus?
	byte m_cpuRam[2*1024 /*2KB*/];
	const byte *m_prgRom;
	uint16_t m_cbPrgRom;

	NES::NES& m_nes;
	PPU::Ppu& m_ppu;

	// PPU stuff
	uint8_t m_ppuCtrlReg1;
	uint8_t m_ppuCtrlReg2;
	uint8_t m_ppuStatusReg;

	// CPU Registers
	uint16_t m_pc; // Program counter
	uint8_t m_sp; // stack pointer
	uint8_t m_acc;
	uint8_t m_x; // Index Register X
	uint8_t m_y; // Index Register Y
	uint8_t m_status; // (P) processor status (NV.BDIZC) (N)egative,o(V)erflow,(B)reak,(D)ecimal,(I)nterrupt disable, (Z)ero Flag
};

}
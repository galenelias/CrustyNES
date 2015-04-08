#pragma once

#include <stdint.h>
#include <string>

#include "NESRom.h"

namespace CPU
{

enum class SingleByteInstructions
{
	INY = 0xC8, // Increment Y
	INX = 0xE8, // Increment Y
	CLI = 0x58, // Clear Interrupt Disable
	SEI = 0x78, // Set Interrupt Disable
	CLD = 0xD8, // Clear Decimal Mode
	TXA = 0x8A, // Transfer X to Accumulator
	TXS = 0x9A, // Transfer X to Stack Pointer
	TAX = 0xAA, // Transfer Accumulator to X
	TSX = 0xBA, // Transfer Stack Pointer to X
	DEX = 0xCA, // Decrement X Register
	NOP = 0xEA, // No operation
};

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
	LSR = 0x2, // 010
	ROR = 0x3, // 011
	STX = 0x4, // 100
	LDX = 0x5, // 101
	DEC = 0x6, // 110
	INC = 0x7, // 111
};


//enum class OpCode00
//{
//	ORA = 0x0, // 000
//	AND = 0x1, // 001
//	EOR = 0x2, // 010
//	ADC = 0x3, // 011
//	STA = 0x4, // 100
//	LDA = 0x5, // 101
//	CMP = 0x6, // 110
//	SBC = 0x7, // 111
//};


enum class AddressingMode
{
	IMM,   // Immediate
	ABS,   // Absolute
	ABSY,  // Absolute, Y
	ABSX,  // Absolute, X
	ZP,    // Zero Page
	ZPX,   // Zero Page, X
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
	Overflow = 0x40,
	Negative = 0x80,
};

inline CpuStatusFlag operator&(CpuStatusFlag field1, CpuStatusFlag field2)
{
	return static_cast<CpuStatusFlag>(static_cast<uint8_t>(field1) & static_cast<uint8_t>(field2));
}

inline CpuStatusFlag operator|(CpuStatusFlag field1, CpuStatusFlag field2)
{
	return static_cast<CpuStatusFlag>(static_cast<uint8_t>(field1) | static_cast<uint8_t>(field2));
}


enum class PpuStatusFlag
{
	Bit0 = 0x01,
	Bit1 = 0x02,
	Bit2 = 0x04,
	Bit3 = 0x08,
	Bit4 = 0x10,
	Bit5 = 0x20,
	Bit6 = 0x40,
	InVBlank = 0x80,
};




// Memory Regions
//  Interrupts ($FFFA-$FFFF)
//   - $FFFC-$FFFD - RESET


class Cpu6502
{
public:
	Cpu6502(const NES::NESRom& rom); //REVIEW: Should cpu depend on ram, or abstract the PRG/CHR loading?
	void Reset();

	void RunNextInstruction();

	std::wstring GetDebugState() const;

private:
	const byte* MapMemoryOffset(uint16_t offset) const;
	uint8_t ReadMemory8(uint16_t offset) const;
	uint16_t ReadMemory16(uint16_t offset) const;

	byte* MapWritableMemoryOffset(uint16_t offset);
	void WriteMemory8(uint16_t offset, uint8_t val);

	//const byte* ReadMemoryWithAddressingMode(AddressingMode mode)
	uint8_t ReadUInt8(AddressingMode mode, uint8_t instruction);
	uint16_t ReadUInt16(AddressingMode mode, uint8_t instruction);

	void SetStatusFlagsFromValue(uint8_t value);
	void SetStatusFlags(CpuStatusFlag flags, CpuStatusFlag mask);

	// REVIEW: Simulate memory bus?
	byte m_cpuRam[2*1024 /*2KB*/];
	const byte *m_prgRom;

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

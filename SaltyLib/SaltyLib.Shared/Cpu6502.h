#pragma once

#include <stdint.h>

#include "NESRom.h"

namespace CPU
{

	
enum class OpCode
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

enum class AddressingMode
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


// Memory Regions
//  Interrupts ($FFFA-$FFFF)
//   - $FFFC-$FFFD - RESET


class Cpu6502
{
public:
	Cpu6502(const NES::NESRom& rom); //REVIEW: Should cpu depend on ram, or abstract the PRG/CHR loading?
	void Reset();

	void RunNextInstruction();

private:
	const byte* MapMemoryOffset(uint16_t offset);
	uint8_t ReadMemory8(uint16_t offset);
	uint16_t ReadMemory16(uint16_t offset);

	 byte* MapWritableMemoryOffset(uint16_t offset);
	void WriteMemory8(uint16_t offset, uint8_t val);

	//const byte* ReadMemoryWithAddressingMode(AddressingMode mode)

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
	uint8_t m_accumulator;
	uint8_t m_x; // Index Register X
	uint8_t m_y; // Index Register Y
	uint8_t m_status; // (P) processor status (NV.BDIZC) (N)egative,o(V)erflow,(B)reak,(D)ecimal,(I)nterrupt disable, (Z)ero Flag
};

}

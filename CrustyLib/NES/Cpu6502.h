#pragma once

#include <stdint.h>
#include <string>

#include "NESRom.h"
#include "..\Util\CoreUtils.h"

namespace PPU
{
	class Ppu;
}

namespace NES
{
	class NES;

	namespace APU
	{
		class IApu;
	}
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
	IMP,   // Implied (not a real addressing mode)
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
	Cpu6502(const Cpu6502&) = delete;
	Cpu6502& operator=(const Cpu6502&) = delete;

	void SetRomMapper(NES::IMapper* pMapper);

	void Reset();

	uint32_t RunNextInstruction();
	uint32_t RunInstructions(int targetCycles);

	int64_t GetElapsedCycles() const;

	const char* GetDebugState() const;
	uint16_t GetProgramCounter() const { return m_pc; }

	void GenerateNonMaskableInterrupt();

	//enum class OpCode : uint16_t;
	typedef void (Cpu6502::*InstrunctionFunc)(AddressingMode addressingMode);
	struct OpCodeTableEntry
	{
		uint8_t opCode;
		uint16_t baseCycles;
		InstrunctionFunc func;
		AddressingMode addrMode;
	};
private:

	OpCodeTableEntry* DoOpcodeStuff(uint8_t opCode);

	void Instruction_Unhandled(AddressingMode addressingMode);
	void Instruction_Noop(AddressingMode addressingMode);
	void Instruction_Break(AddressingMode addressingMode);

	void Instruction_LoadAccumulator(AddressingMode addressingMode);
	void Instruction_LoadX(AddressingMode addressingMode);
	void Instruction_LoadY(AddressingMode addressingMode);
	void Instruction_StoreAccumulator(AddressingMode addressingMode);
	void Instruction_StoreX(AddressingMode addressingMode);
	void Instruction_StoreY(AddressingMode addressingMode);
	void Instruction_Compare(AddressingMode addressingMode);
	void Instruction_CompareXRegister(AddressingMode addressingMode);
	void Instruction_CompareYRegister(AddressingMode addressingMode);
	void Instruction_TestBits(AddressingMode addressingMode);
	void Instruction_And(AddressingMode addressingMode);
	void Instruction_OrWithAccumulator(AddressingMode addressingMode);
	void Instruction_ExclusiveOr(AddressingMode addressingMode);
	void Instruction_AddWithCarry(AddressingMode addressingMode);
	void Instruction_SubtractWithCarry(AddressingMode addressingMode);
	void Instruction_RotateLeft(AddressingMode addressingMode);
	void Instruction_RotateRight(AddressingMode addressingMode);
	void Instruction_ArithmeticShiftLeft(AddressingMode addressingMode);
	void Instruction_LogicalShiftRight(AddressingMode addressingMode);
	void Instruction_DecrementX(AddressingMode addressingMode);
	void Instruction_IncrementX(AddressingMode addressingMode);
	void Instruction_DecrementY(AddressingMode addressingMode);
	void Instruction_IncrementY(AddressingMode addressingMode);
	void Instruction_TransferAtoX(AddressingMode addressingMode);
	void Instruction_TransferXtoA(AddressingMode addressingMode);
	void Instruction_TransferAtoY(AddressingMode addressingMode);
	void Instruction_TransferYtoA(AddressingMode addressingMode);
	void Instruction_SetInterrupt(AddressingMode addressingMode);
	void Instruction_ClearInterrupt(AddressingMode addressingMode);
	void Instruction_SetDecimal(AddressingMode addressingMode);
	void Instruction_ClearDecimal(AddressingMode addressingMode);
	void Instruction_SetCarry(AddressingMode addressingMode);
	void Instruction_ClearCarry(AddressingMode addressingMode);
	void Instruction_ClearOverflow(AddressingMode addressingMode);
	void Instruction_TransferXToStack(AddressingMode addressingMode);
	void Instruction_TransferStackToX(AddressingMode addressingMode);
	void Instruction_PushAccumulator(AddressingMode addressingMode);
	void Instruction_PullAccumulator(AddressingMode addressingMode);
	void Instruction_PushProcessorStatus(AddressingMode addressingMode);
	void Instruction_PullProcessorStatus(AddressingMode addressingMode);
	void Instruction_BranchOnPlus(AddressingMode addressingMode);
	void Instruction_BranchOnMinus(AddressingMode addressingMode);
	void Instruction_BranchOnOverflowClear(AddressingMode addressingMode);
	void Instruction_BranchOnOverflowSet(AddressingMode addressingMode);
	void Instruction_BranchOnCarryClear(AddressingMode addressingMode);
	void Instruction_BranchOnCarrySet(AddressingMode addressingMode);
	void Instruction_BranchOnNotEqual(AddressingMode addressingMode);
	void Instruction_BranchOnEqual(AddressingMode addressingMode);
	void Instruction_DecrementMemory(AddressingMode addressingMode);
	void Instruction_IncrementMemory(AddressingMode addressingMode);
	void Instruction_JumpToSubroutine(AddressingMode addressingMode);
	void Instruction_ReturnFromSubroutine(AddressingMode addressingMode);
	void Instruction_ReturnFromInterrupt(AddressingMode addressingMode);
	void Instruction_Jump(AddressingMode addressingMode);
	void Instruction_JumpIndirect(AddressingMode addressingMode);

	void Helper_ExecuteBranch(bool shouldBranch);

	void AddCycles(uint32_t cycles);

	// Read stuff
	uint8_t ReadMemory8(uint16_t offset) const;
	uint16_t ReadMemory16(uint8_t /*offset*/) const { throw std::runtime_error("Oh shit"); }
	uint16_t ReadMemory16(uint16_t offset) const;

	// Addressing mode resolution
	template <typename Func>
	void ReadModifyWriteUint8(AddressingMode mode, Func func);

	uint8_t ReadUInt8(AddressingMode mode);
	uint16_t ReadUInt16(AddressingMode mode);
	uint16_t GetAddressingModeOffset_Read(AddressingMode mode);
	uint16_t GetAddressingModeOffset_ReadWrite(AddressingMode mode);

	uint16_t GetIndexedIndirectOffset();
	uint16_t GetIndirectIndexedOffset_Read();
	uint16_t GetIndirectIndexedOffset_ReadWrite();

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

	// REVIEW: Simulate memory bus?
	byte m_cpuRam[2*1024 /*2KB*/];

	NES::NES& m_nes;
	PPU::Ppu& m_ppu;
	NES::APU::IApu& m_apu;
	NES::IMapper* m_pMapper;

	// PPU stuff
	uint8_t m_ppuCtrlReg1;
	uint8_t m_ppuCtrlReg2;
	uint8_t m_ppuStatusReg;

	uint32_t m_currentInstructionCycleCount = 0;
	//uint64_t m_totalCycles = 0;
	int64_t m_totalCycles = 0;
	int64_t m_cyclesRemaining = 0;

	// CPU Registers
	uint16_t m_pc = 0; // Program counter
	uint8_t m_sp = 0; // stack pointer
	uint8_t m_acc = 0;
	uint8_t m_x = 0; // Index Register X
	uint8_t m_y = 0; // Index Register Y
	uint8_t m_status; // (P) processor status (NV.BDIZC) (N)egative,o(V)erflow,(B)reak,(D)ecimal,(I)nterrupt disable, (Z)ero Flag
};

}

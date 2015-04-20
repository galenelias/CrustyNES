#pragma once

#include <stdint.h>
#include <memory>

typedef unsigned char byte;

class IReadableFile
{
public:
	virtual ~IReadableFile() {}
	virtual void Read(size_t cbRead, _Out_writes_bytes_(cbRead) byte* pBuffer) = 0;

};

class InvalidRomFormatException;

namespace NES
{

class NESROMHeader
{
public:
	void LoadFromFile(IReadableFile* pRomFile);

	bool UseTrainer() const { return (m_Flags6 & 0x04) != 0; }
	uint16_t CbPrgRomData() const { return m_cbPRGRom * 16 * 1024; }
	uint16_t CbChrRomData() const { return m_cbCHRRom * 8 * 1024; }

	byte MapperNumber() { return ((m_Flags6 & 0xF0) >> 4) | (m_Flags7 & 0x0f); }

private:
	uint8_t m_cbPRGRom;
	uint8_t m_cbCHRRom;
	byte m_Flags6;
	byte m_Flags7;
	uint32_t m_cbPRGRam;
	byte m_Flags9;
	byte m_Flags10;

	//enum class Flags6Values : byte
	//{
	//	VerticalMirroring = 0x01,
	//	HorizontalMirroring = 0x
	//};

};


class NESRom
{
public:
	NESRom();
	~NESRom();

	void LoadRomFromFile(IReadableFile* pRomFile);

	uint16_t GetCbPrgRom() const;
	const byte* GetPrgRom() const;

	bool HasChrRom() const;
	const byte* GetChrRom() const;
	uint16_t CbChrRomData() const { return m_header.CbChrRomData(); }

private:
	NESROMHeader m_header;
	std::unique_ptr<byte[]> m_prgRomData;
	std::unique_ptr<byte[]> m_chrRomData;
};


} // namespace NES


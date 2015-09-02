#include "pch.h"
#include "NESRom.h"

#include <stdexcept>

namespace NES
{


class InvalidRomFormatException : public std::runtime_error
{
public:
	InvalidRomFormatException(const std::string& errorString)
		: std::runtime_error(errorString)
	{ }
};


class UnsupportedRomException : public std::runtime_error
{
public:
	UnsupportedRomException(const std::string& errorString)
		: std::runtime_error(errorString)
	{ }
};


void NESROMHeader::LoadFromFile(IReadableFile* pRomFile)
{
	const char c_NESCookie[] = "NES\x01A";
	const uint32_t  c_cbNESCookie = 4;

	byte readBuffer[16];
	pRomFile->Read(_countof(readBuffer), readBuffer);

	if (0 != memcmp(readBuffer, c_NESCookie, c_cbNESCookie))
		throw InvalidRomFormatException("Invalid ROM Header");

	this->m_cbPRGRom = readBuffer[4];
	this->m_cbCHRRom = readBuffer[5];
	this->m_Flags6 = readBuffer[6];
	this->m_Flags7 = readBuffer[7];
	this->m_cbPRGRam = readBuffer[8];
	this->m_Flags9 = readBuffer[9];
	this->m_Flags10 = readBuffer[10];
}

PPU::MirroringMode NESROMHeader::GetMirroringMode() const
{
	const uint8_t mirrorBits = (m_Flags6 & 0x09);

	if (mirrorBits == 0x00)
		return PPU::MirroringMode::HorizontalMirroring;
	else if (mirrorBits == 0x01)
		return PPU::MirroringMode::VerticalMirroring;
	else //if (mirrorBits & 0x08)
		return PPU::MirroringMode::FourScreen;
}

void NESRom::LoadRomFromFile(IReadableFile* pRomFile)
{
	m_header.LoadFromFile(pRomFile);

	if (m_header.UseTrainer())
		throw UnsupportedRomException("Not Supported: Trainers");

	const auto cbPrgRomData = m_header.CbPrgRomData();
	m_prgRomData = std::make_unique<byte[]>(cbPrgRomData);
	pRomFile->Read(cbPrgRomData, m_prgRomData.get());

	const auto cbChrRomData = m_header.CbChrRomData();
	if (cbChrRomData > 0)
	{
		m_chrRomData = std::make_unique<byte[]>(cbChrRomData);
		pRomFile->Read(cbChrRomData, m_chrRomData.get());
	}
		
}

uint8_t NESRom::GetMapperId() const
{
	return m_header.MapperNumber();
}


uint32_t NESRom::GetCbPrgRom() const
{
	return m_header.CbPrgRomData();
}

const byte* NESRom::GetPrgRom() const
{
	return m_prgRomData.get();
}

bool NESRom::HasChrRom() const
{
	return m_chrRomData != nullptr;
}

const byte* NESRom::GetChrRom() const
{
	return m_chrRomData.get();
}



} // namespace NES


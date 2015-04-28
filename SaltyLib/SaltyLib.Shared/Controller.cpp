#include "pch.h"
#include "Controller.h"

namespace NES
{


void Controller::SetInputStatus(ControllerInput input, bool isPressed)
{
	m_inputs[static_cast<size_t>(input)] = isPressed ? 1 : 0;
}


void Controller::WriteData(uint8_t value)
{
	m_strobeOn = (value & 0x1) != 0;
	
	if (m_strobeOn)
		m_readInputOffset = 0;
}


uint8_t Controller::ReadData()
{
	uint8_t result = m_readInputOffset < (int)ControllerInput::_Max ? m_inputs[m_readInputOffset] : 0;

	if (!m_strobeOn)
	{
		m_readInputOffset++;
	}

	return result;
}

}
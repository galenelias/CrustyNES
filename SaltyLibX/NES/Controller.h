#pragma once

#include <stdint.h>

namespace NES
{

enum class ControllerInput
{
	A = 0,
	B = 1,
	Select = 2,
	Start = 3,
	Up = 4,
	Down = 5,
	Left = 6,
	Right = 7,
	_Max,
};

class Controller
{
public:
	void SetInputStatus(ControllerInput input, bool isPressed);

	void WriteData(uint8_t value);
	uint8_t ReadData();

private:
	bool m_strobeOn = false;
	int m_readInputOffset = 0;

	uint8_t m_inputs[static_cast<size_t>(ControllerInput::_Max)];

};

}

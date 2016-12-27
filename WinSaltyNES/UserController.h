#pragma once

#include "NES/Controller.h"
#include <initializer_list>

class Controller
{
public:
	void SetInputStatus(NES::ControllerInput input, bool isPressed);
	bool GetInputStatus(NES::ControllerInput input) const;

	void PushControllerState(NES::Controller& controller) const;

private:
	uint8_t m_inputs[static_cast<size_t>(NES::ControllerInput::_Max)];
};


class XInputGamepadController : public Controller
{
public:
	XInputGamepadController(int controllerNumber = 1)
		: m_controllerOffset(controllerNumber)
	{ }

	bool Poll();

private:
	int m_controllerOffset;
};

class Win32KeyboardController : public Controller
{
public:
	bool TranlateWindowsMessage(UINT msg, WPARAM wParam);

};

void PushAggregateControllerState(const std::initializer_list<Controller>& controllers, NES::Controller& nesController);

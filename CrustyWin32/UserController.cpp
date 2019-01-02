#include "stdafx.h"

#include "UserController.h"
#include "NES/Controller.h"

#include <Xinput.h>

void Controller::SetInputStatus(NES::ControllerInput input, bool isPressed)
{
	m_inputs[static_cast<size_t>(input)] = isPressed ? 1 : 0;
}


bool Controller::GetInputStatus(NES::ControllerInput input) const
{
	return !!m_inputs[static_cast<size_t>(input)];
}


void Controller::PushControllerState(NES::Controller& controller) const
{
	for (size_t i = 0; i < _countof(m_inputs); ++i)
	{
		controller.SetInputStatus(static_cast<NES::ControllerInput>(i), !!m_inputs[i]);
	}
}

static NES::ControllerInput MapVirtualKeyToNesInput(WPARAM vkey)
{
	switch (vkey)
	{
	case VK_RETURN:
		return NES::ControllerInput::Start;
	case VK_SPACE:
		return NES::ControllerInput::Select;
	case 'z':
	case 'Z':
		return NES::ControllerInput::A;
	case 'x':
	case 'X':
		return NES::ControllerInput::B;
	case VK_DOWN:
		return NES::ControllerInput::Down;
	case VK_UP:
		return NES::ControllerInput::Up;
	case VK_LEFT:
		return NES::ControllerInput::Left;
	case VK_RIGHT:
		return NES::ControllerInput::Right;
	default:
		return NES::ControllerInput::_Max;
	}
}

bool Win32KeyboardController::TranlateWindowsMessage(UINT msg, WPARAM wParam)
{
	if (msg == WM_KEYDOWN || msg == WM_KEYUP)
	{
		NES::ControllerInput nesInput = MapVirtualKeyToNesInput(wParam);
		if (nesInput != NES::ControllerInput::_Max)
		{
			SetInputStatus(nesInput, (msg == WM_KEYDOWN));
			return true;
		}
	}
	return false;
}


bool XInputGamepadController::Poll()
{
	XINPUT_STATE state;
	ZeroMemory(&state, sizeof(XINPUT_STATE));

	// Simply get the state of the controller from XInput.
	const DWORD dwResult = XInputGetState(0, &state);
	if (dwResult != ERROR_SUCCESS)
		return false;

	//WCHAR wzControllerStatus[128];
	//swprintf_s(wzControllerStatus, _countof(wzControllerStatus), L"%x", state.Gamepad.wButtons);
	//SetDlgItemTextW(IDC_CONTROLLER_OUTPUT, wzControllerStatus);

	struct XInputMapping
	{
		DWORD dwXInputValue;
		NES::ControllerInput nesInputValue;
	};

	static const XInputMapping c_inputMappings[] =
	{
		{XINPUT_GAMEPAD_DPAD_UP, NES::ControllerInput::Up},
		{XINPUT_GAMEPAD_DPAD_LEFT, NES::ControllerInput::Left},
		{XINPUT_GAMEPAD_DPAD_DOWN, NES::ControllerInput::Down},
		{XINPUT_GAMEPAD_DPAD_RIGHT, NES::ControllerInput::Right},
		{XINPUT_GAMEPAD_START, NES::ControllerInput::Start},
		{XINPUT_GAMEPAD_BACK, NES::ControllerInput::Select},
		{XINPUT_GAMEPAD_A, NES::ControllerInput::B},
		{XINPUT_GAMEPAD_B, NES::ControllerInput::A},
	};

	for (auto mapping : c_inputMappings)
	{
		SetInputStatus(mapping.nesInputValue, !!(state.Gamepad.wButtons & mapping.dwXInputValue));
	}
	return true;
}

void PushAggregateControllerState(const std::initializer_list<Controller>& controllers, NES::Controller& nesController)
{
	for (int i = static_cast<int>(NES::ControllerInput::_Min); i < static_cast<int>(NES::ControllerInput::_Max); ++i)
	{
		const NES::ControllerInput nesInput = static_cast<NES::ControllerInput>(i);
		bool isInputOn = false;

		for (const auto& controller : controllers)
		{
			if (controller.GetInputStatus(static_cast<NES::ControllerInput>(nesInput)))
				isInputOn = true;
		}
		nesController.SetInputStatus(static_cast<NES::ControllerInput>(nesInput), isInputOn);
	}

}


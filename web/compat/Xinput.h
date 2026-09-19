#pragma once
#include "Windows.h"

struct XINPUT_GAMEPAD {
    WORD wButtons;
    BYTE bLeftTrigger, bRightTrigger;
    SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY;
};
struct XINPUT_STATE { DWORD dwPacketNumber; XINPUT_GAMEPAD Gamepad; };
extern "C" DWORD WINAPI XInputGetState(DWORD, XINPUT_STATE*);


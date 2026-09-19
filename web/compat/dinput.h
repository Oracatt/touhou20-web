#pragma once
#include "Windows.h"

struct DIDEVCAPS {
    DWORD dwSize, dwFlags, dwDevType, dwAxes, dwButtons, dwPOVs;
    DWORD dwFFSamplePeriod, dwFFMinTimeResolution, dwFirmwareRevision;
    DWORD dwHardwareRevision, dwFFDriverVersion;
};
static_assert(sizeof(DIDEVCAPS) == 0x2c);

struct DIJOYSTATE2 {
    LONG lX, lY, lZ, lRx, lRy, lRz;
    LONG rglSlider[2];
    DWORD rgdwPOV[4];
    BYTE rgbButtons[128];
    LONG lVX, lVY, lVZ, lVRx, lVRy, lVRz;
    LONG rglVSlider[2];
    LONG lAX, lAY, lAZ, lARx, lARy, lARz;
    LONG rglASlider[2];
    LONG lFX, lFY, lFZ, lFRx, lFRy, lFRz;
    LONG rglFSlider[2];
};
static_assert(sizeof(DIJOYSTATE2) == 272);

struct DIDATAFORMAT;
struct DIPROPHEADER;
struct DIDEVICEINSTANCEW;
using LPDIENUMDEVICESCALLBACKW = BOOL (CALLBACK*)(const DIDEVICEINSTANCEW*, void*);

struct IDirectInputDevice8W : IUnknown {
    virtual HRESULT GetCapabilities(DIDEVCAPS*) = 0;
    virtual HRESULT EnumObjects(void*, void*, DWORD) = 0;
    virtual HRESULT SetProperty(REFGUID, const DIPROPHEADER*) = 0;
    virtual HRESULT Acquire() = 0;
    virtual HRESULT Unacquire() = 0;
    virtual HRESULT GetDeviceState(DWORD, void*) = 0;
    virtual HRESULT SetDataFormat(const DIDATAFORMAT*) = 0;
    virtual HRESULT SetCooperativeLevel(HWND, DWORD) = 0;
    virtual HRESULT Poll() = 0;
};
struct IDirectInput8W : IUnknown {
    virtual HRESULT CreateDevice(REFGUID, IDirectInputDevice8W**, IUnknown*) = 0;
    virtual HRESULT EnumDevices(DWORD, LPDIENUMDEVICESCALLBACKW, void*, DWORD) = 0;
};

inline constexpr HRESULT DIERR_INPUTLOST = static_cast<HRESULT>(0x8007001eU);


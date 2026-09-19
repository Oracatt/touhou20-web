#pragma once
#include "Windows.h"

struct JOYINFOEX {
    DWORD dwSize, dwFlags, dwXpos, dwYpos, dwZpos, dwRpos, dwUpos, dwVpos;
    DWORD dwButtons, dwButtonNumber, dwPOV, dwReserved1, dwReserved2;
};
struct JOYCAPSW {
    WORD wMid, wPid;
    WCHAR szPname[32];
    UINT wXmin, wXmax, wYmin, wYmax, wZmin, wZmax;
    UINT wNumButtons, wPeriodMin, wPeriodMax;
    UINT wRmin, wRmax, wUmin, wUmax, wVmin, wVmax;
    UINT wCaps, wMaxAxes, wNumAxes, wMaxButtons;
    WCHAR szRegKey[32];
    WCHAR szOEMVxD[260];
};
static_assert(sizeof(JOYCAPSW) == 0x2d8);
inline constexpr MMRESULT JOYERR_NOERROR = 0;
extern "C" MMRESULT WINAPI joyGetPosEx(UINT, JOYINFOEX*);
extern "C" MMRESULT WINAPI joyGetDevCapsW(UINT_PTR, JOYCAPSW*, UINT);
extern "C" DWORD WINAPI timeGetTime();
extern "C" MMRESULT WINAPI timeBeginPeriod(UINT);
extern "C" MMRESULT WINAPI timeEndPeriod(UINT);


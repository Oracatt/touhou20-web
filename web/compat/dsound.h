#pragma once
#include "Windows.h"

#pragma pack(push, 1)
struct WAVEFORMATEX {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
};
#pragma pack(pop)
static_assert(sizeof(WAVEFORMATEX) == 18);

struct DSBUFFERDESC {
    DWORD dwSize, dwFlags, dwBufferBytes, dwReserved;
    WAVEFORMATEX* lpwfxFormat;
    GUID guid3DAlgorithm;
};
struct DSBPOSITIONNOTIFY { DWORD dwOffset; HANDLE hEventNotify; };
struct DSCAPS { DWORD dwSize; std::uint8_t remaining[92]; };

struct IDirectSoundBuffer : IUnknown {
    virtual HRESULT GetCaps(DSCAPS*) = 0;
    virtual HRESULT GetCurrentPosition(DWORD*, DWORD*) = 0;
    virtual HRESULT GetStatus(DWORD*) = 0;
    virtual HRESULT Initialize(void*, const DSBUFFERDESC*) = 0;
    virtual HRESULT Lock(DWORD, DWORD, void**, DWORD*, void**, DWORD*, DWORD) = 0;
    virtual HRESULT Play(DWORD, DWORD, DWORD) = 0;
    virtual HRESULT SetCurrentPosition(DWORD) = 0;
    virtual HRESULT SetFormat(const WAVEFORMATEX*) = 0;
    virtual HRESULT SetPan(LONG) = 0;
    virtual HRESULT SetVolume(LONG) = 0;
    virtual HRESULT Stop() = 0;
    virtual HRESULT Unlock(void*, DWORD, void*, DWORD) = 0;
    virtual HRESULT Restore() = 0;
};
struct IDirectSoundNotify : IUnknown {
    virtual HRESULT SetNotificationPositions(DWORD, const DSBPOSITIONNOTIFY*) = 0;
};
struct IDirectSound8 : IUnknown {
    virtual HRESULT CreateSoundBuffer(const DSBUFFERDESC*, IDirectSoundBuffer**, IUnknown*) = 0;
    virtual HRESULT DuplicateSoundBuffer(IDirectSoundBuffer*, IDirectSoundBuffer**) = 0;
    virtual HRESULT SetCooperativeLevel(HWND, DWORD) = 0;
};

inline constexpr DWORD DSSCL_PRIORITY = 2;
inline constexpr DWORD DSBCAPS_PRIMARYBUFFER = 1;
inline constexpr DWORD DSBCAPS_CTRLVOLUME = 0x80;
inline constexpr DWORD DSBCAPS_CTRLPAN = 0x40;
inline constexpr DWORD DSBCAPS_CTRLPOSITIONNOTIFY = 0x100;
inline constexpr DWORD DSBCAPS_GETCURRENTPOSITION2 = 0x10000;
inline constexpr DWORD DSBPLAY_LOOPING = 1;
inline constexpr DWORD DSBSTATUS_PLAYING = 1;
inline constexpr DWORD DSBSTATUS_BUFFERLOST = 2;
inline constexpr DWORD DSBLOCK_ENTIREBUFFER = 2;
inline constexpr DWORD WAVE_FORMAT_PCM = 1;
inline constexpr HRESULT DSERR_BUFFERLOST = static_cast<HRESULT>(0x88780096U);
inline constexpr GUID IID_IDirectSoundNotify{
    0xb0210783U, 0x89cdU, 0x11d0U, {0xaf,0x08,0x00,0xa0,0xc9,0x25,0xcd,0x16}
};

extern "C" HRESULT WINAPI DirectSoundCreate8(const GUID*, IDirectSound8**, IUnknown*);

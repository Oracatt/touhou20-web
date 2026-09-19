#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cerrno>
#include <ctime>

#ifndef WINAPI
#define WINAPI
#endif
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef APIENTRY
#define APIENTRY WINAPI
#endif

using BOOL = std::int32_t;
using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using SHORT = std::int16_t;
using USHORT = std::uint16_t;
using INT = std::int32_t;
using UINT = std::uint32_t;
using LONG = std::int32_t;
using ULONG = std::uint32_t;
using DWORD = std::uint32_t;
using LONGLONG = std::int64_t;
using ULONGLONG = std::uint64_t;
using __time64_t = std::int64_t;
using UINT_PTR = std::uintptr_t;
using INT_PTR = std::intptr_t;
using ULONG_PTR = std::uintptr_t;
using LONG_PTR = std::intptr_t;
using DWORD_PTR = std::uintptr_t;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;
using HRESULT = std::int32_t;
using MMRESULT = std::uint32_t;
using CHAR = char;
using WCHAR = char16_t;
using LPSTR = char*;
using LPCSTR = const char*;
using LPWSTR = wchar_t*;
using LPCWSTR = const wchar_t*;
using LPVOID = void*;
using LPCVOID = const void*;

using HANDLE = void*;
using HWND = void*;
using HINSTANCE = void*;
using HMODULE = void*;
using HDC = void*;
using HGDIOBJ = void*;
using HBITMAP = void*;
using HFONT = void*;
using HBRUSH = void*;
using HCURSOR = void*;
using HMENU = void*;

inline constexpr BOOL FALSE = 0;
inline constexpr BOOL TRUE = 1;
inline constexpr HRESULT S_OK = 0;
inline constexpr HRESULT E_FAIL = static_cast<HRESULT>(0x80004005U);
inline constexpr HRESULT E_POINTER = static_cast<HRESULT>(0x80004003U);
inline constexpr HRESULT E_INVALIDARG = static_cast<HRESULT>(0x80070057U);
inline constexpr HRESULT E_NOTIMPL = static_cast<HRESULT>(0x80004001U);
inline constexpr HRESULT E_UNEXPECTED = static_cast<HRESULT>(0x8000ffffU);
inline constexpr HRESULT CO_E_NOTINITIALIZED = static_cast<HRESULT>(0x800401f0U);
inline constexpr DWORD ERROR_SUCCESS = 0;
inline constexpr DWORD INFINITE = 0xffffffffU;

#ifndef FAILED
#define FAILED(value) (static_cast<HRESULT>(value) < 0)
#endif
#ifndef SUCCEEDED
#define SUCCEEDED(value) (static_cast<HRESULT>(value) >= 0)
#endif

struct GUID {
    std::uint32_t Data1;
    std::uint16_t Data2;
    std::uint16_t Data3;
    std::uint8_t Data4[8];
};
using REFGUID = const GUID&;
using REFIID = const GUID&;
using CLSID = GUID;

struct IUnknown {
    virtual HRESULT QueryInterface(REFIID, void**) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
protected:
    ~IUnknown() = default;
};

struct RECT { LONG left, top, right, bottom; };
struct POINT { LONG x, y; };
struct SIZE { LONG cx, cy; };
struct LARGE_INTEGER { LONGLONG QuadPart; };
struct PALETTEENTRY { BYTE peRed, peGreen, peBlue, peFlags; };
struct DEVMODEW {
    std::uint8_t reserved[172]{};
    DWORD dmPelsWidth = 640;
    DWORD dmPelsHeight = 480;
};
struct LOGFONTW {
    LONG lfHeight,lfWidth,lfEscapement,lfOrientation,lfWeight;
    BYTE lfItalic,lfUnderline,lfStrikeOut,lfCharSet,lfOutPrecision,lfClipPrecision,lfQuality,lfPitchAndFamily;
    wchar_t lfFaceName[32];
};
struct TEXTMETRICW {
    LONG tmHeight,tmAscent,tmDescent,tmInternalLeading,tmExternalLeading;
    LONG tmAveCharWidth,tmMaxCharWidth,tmWeight,tmOverhang;
    LONG tmDigitizedAspectX,tmDigitizedAspectY;
    wchar_t tmFirstChar,tmLastChar,tmDefaultChar,tmBreakChar;
    BYTE tmItalic,tmUnderlined,tmStruckOut,tmPitchAndFamily,tmCharSet;
};
using FONTENUMPROCW = int (CALLBACK*)(const LOGFONTW*,const TEXTMETRICW*,DWORD,LPARAM);

using WNDPROC = LRESULT (CALLBACK*)(HWND, UINT, WPARAM, LPARAM);
struct WNDCLASSW {
    UINT style{};
    WNDPROC lpfnWndProc{};
    INT cbClsExtra{}, cbWndExtra{};
    HINSTANCE hInstance{};
    HGDIOBJ hIcon{};
    HCURSOR hCursor{};
    HBRUSH hbrBackground{};
    const wchar_t* lpszMenuName{};
    const wchar_t* lpszClassName{};
};
struct MSG { HWND hwnd{}; UINT message{}; WPARAM wParam{}; LPARAM lParam{}; DWORD time{}; POINT pt{}; };

inline constexpr UINT ENUM_CURRENT_SETTINGS = 0xffffffffU;
inline constexpr INT VREFRESH = 116;
inline constexpr DWORD SHIFTJIS_CHARSET = 128;
inline constexpr DWORD ERROR_PROC_NOT_FOUND = 127;
inline constexpr DWORD GENERIC_READ = 0x80000000U;
inline constexpr DWORD GENERIC_WRITE = 0x40000000U;
inline constexpr DWORD FILE_SHARE_READ = 1;
inline constexpr DWORD CREATE_ALWAYS = 2;
inline constexpr DWORD OPEN_EXISTING = 3;
inline constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x80;
inline constexpr DWORD FILE_FLAG_SEQUENTIAL_SCAN = 0x08000000;
inline constexpr DWORD FILE_BEGIN = 0;
inline constexpr DWORD FILE_CURRENT = 1;
inline constexpr DWORD WAIT_OBJECT_0 = 0;
inline constexpr UINT WM_QUIT = 0x0012;
inline constexpr UINT PM_REMOVE = 1;
inline constexpr GUID GUID_NULL{};
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(std::intptr_t)-1)
#endif
using errno_t = int;

void Sleep(DWORD milliseconds);
UINT_PTR SetTimer(HWND window, UINT_PTR id, UINT interval, void* callback);
BOOL KillTimer(HWND window, UINT_PTR id);
int MultiByteToWideChar(UINT code_page, DWORD flags, const char* source, int source_length, wchar_t* destination, int capacity);
int MultiByteToWideChar(UINT code_page, DWORD flags, const char* source, int source_length, char16_t* destination, int capacity);
int WideCharToMultiByte(UINT code_page, DWORD flags, const wchar_t* source, int source_length, char* destination, int capacity, const char* fallback, BOOL* used_fallback);
HANDLE CreateFileW(const wchar_t* path, DWORD access, DWORD sharing, void* security, DWORD creation, DWORD attributes, HANDLE template_file);
BOOL ReadFile(HANDLE file, void* destination, DWORD count, DWORD* read, void* overlapped);
BOOL WriteFile(HANDLE file, const void* source, DWORD count, DWORD* written, void* overlapped);
DWORD SetFilePointer(HANDLE file, LONG distance, LONG* high, DWORD method);
DWORD GetFileSize(HANDLE file, DWORD* high);
BOOL CloseHandle(HANDLE handle);
HANDLE CreateEventW(void* security, BOOL manual_reset, BOOL initial_state, const wchar_t* name);
using LPTHREAD_START_ROUTINE = DWORD (WINAPI*)(void*);
HANDLE CreateThread(void* attributes, std::size_t stack_size, LPTHREAD_START_ROUTINE start, void* parameter, DWORD flags, DWORD* id);
DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds);
DWORD MsgWaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL wait_all, DWORD milliseconds, DWORD wake_mask);
BOOL PeekMessageW(MSG* message, HWND window, UINT minimum, UINT maximum, UINT remove);
BOOL PostThreadMessageW(DWORD thread_id, UINT message, WPARAM wparam, LPARAM lparam);
DWORD FormatMessageW(DWORD flags, const void* source, DWORD message_id, DWORD language_id, LPWSTR buffer, DWORD size, std::va_list* arguments);
HANDLE LocalFree(HANDLE memory);
HMODULE LoadLibraryW(const wchar_t* name);
BOOL FreeLibrary(HMODULE module);
void* GetProcAddress(HMODULE module, const char* name);
DWORD GetLastError();
BOOL EnumDisplaySettingsW(const wchar_t* device, DWORD mode, DEVMODEW* output);
HDC GetDC(HWND window);
INT ReleaseDC(HWND window, HDC context);
INT GetDeviceCaps(HDC context, INT index);
HFONT CreateFontW(INT height,INT width,INT escapement,INT orientation,INT weight,DWORD italic,DWORD underline,DWORD strikeout,
    DWORD charset,DWORD output_precision,DWORD clip_precision,DWORD quality,DWORD pitch_family,const wchar_t* face);
INT EnumFontFamiliesExW(HDC context,LOGFONTW* description,FONTENUMPROCW callback,LPARAM parameter,DWORD flags);
BOOL DeleteObject(HGDIOBJ object);

inline __time64_t _time64(__time64_t* destination) noexcept {
    const auto value = static_cast<__time64_t>(std::time(nullptr));
    if (destination) *destination = value;
    return value;
}

template<typename LocalTime>
inline errno_t _localtime64_s(LocalTime* destination, const __time64_t* source) noexcept {
    if (!destination || !source) return EINVAL;
    const std::time_t value = static_cast<std::time_t>(*source);
    const std::tm* converted = std::localtime(&value);
    if (!converted) return EINVAL;
    destination->tm_sec = converted->tm_sec;
    destination->tm_min = converted->tm_min;
    destination->tm_hour = converted->tm_hour;
    destination->tm_mday = converted->tm_mday;
    destination->tm_mon = converted->tm_mon;
    destination->tm_year = converted->tm_year;
    destination->tm_wday = converted->tm_wday;
    destination->tm_yday = converted->tm_yday;
    destination->tm_isdst = converted->tm_isdst;
    return 0;
}

// The recovered code uses the bounds-checked Microsoft CRT entry points.
// Keep their destination-clearing and overflow behavior on the web target.
inline errno_t strcpy_s(char* destination, std::size_t capacity, const char* source) noexcept {
    if (!destination || capacity == 0) return EINVAL;
    destination[0] = '\0';
    if (!source) return EINVAL;
    const std::size_t length = std::strlen(source);
    if (length >= capacity) return ERANGE;
    std::memcpy(destination, source, length + 1);
    return 0;
}

template<std::size_t Capacity>
inline errno_t strcpy_s(char (&destination)[Capacity], const char* source) noexcept {
    return strcpy_s(destination, Capacity, source);
}

inline errno_t wcscpy_s(wchar_t* destination,std::size_t capacity,const wchar_t* source) noexcept {
    if(!destination||capacity==0)return EINVAL;destination[0]=L'\0';if(!source)return EINVAL;
    const auto length=std::wcslen(source);if(length>=capacity)return ERANGE;
    std::wmemcpy(destination,source,length+1);return 0;
}
template<std::size_t Capacity>
inline errno_t wcscpy_s(wchar_t (&destination)[Capacity],const wchar_t* source) noexcept {
    return wcscpy_s(destination,Capacity,source);
}

inline int vsprintf_s(char* destination, std::size_t capacity, const char* format, std::va_list arguments) noexcept {
    if (!destination || capacity == 0 || !format) {
        if (destination && capacity) destination[0] = '\0';
        return -1;
    }
    const int result = std::vsnprintf(destination, capacity, format, arguments);
    if (result < 0 || static_cast<std::size_t>(result) >= capacity) {
        destination[0] = '\0';
        return -1;
    }
    return result;
}

inline int sprintf_s(char* destination, std::size_t capacity, const char* format, ...) noexcept {
    std::va_list arguments;
    va_start(arguments, format);
    const int result = vsprintf_s(destination, capacity, format, arguments);
    va_end(arguments);
    return result;
}

template<std::size_t Capacity, typename... Arguments>
inline int sprintf_s(char (&destination)[Capacity], const char* format, Arguments... arguments) noexcept {
    const int result = std::snprintf(destination, Capacity, format, arguments...);
    if (result < 0 || static_cast<std::size_t>(result) >= Capacity) {
        destination[0] = '\0';
        return -1;
    }
    return result;
}

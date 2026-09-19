#pragma once
#include "Windows.h"

namespace Gdiplus {
using Status = INT;
inline constexpr Status Ok = 0;
struct GdiplusStartupInput {};
Status GdiplusStartup(ULONG_PTR*, const GdiplusStartupInput*, void*);
void GdiplusShutdown(ULONG_PTR);
class Bitmap {
public:
    explicit Bitmap(const wchar_t*);
    Status GetLastStatus() const;
    Status Save(const wchar_t*, const void*, const void*);
};
struct ImageCodecInfo { const WCHAR* MimeType{}; GUID Clsid{}; };
Status GetImageEncodersSize(UINT*, UINT*);
Status GetImageEncoders(UINT, UINT, ImageCodecInfo*);
}

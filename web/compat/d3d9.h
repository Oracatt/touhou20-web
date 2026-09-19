#pragma once

#include "Windows.h"
#include <cstdint>

using D3DCOLOR = std::uint32_t;
using D3DFORMAT = std::uint32_t;
using D3DDEVTYPE = std::uint32_t;
using D3DSWAPEFFECT = std::uint32_t;
using D3DMULTISAMPLE_TYPE = std::uint32_t;
using D3DRESOURCETYPE = std::uint32_t;
using D3DPOOL = std::uint32_t;
using D3DPRIMITIVETYPE = std::uint32_t;
using D3DRENDERSTATETYPE = std::uint32_t;
using D3DTEXTURESTAGESTATETYPE = std::uint32_t;
using D3DSAMPLERSTATETYPE = std::uint32_t;
using D3DTRANSFORMSTATETYPE = std::uint32_t;
using D3DBACKBUFFER_TYPE = std::uint32_t;

inline constexpr HRESULT D3D_OK = 0;
inline constexpr HRESULT D3DERR_DEVICENOTRESET = static_cast<HRESULT>(0x88760869U);
inline constexpr UINT D3D_SDK_VERSION = 32;
inline constexpr D3DDEVTYPE D3DDEVTYPE_HAL = 1, D3DDEVTYPE_REF = 2;
inline constexpr DWORD D3DCREATE_FPU_PRESERVE = 0x2, D3DCREATE_MULTITHREADED = 0x4;
inline constexpr DWORD D3DCREATE_SOFTWARE_VERTEXPROCESSING = 0x20, D3DCREATE_HARDWARE_VERTEXPROCESSING = 0x40;
inline constexpr D3DFORMAT D3DFMT_UNKNOWN = 0, D3DFMT_R5G6B5 = 23, D3DFMT_X8R8G8B8 = 22;
inline constexpr D3DFORMAT D3DFMT_A8R8G8B8 = 21, D3DFMT_A1R5G5B5 = 25, D3DFMT_A4R4G4B4 = 26;
inline constexpr D3DFORMAT D3DFMT_A8R3G3B2 = 29, D3DFMT_A8 = 28, D3DFMT_D16 = 80, D3DFMT_DXT5 = 0x35545844;
inline constexpr D3DSWAPEFFECT D3DSWAPEFFECT_DISCARD = 1;
inline constexpr D3DMULTISAMPLE_TYPE D3DMULTISAMPLE_NONE = 0;
inline constexpr D3DRESOURCETYPE D3DRTYPE_SURFACE = 1, D3DRTYPE_TEXTURE = 3;
inline constexpr D3DPOOL D3DPOOL_DEFAULT = 0, D3DPOOL_MANAGED = 1;
inline constexpr D3DBACKBUFFER_TYPE D3DBACKBUFFER_TYPE_MONO = 0;
inline constexpr DWORD D3DPRESENT_INTERVAL_IMMEDIATE = 0x80000000U, D3DPRESENT_INTERVAL_ONE = 1;
inline constexpr DWORD D3DPRESENTFLAG_LOCKABLE_BACKBUFFER = 1;
inline constexpr DWORD D3DUSAGE_RENDERTARGET = 1, D3DUSAGE_DYNAMIC = 0x200;
inline constexpr DWORD D3DLOCK_READONLY = 0x10;
inline constexpr DWORD D3DCLEAR_TARGET = 1, D3DCLEAR_ZBUFFER = 2;
inline constexpr DWORD D3DFVF_XYZ = 0x2, D3DFVF_XYZRHW = 0x4, D3DFVF_DIFFUSE = 0x40, D3DFVF_TEX1 = 0x100;
inline constexpr D3DPRIMITIVETYPE D3DPT_POINTLIST = 1, D3DPT_LINELIST = 2, D3DPT_LINESTRIP = 3;
inline constexpr D3DPRIMITIVETYPE D3DPT_TRIANGLELIST = 4, D3DPT_TRIANGLESTRIP = 5, D3DPT_TRIANGLEFAN = 6;
inline constexpr DWORD D3DTS_VIEW = 2, D3DTS_PROJECTION = 3, D3DTS_TEXTURE0 = 16, D3DTS_WORLD = 256;
inline constexpr DWORD D3DRS_ZWRITEENABLE = 14, D3DRS_ALPHATESTENABLE = 15, D3DRS_SRCBLEND = 19, D3DRS_DESTBLEND = 20;
inline constexpr DWORD D3DRS_ZFUNC = 23, D3DRS_FOGENABLE = 28, D3DRS_FOGCOLOR = 34, D3DRS_FOGSTART = 36, D3DRS_FOGEND = 37;
inline constexpr DWORD D3DRS_TEXTUREFACTOR = 60, D3DRS_BLENDOP = 171, D3DRS_SEPARATEALPHABLENDENABLE = 206;
inline constexpr DWORD D3DRS_SRCBLENDALPHA = 207, D3DRS_DESTBLENDALPHA = 208, D3DRS_BLENDOPALPHA = 209;
inline constexpr DWORD D3DBLEND_INVSRCALPHA = 6, D3DCMP_LESSEQUAL = 4, D3DCMP_ALWAYS = 8;
inline constexpr DWORD D3DTSS_COLOROP = 1, D3DTSS_COLORARG1 = 2, D3DTSS_COLORARG2 = 3;
inline constexpr DWORD D3DTSS_ALPHAOP = 4, D3DTSS_ALPHAARG1 = 5, D3DTSS_ALPHAARG2 = 6;
inline constexpr DWORD D3DTOP_SELECTARG1 = 2, D3DTOP_MODULATE = 4, D3DTA_DIFFUSE = 0, D3DTA_TEXTURE = 2;

struct D3DMATRIX { float m[4][4]; };
struct D3DVIEWPORT9 { DWORD X, Y, Width, Height; float MinZ, MaxZ; };
struct D3DRECT { LONG x1, y1, x2, y2; };
struct D3DRASTER_STATUS { BOOL InVBlank; UINT ScanLine; };
struct D3DLOCKED_RECT { INT Pitch; void* pBits; };
struct D3DDISPLAYMODE { UINT Width, Height, RefreshRate; D3DFORMAT Format; };
struct D3DSURFACE_DESC {
    D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool;
    D3DMULTISAMPLE_TYPE MultiSampleType; DWORD MultiSampleQuality; UINT Width, Height;
};
struct D3DPRESENT_PARAMETERS {
    UINT BackBufferWidth, BackBufferHeight; D3DFORMAT BackBufferFormat; UINT BackBufferCount;
    D3DMULTISAMPLE_TYPE MultiSampleType; DWORD MultiSampleQuality; D3DSWAPEFFECT SwapEffect;
    HWND hDeviceWindow; BOOL Windowed, EnableAutoDepthStencil; D3DFORMAT AutoDepthStencilFormat;
    DWORD Flags; UINT FullScreen_RefreshRateInHz, PresentationInterval;
};
static_assert(sizeof(D3DPRESENT_PARAMETERS) == 0x38);
struct D3DCAPS9 {
    std::uint8_t before_width[0x58];
    DWORD MaxTextureWidth, MaxTextureHeight;
    std::uint8_t before_texture_ops[0x90 - 0x60];
    DWORD TextureOpCaps;
    std::uint8_t trailing[0x130 - 0x94];
};
static_assert(sizeof(D3DCAPS9) == 0x130);
static_assert(offsetof(D3DCAPS9, MaxTextureWidth) == 0x58);
static_assert(offsetof(D3DCAPS9, TextureOpCaps) == 0x90);

struct IDirect3DSurface9 : IUnknown {
    virtual HRESULT GetDesc(D3DSURFACE_DESC*) = 0;
    virtual HRESULT LockRect(D3DLOCKED_RECT*, const RECT*, DWORD) = 0;
    virtual HRESULT UnlockRect() = 0;
};
struct IDirect3DBaseTexture9 : IUnknown {
    virtual void PreLoad() = 0;
};
struct IDirect3DTexture9 : IDirect3DBaseTexture9 {
    virtual HRESULT GetLevelDesc(UINT, D3DSURFACE_DESC*) = 0;
    virtual HRESULT GetSurfaceLevel(UINT, IDirect3DSurface9**) = 0;
    virtual HRESULT LockRect(UINT, D3DLOCKED_RECT*, const RECT*, DWORD) = 0;
    virtual HRESULT UnlockRect(UINT) = 0;
    virtual void AddDirtyRect(const RECT*) = 0;
};
struct IDirect3DVertexBuffer9 : IUnknown {
    virtual HRESULT Lock(UINT, UINT, void**, DWORD) = 0;
    virtual HRESULT Unlock() = 0;
};
struct IDirect3DDevice9 : IUnknown {
    virtual HRESULT TestCooperativeLevel() = 0;
    virtual UINT GetAvailableTextureMem() = 0;
    virtual HRESULT EvictManagedResources() = 0;
    virtual HRESULT GetDeviceCaps(D3DCAPS9*) = 0;
    virtual HRESULT GetRasterStatus(UINT, D3DRASTER_STATUS*) = 0;
    virtual HRESULT Reset(D3DPRESENT_PARAMETERS*) = 0;
    virtual HRESULT Present(const RECT*, const RECT*, HWND, const void*) = 0;
    virtual HRESULT GetBackBuffer(UINT, UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9**) = 0;
    virtual HRESULT CreateTexture(UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*) = 0;
    virtual HRESULT CreateVertexBuffer(UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9**, HANDLE*) = 0;
    virtual HRESULT SetRenderTarget(DWORD, IDirect3DSurface9*) = 0;
    virtual HRESULT BeginScene() = 0;
    virtual HRESULT EndScene() = 0;
    virtual HRESULT Clear(DWORD, const D3DRECT*, DWORD, D3DCOLOR, float, DWORD) = 0;
    virtual HRESULT SetTransform(D3DTRANSFORMSTATETYPE, const D3DMATRIX*) = 0;
    virtual HRESULT SetViewport(const D3DVIEWPORT9*) = 0;
    virtual HRESULT SetRenderState(D3DRENDERSTATETYPE, DWORD) = 0;
    virtual HRESULT SetTexture(DWORD, IDirect3DBaseTexture9*) = 0;
    virtual HRESULT SetTextureStageState(DWORD, D3DTEXTURESTAGESTATETYPE, DWORD) = 0;
    virtual HRESULT SetSamplerState(DWORD, D3DSAMPLERSTATETYPE, DWORD) = 0;
    virtual HRESULT SetFVF(DWORD) = 0;
    virtual HRESULT SetVertexShader(void*) = 0;
    virtual HRESULT SetStreamSource(UINT, IDirect3DVertexBuffer9*, UINT, UINT) = 0;
    virtual HRESULT DrawPrimitive(D3DPRIMITIVETYPE, UINT, UINT) = 0;
    virtual HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE, UINT, const void*, UINT) = 0;
};
struct IDirect3D9 : IUnknown {
    virtual HRESULT GetAdapterDisplayMode(UINT, D3DDISPLAYMODE*) = 0;
    virtual HRESULT CheckDeviceFormat(UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT) = 0;
    virtual HRESULT CreateDevice(UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**) = 0;
};

extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT);

#include <Windows.h>
#include <d3d9.h>
#include <mmsystem.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
DWORD last_error=0;
struct WebFile {std::FILE* stream;};

std::string utf8(const wchar_t* source) {
    std::string result;
    if(!source)return result;
    for(;*source;++source) {
        auto value=static_cast<std::uint32_t>(*source);
        if(value<=0x7f)result.push_back(static_cast<char>(value));
        else if(value<=0x7ff){result.push_back(static_cast<char>(0xc0|(value>>6)));result.push_back(static_cast<char>(0x80|(value&0x3f)));}
        else if(value<=0xffff){result.push_back(static_cast<char>(0xe0|(value>>12)));result.push_back(static_cast<char>(0x80|((value>>6)&0x3f)));result.push_back(static_cast<char>(0x80|(value&0x3f)));}
        else {result.push_back(static_cast<char>(0xf0|(value>>18)));result.push_back(static_cast<char>(0x80|((value>>12)&0x3f)));result.push_back(static_cast<char>(0x80|((value>>6)&0x3f)));result.push_back(static_cast<char>(0x80|(value&0x3f)));}
    }
    return result;
}

EM_JS(int,decode_shift_jis_wide,(const char* source,int source_length,wchar_t* destination,int capacity),{
    let length=source_length;
    if(length<0){length=0;while(HEAPU8[source+length])++length;}
    let value;
    try {value=new TextDecoder('shift_jis').decode(HEAPU8.slice(source,source+length));}
    catch (_) {value=new TextDecoder().decode(HEAPU8.slice(source,source+length));}
    const points=Array.from(value);
    const required=points.length+(source_length<0?1:0);
    if(destination && capacity>0){
        const count=Math.min(points.length,capacity-(source_length<0?1:0));
        for(let index=0;index<count;++index)HEAPU32[(destination>>2)+index]=points[index].codePointAt(0);
        if(source_length<0 && count<capacity)HEAPU32[(destination>>2)+count]=0;
    }
    return required;
});

EM_JS(int,decode_shift_jis_utf16,(const char* source,int source_length,char16_t* destination,int capacity),{
    let length=source_length;
    if(length<0){length=0;while(HEAPU8[source+length])++length;}
    let value;
    try {value=new TextDecoder('shift_jis').decode(HEAPU8.slice(source,source+length));}
    catch (_) {value=new TextDecoder().decode(HEAPU8.slice(source,source+length));}
    const required=value.length+(source_length<0?1:0);
    if(destination && capacity>0){
        const count=Math.min(value.length,capacity-(source_length<0?1:0));
        for(let index=0;index<count;++index)HEAPU16[(destination>>1)+index]=value.charCodeAt(index);
        if(source_length<0 && count<capacity)HEAPU16[(destination>>1)+count]=0;
    }
    return required;
});

void multiply(D3DMATRIX& output,const D3DMATRIX& left,const D3DMATRIX& right) {
    D3DMATRIX result{};
    for(int row=0;row<4;++row)for(int column=0;column<4;++column)
        for(int inner=0;inner<4;++inner)result.m[row][column]+=left.m[row][inner]*right.m[inner][column];
    output=result;
}
struct V3 {float x,y,z;};struct V4 {float x,y,z,w;};
V4 transform4(const V3& input,const D3DMATRIX& matrix) {
    return {
        input.x*matrix.m[0][0]+input.y*matrix.m[1][0]+input.z*matrix.m[2][0]+matrix.m[3][0],
        input.x*matrix.m[0][1]+input.y*matrix.m[1][1]+input.z*matrix.m[2][1]+matrix.m[3][1],
        input.x*matrix.m[0][2]+input.y*matrix.m[1][2]+input.z*matrix.m[2][2]+matrix.m[3][2],
        input.x*matrix.m[0][3]+input.y*matrix.m[1][3]+input.z*matrix.m[2][3]+matrix.m[3][3]};
}
}

namespace { bool browser_frame_loop_active=false; }
extern "C" void th20_web_set_frame_loop_active(int active){browser_frame_loop_active=active!=0;}
void Sleep(DWORD milliseconds){
    // Startup resource decoding needs to yield so browser image promises can
    // finish. Once requestAnimationFrame owns pacing, suspending in the middle
    // of a recovered frame would strand the scheduler.
    if(!browser_frame_loop_active) emscripten_sleep(milliseconds);
}
UINT_PTR SetTimer(HWND,UINT_PTR id,UINT,void*){return id?id:1;}
BOOL KillTimer(HWND,UINT_PTR){return TRUE;}
int MultiByteToWideChar(UINT,DWORD,const char* source,int length,wchar_t* destination,int capacity){return decode_shift_jis_wide(source,length,destination,capacity);}
int MultiByteToWideChar(UINT,DWORD,const char* source,int length,char16_t* destination,int capacity){return decode_shift_jis_utf16(source,length,destination,capacity);}

HANDLE CreateFileW(const wchar_t* path,DWORD access,DWORD,void*,DWORD creation,DWORD,HANDLE){
    const auto name=utf8(path);const char* mode=(access&GENERIC_WRITE)?(creation==CREATE_ALWAYS?"wb+":"rb+"):"rb";
    auto* stream=std::fopen(name.c_str(),mode);if(!stream){last_error=2;return INVALID_HANDLE_VALUE;}
    return new WebFile{stream};
}
BOOL ReadFile(HANDLE handle,void* destination,DWORD count,DWORD* received,void*){
    if(!handle||handle==INVALID_HANDLE_VALUE)return FALSE;auto* file=static_cast<WebFile*>(handle);
    const auto size=std::fread(destination,1,count,file->stream);if(received)*received=static_cast<DWORD>(size);return TRUE;
}
BOOL WriteFile(HANDLE handle,const void* source,DWORD count,DWORD* written,void*){
    if(!handle||handle==INVALID_HANDLE_VALUE)return FALSE;auto* file=static_cast<WebFile*>(handle);
    const auto size=std::fwrite(source,1,count,file->stream);if(written)*written=static_cast<DWORD>(size);return size==count;
}
DWORD SetFilePointer(HANDLE handle,LONG distance,LONG* high,DWORD method){
    if(!handle||handle==INVALID_HANDLE_VALUE)return 0xffffffffU;auto* file=static_cast<WebFile*>(handle);
    std::int64_t offset=static_cast<std::uint32_t>(distance);if(high)offset|=static_cast<std::int64_t>(*high)<<32;
    const int origin=method==FILE_BEGIN?SEEK_SET:method==FILE_CURRENT?SEEK_CUR:SEEK_END;
    if(std::fseek(file->stream,static_cast<long>(offset),origin)!=0)return 0xffffffffU;
    const auto position=std::ftell(file->stream);if(high)*high=static_cast<LONG>(static_cast<std::uint64_t>(position)>>32);return static_cast<DWORD>(position);
}
DWORD GetFileSize(HANDLE handle,DWORD* high){
    if(!handle||handle==INVALID_HANDLE_VALUE)return 0xffffffffU;auto* file=static_cast<WebFile*>(handle);
    const auto position=std::ftell(file->stream);std::fseek(file->stream,0,SEEK_END);const auto size=std::ftell(file->stream);std::fseek(file->stream,position,SEEK_SET);
    if(high)*high=static_cast<DWORD>(static_cast<std::uint64_t>(size)>>32);return static_cast<DWORD>(size);
}
BOOL CloseHandle(HANDLE handle){if(!handle||handle==INVALID_HANDLE_VALUE)return FALSE;auto* file=static_cast<WebFile*>(handle);std::fclose(file->stream);delete file;return TRUE;}

HMODULE LoadLibraryW(const wchar_t*){return reinterpret_cast<HMODULE>(1);}
BOOL FreeLibrary(HMODULE){return TRUE;}
DWORD GetLastError(){return last_error;}
HDC GetDC(HWND){return reinterpret_cast<HDC>(1);}
INT ReleaseDC(HWND,HDC){return 1;}
INT GetDeviceCaps(HDC,INT index){return index==VREFRESH?60:0;}
BOOL DeleteObject(HGDIOBJ){return TRUE;}
BOOL EnumDisplaySettingsW(const wchar_t*,DWORD,DEVMODEW* mode){if(!mode)return FALSE;mode->dmPelsWidth=static_cast<DWORD>(emscripten_run_script_int("screen.width"));mode->dmPelsHeight=static_cast<DWORD>(emscripten_run_script_int("screen.height"));return TRUE;}

extern "C" DWORD WINAPI timeGetTime(){return static_cast<DWORD>(emscripten_get_now());}
extern "C" MMRESULT WINAPI timeBeginPeriod(UINT){return 0;}
extern "C" MMRESULT WINAPI timeEndPeriod(UINT){return 0;}

extern "C" D3DMATRIX* WINAPI D3DXMatrixRotationX(D3DMATRIX* out,float angle){const auto c=std::cos(angle),s=std::sin(angle);*out={{{1,0,0,0},{0,c,s,0},{0,-s,c,0},{0,0,0,1}}};return out;}
extern "C" D3DMATRIX* WINAPI D3DXMatrixRotationY(D3DMATRIX* out,float angle){const auto c=std::cos(angle),s=std::sin(angle);*out={{{c,0,-s,0},{0,1,0,0},{s,0,c,0},{0,0,0,1}}};return out;}
extern "C" D3DMATRIX* WINAPI D3DXMatrixRotationZ(D3DMATRIX* out,float angle){const auto c=std::cos(angle),s=std::sin(angle);*out={{{c,s,0,0},{-s,c,0,0},{0,0,1,0},{0,0,0,1}}};return out;}
extern "C" D3DMATRIX* WINAPI D3DXMatrixTranslation(D3DMATRIX* out,float x,float y,float z){*out={{{1,0,0,0},{0,1,0,0},{0,0,1,0},{x,y,z,1}}};return out;}
extern "C" D3DMATRIX* WINAPI D3DXMatrixMultiply(D3DMATRIX* out,const D3DMATRIX* a,const D3DMATRIX* b){multiply(*out,*a,*b);return out;}
extern "C" D3DMATRIX* WINAPI D3DXMatrixLookAtLH(D3DMATRIX* out,const V3* eye,const V3* at,const V3* up){
    auto normalize=[](V3 value){const auto length=std::sqrt(value.x*value.x+value.y*value.y+value.z*value.z);return V3{value.x/length,value.y/length,value.z/length};};
    const auto z=normalize({at->x-eye->x,at->y-eye->y,at->z-eye->z});
    const auto x=normalize({up->y*z.z-up->z*z.y,up->z*z.x-up->x*z.z,up->x*z.y-up->y*z.x});
    const V3 y{z.y*x.z-z.z*x.y,z.z*x.x-z.x*x.z,z.x*x.y-z.y*x.x};
    *out={{{x.x,y.x,z.x,0},{x.y,y.y,z.y,0},{x.z,y.z,z.z,0},{-(x.x*eye->x+x.y*eye->y+x.z*eye->z),-(y.x*eye->x+y.y*eye->y+y.z*eye->z),-(z.x*eye->x+z.y*eye->y+z.z*eye->z),1}}};return out;
}
extern "C" D3DMATRIX* WINAPI D3DXMatrixPerspectiveFovLH(D3DMATRIX* out,float fov,float aspect,float near_z,float far_z){const auto y=1/std::tan(fov*.5f),x=y/aspect;*out={{{x,0,0,0},{0,y,0,0},{0,0,far_z/(far_z-near_z),1},{0,0,-near_z*far_z/(far_z-near_z),0}}};return out;}
extern "C" V4* WINAPI D3DXVec3Transform(V4* out,const V3* input,const D3DMATRIX* matrix){*out=transform4(*input,*matrix);return out;}
extern "C" V3* WINAPI D3DXVec3TransformCoord(V3* out,const V3* input,const D3DMATRIX* matrix){const auto value=transform4(*input,*matrix);*out={value.x/value.w,value.y/value.w,value.z/value.w};return out;}
extern "C" V3* WINAPI D3DXVec3Project(V3* out,const V3* input,const D3DVIEWPORT9* viewport,const D3DMATRIX* projection,const D3DMATRIX* view,const D3DMATRIX* world){D3DMATRIX a{},combined{};multiply(a,*world,*view);multiply(combined,a,*projection);const auto value=transform4(*input,combined);const auto x=value.x/value.w,y=value.y/value.w,z=value.z/value.w;*out={viewport->X+(1+x)*viewport->Width*.5f,viewport->Y+(1-y)*viewport->Height*.5f,viewport->MinZ+z*(viewport->MaxZ-viewport->MinZ)};return out;}
extern "C" V3* WINAPI D3DXVec3ProjectArray(V3* output,UINT output_stride,const V3* input,UINT input_stride,const D3DVIEWPORT9* viewport,const D3DMATRIX* projection,const D3DMATRIX* view,const D3DMATRIX* world,UINT count){for(UINT index=0;index<count;++index)D3DXVec3Project(reinterpret_cast<V3*>(reinterpret_cast<std::uint8_t*>(output)+index*output_stride),reinterpret_cast<const V3*>(reinterpret_cast<const std::uint8_t*>(input)+index*input_stride),viewport,projection,view,world);return output;}

extern "C" HRESULT WINAPI D3DXCreateTexture(IDirect3DDevice9*,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DTexture9**);
extern "C" HRESULT WINAPI D3DXLoadSurfaceFromFileInMemory(IDirect3DSurface9*,const PALETTEENTRY*,const RECT*,const void*,UINT,const RECT*,DWORD,D3DCOLOR,void*);
extern "C" HRESULT WINAPI D3DXLoadSurfaceFromSurface(IDirect3DSurface9*,const PALETTEENTRY*,const RECT*,IDirect3DSurface9*,const PALETTEENTRY*,const RECT*,DWORD,D3DCOLOR);

void* GetProcAddress(HMODULE,const char* name){
#define TH20_PROC(symbol) if(std::strcmp(name,#symbol)==0)return reinterpret_cast<void*>(&symbol)
    TH20_PROC(D3DXMatrixRotationX);TH20_PROC(D3DXMatrixRotationY);TH20_PROC(D3DXMatrixRotationZ);
    TH20_PROC(D3DXMatrixTranslation);TH20_PROC(D3DXMatrixMultiply);TH20_PROC(D3DXMatrixLookAtLH);
    TH20_PROC(D3DXMatrixPerspectiveFovLH);TH20_PROC(D3DXVec3Transform);TH20_PROC(D3DXVec3TransformCoord);
    TH20_PROC(D3DXVec3Project);TH20_PROC(D3DXVec3ProjectArray);TH20_PROC(D3DXCreateTexture);
    TH20_PROC(D3DXLoadSurfaceFromFileInMemory);TH20_PROC(D3DXLoadSurfaceFromSurface);
#undef TH20_PROC
    last_error=ERROR_PROC_NOT_FOUND;return nullptr;
}

#include "raster.hpp"
#include "deferred_queue.hpp"
#include "../platform_window/fonts.hpp"
#include "../runtime_core/runtime_core.hpp"
#include <cwchar>
#include <cstring>
#include <mutex>
#include <emmintrin.h>
#if defined(TH20_WEB)
#include <emscripten/emscripten.h>
#endif
namespace th20::source::text {
#if defined(TH20_WEB)
namespace {
int web_font_height(int font) {
    constexpr int heights[22]={24,24,24,28,32,36,40,44,48,60,64,32,32,32,40,48,60,64,15,15,15,15};
    return font>=0 && font<22?heights[font]:24;
}
EM_JS(void,raster_browser_text,(std::uint8_t* output,int pitch,int width,int height,const char* bytes,
    int font,int spacing,unsigned foreground,unsigned background,int draw_outline,double radius,int x,int top,
    int* extent_width,int* extent_height),{
    let text;
    const length=HEAPU8.indexOf(0,bytes)-bytes;
    const source=HEAPU8.slice(bytes,bytes+(length>=0?length:0));
    try {text=new TextDecoder('shift_jis').decode(source);}
    catch (_) {text=new TextDecoder().decode(source);}
    const canvas=document.createElement('canvas');canvas.width=width;canvas.height=height;
    const context=canvas.getContext('2d',{willReadFrequently:true});
    const sizes=[24,24,24,28,32,36,40,44,48,60,64,32,32,32,40,48,60,64,15,15,15,15];
    const size=sizes[font] || 24;
    const mincho=font>=13;
    context.font=(mincho?'700 ':'400 ')+size+'px '+(mincho?'"Yu Mincho","MS Mincho",serif':'"Yu Gothic","Meiryo","MS Gothic",sans-serif');
    context.textBaseline='top';context.lineJoin='round';
    const red=(foreground>>>16)&255,green=(foreground>>>8)&255,blue=foreground&255;
    const backRed=(background>>>16)&255,backGreen=(background>>>8)&255,backBlue=background&255;
    context.fillStyle='rgb('+red+','+green+','+blue+')';
    context.strokeStyle='rgb('+backRed+','+backGreen+','+backBlue+')';
    context.lineWidth=radius*2;
    let measured=0;
    if(spacing) measured=Math.max(0,Array.from(text).length*spacing);
    else measured=context.measureText(text).width;
    if(spacing) {
        let at=x;
        for(const character of text) {
            if(draw_outline) context.strokeText(character,at+2,top);
            context.fillText(character,at+2,top);at+=spacing;
        }
    } else {
        if(draw_outline) context.strokeText(text,x+2,top);
        context.fillText(text,x+2,top);
    }
    const image=context.getImageData(0,0,width,height).data;
    for(let row=0;row<height;++row) for(let column=0;column<width;++column) {
        const input=(row*width+column)*4;
        const destination=output+row*pitch+column*4;
        HEAPU8[destination]=image[input+2];
        HEAPU8[destination+1]=image[input+1];
        HEAPU8[destination+2]=image[input];
        HEAPU8[destination+3]=image[input+3];
    }
    HEAP32[extent_width>>2]=Math.ceil(measured);
    HEAP32[extent_height>>2]=size;
});
}
#endif
float next_outline_scale=1.f;
void set_next_outline_scale(float value) noexcept {next_outline_scale=value;}
RasterizedText rasterize_text(const RECT& rectangle,std::int32_t x,std::uint32_t foreground,
    std::uint32_t background,const char* string,std::int32_t font,std::int32_t spacing,bool outline) {
    std::lock_guard lock(runtime::shared_locks().slot(18));
#if !defined(TH20_WEB)
    const auto selected_font=platform_window::fonts[font];float radius=2.f;int top=3;
#else
    float radius=2.f;int top=3;
#endif
    auto bitmap=std::make_unique<Bitmap>();
    switch(font){case 4:case 5:radius=2.5f;break;case 6:case 8:radius=3;break;case 9:radius=3.5f;break;
    case 10:radius=4;top=4;break;case 12:case 13:top=6;break;case 14:case 15:radius=3;top=6;break;
    case 16:case 17:radius=4;top=6;break;case 18:case 19:top=2;break;case 20:case 21:radius=3;top=7;break;}
    radius=_mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(radius),_mm_set_ss(next_outline_scale)));next_outline_scale=1.f;
#if defined(TH20_WEB)
    const int margin=_mm_cvttss_si32(_mm_set_ss(radius));
    const int width=rectangle.right-rectangle.left+3+margin,height=rectangle.bottom-rectangle.top+3+margin;
    bitmap->create_with_fallback(width,height+6,21);
    SIZE extent{};
    raster_browser_text(bitmap->pixels,bitmap->pitch,width,height+6,string,font,spacing,foreground,background,
        outline?1:0,radius,x,top,&extent.cx,&extent.cy);
    RECT source{0,0,rectangle.right-rectangle.left,rectangle.bottom-rectangle.top};
    if(source.right>1024)source.right=1024;
    return {std::move(bitmap),extent,rectangle,source};
#else
    wchar_t converted[257]{};MultiByteToWideChar(932,0,string,-1,converted,256);
    const int margin=_mm_cvttss_si32(_mm_set_ss(radius));
    const int width=rectangle.right-rectangle.left+3+margin,height=rectangle.bottom-rectangle.top+3+margin;
    bitmap->create_with_fallback(width,height+6,21);auto dc=bitmap->dc;auto old=SelectObject(dc,selected_font);
    const auto color=RGB((foreground>>16)&0xff,(foreground>>8)&0xff,foreground&0xff);
    SetBkMode(dc,TRANSPARENT);const auto count=static_cast<int>(std::wcslen(converted));SIZE extent;
    GetTextExtentPoint32W(dc,converted,count,&extent);SetTextColor(dc,color);
    auto draw=[&]{if(!spacing)TextOutW(dc,x+2,top,converted,count);else{int at=x;for(int i=0;i<count;++i){const wchar_t character[]{converted[i],0};TextOutW(dc,at,top,character,1);at+=spacing;}}};
    if(outline){draw();bitmap->outline(height+6,0,width,background,radius);}draw();
    SelectObject(dc,old);bitmap->invert_alpha(height+6,0,width);SelectObject(dc,old);
    RECT source{0,0,rectangle.right-rectangle.left,rectangle.bottom-rectangle.top};
    if(source.right>1024)source.right=1024;
    if(platform_window::font_available[0]&&font!=18&&font!=19){source.top=6;source.bottom+=6;}
    return {std::move(bitmap),extent,rectangle,source};
#endif
}
void upload_text_bitmap(Bitmap* bitmap,IDirect3DTexture9& texture,const RECT& destination,
                        const RECT& source,std::uint8_t* ready,const std::function<void()>& completion) {
    IDirect3DSurface9* surface;texture.GetSurfaceLevel(0,&surface);D3DLOCKED_RECT lock;
    surface->LockRect(&lock,&destination,0);auto* out=static_cast<std::uint8_t*>(lock.pBits);
    auto* in=bitmap->pixels+bitmap->pitch*source.top;
    //4145d0 adds source.left as a BYTE offset; normal producers always use0.
    for(int y=0;y<destination.bottom-destination.top;++y){std::memcpy(out,in+source.left,(source.right-source.left)*4);out+=lock.Pitch;in+=bitmap->pitch;}
    surface->UnlockRect();if(surface)surface->Release();bitmap->release();
    {bitmap->~Bitmap();std::lock_guard allocation(runtime::shared_locks().slot(1));::operator delete(bitmap);}
    if(ready)*ready=1;if(completion)completion();
}
SIZE queue_text(const RECT& rectangle,std::int32_t x,std::uint32_t foreground,std::uint32_t background,
    const char* string,IDirect3DTexture9& texture,std::int32_t font,std::int32_t spacing,bool outline,
    std::uint8_t* ready,std::function<void()> completion) {
    std::lock_guard consumer(runtime::shared_locks().slot(18));
    auto data=rasterize_text(rectangle,x,foreground,background,string,font,spacing,outline);
    std::lock_guard producer(runtime::shared_locks().slot(19));auto* bitmap=data.bitmap.release();
    enqueue_deferred_task([bitmap,&texture,destination=data.destination,source=data.source,ready,completion=std::move(completion)]{
        upload_text_bitmap(bitmap,texture,destination,source,ready,completion);
    });
    return data.extent;
}
}

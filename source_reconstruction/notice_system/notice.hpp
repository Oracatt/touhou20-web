#pragma once
#include "../runtime_core/callback_owner.hpp"
#include "../stone_menu/cursor.hpp"
#include "../sprite_renderer/sprite.hpp"
#include <array>
#include <string>
namespace th20::source::notice {
struct NoticeInf final:runtime::CallbackOwner {
    int state;
    recovered::Timer age;
    menu::Cursor cursor;
    std::uint32_t handles[7];
    std::uint32_t secondary_handle;
    int finished;
    float x;
    int selected_index;
    sprite::AnimationFile* file;
    sprite::AnimationFile* secondary_file;
    std::uint8_t* image_bytes;
    int substate;
    char filename[128];
    std::uint32_t image_size;
    NoticeInf(); //4dea60
    ~NoticeInf() override; //4deb80
    void enable_callbacks() override; //4a0a70
};
static_assert(sizeof(NoticeInf)==0x130&&offsetof(NoticeInf,cursor)==0x24&&offsetof(NoticeInf,handles)==0x70&&offsetof(NoticeInf,finished)==0x90&&offsetof(NoticeInf,filename)==0xac);
#if defined(TH20_WEB)
struct WebPmrString {
    std::pmr::string value;
    std::uint8_t abi_padding[12]{};
    WebPmrString()=default;
    WebPmrString(const char* text):value(text){}
    const char* c_str() const noexcept{return value.c_str();}
};
static_assert(sizeof(WebPmrString)==28);
using Message=std::array<WebPmrString,2>;
#else
using Message=std::array<std::pmr::string,2>;
#endif
extern std::array<Message,25> messages; //5c5b40, initialized40aa40
NoticeInf* controller();
NoticeInf* create(int); //4dfb90
int initialize(NoticeInf&); //4df670
void load_file(); //4df6e0 shared worker entry
int update(NoticeInf&); //4decf0
void spawn(NoticeInf&,int); //4dfb30
}

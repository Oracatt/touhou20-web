#pragma once
#include "../runtime_core/callback_owner.hpp"
#include <Windows.h>
#include <cstdint>
#include <ctime>
namespace th20::source::platform_window {
#if defined(TH20_WEB)
struct WebLocalTime {
    int tm_sec,tm_min,tm_hour,tm_mday,tm_mon,tm_year,tm_wday,tm_yday,tm_isdst;
};
static_assert(sizeof(WebLocalTime)==36);
#endif
class alignas(8) FrameStatistics : public runtime::CallbackOwner {
public:
    FrameStatistics();                    // 4abb80, after zeroing allocator4abad0
    ~FrameStatistics() override;          // 4abc90
    std::uint32_t field_10,padding_14;
    double sample_time;                  // +18
    std::uint32_t fast_intervals,frames;  // +20,+24
    double actual_frames,target_frames;  // +28,+30
    float frames_per_second;             // +38
    std::uint32_t padding_3c;
    double fields_40[2];
    std::int64_t begin_times[8],end_times[8]; // +50,+90, steady_clock nanoseconds
    std::int64_t wall_time_ticks;         // +d0, system_clock 100ns
    __time64_t unix_seconds;              // +d8
#if defined(TH20_WEB)
    WebLocalTime local_time;              // +e0, Windows-compatible 9-int tm
#else
    std::tm local_time;                   // +e0
#endif
    std::uint32_t padding_104;
    void update_local_time();             // 4ac0e0
    int update();                        // 4abd50
    int register_draw();                 // 4ac080
};
static_assert(offsetof(FrameStatistics,sample_time)==0x18);
static_assert(offsetof(FrameStatistics,begin_times)==0x50);
static_assert(offsetof(FrameStatistics,wall_time_ticks)==0xd0);
static_assert(sizeof(FrameStatistics)==0x108);
FrameStatistics* create_frame_statistics(); // 4ac1a0/4abad0
namespace unrecovered {
std::uint32_t* current_game_flags();       // 5ba828 nullable object +e8, supplied by game module
void draw_frame_rate_text(float);          // pending text renderer chain4abf30
}
}

#include "program_entry.hpp"
#include "unrecovered_dependencies.hpp"
#include "text_constants.hpp"
#include <mmsystem.h>
#include <cstring>
#if defined(TH20_WEB)
#include <emscripten/emscripten.h>
extern "C" void th20_web_set_frame_loop_active(int active);
#endif

// Present in the original USER32 import table; some modern SDK configurations
// omit the obsolete public declaration. This remains a normal OS import.
extern "C" BOOL WINAPI WINNLSEnableIME(HWND, BOOL);

namespace th20::source::program_entry {
namespace u = unrecovered;
namespace {
#if defined(TH20_WEB)
EM_JS(void,set_browser_cursor_visible,(int visible),{
    const canvas=Module.canvas || document.querySelector('canvas');
    if(canvas) canvas.style.cursor=visible ? 'default' : 'none';
});
EM_JS(void,focus_browser_canvas,(),{
    const canvas=Module.canvas || document.querySelector('canvas');
    if(canvas) canvas.focus();
});
EM_JS(void,resize_browser_canvas,(int width,int height),{
    const canvas=Module.canvas || document.querySelector('canvas');
    if(canvas) {canvas.width=width;canvas.height=height;}
});
EM_JS(void,report_browser_frame,(unsigned frame,int draw_counter,unsigned event_flags,int scene_state,int status),{
    window.th20Debug={frame:frame,drawCounter:draw_counter,eventFlags:event_flags,sceneState:scene_state,lastFrameStatus:status};
    const root=document.documentElement;
    root.dataset.th20Frame=String(frame);root.dataset.th20Scene=String(scene_state);root.dataset.th20FrameStatus=String(status);
});
EM_JS(void,report_browser_stage,(const char* stage),{
    const value=UTF8ToString(stage);
    window.th20Stage=value;
    console.log('TH20 stage:',value);
});
#endif
void hide_cursor() {
#if defined(TH20_WEB)
    set_browser_cursor_visible(0);
#else
    while (ShowCursor(FALSE) >= 0) {}
    SetCursor(nullptr);
#endif
}
void show_cursor() {
#if defined(TH20_WEB)
    set_browser_cursor_visible(1);
#else
    while (ShowCursor(TRUE) < 0) {}
#endif
}
void restore_system_settings(WindowStatePrefix& w) { // complete 0x0041b490 body
#if defined(TH20_WEB)
    (void)w;
#else
    SystemParametersInfoW(0x11, w.saved_screen_saver, nullptr, 2);
    SystemParametersInfoW(0x55, w.saved_low_power, nullptr, 2);
    SystemParametersInfoW(0x56, w.saved_power_off, nullptr, 2);
    WINNLSEnableIME(nullptr, TRUE);
#endif
}
void release_resource(IUnknown*& resource) {
    if (resource) { resource->Release(); resource = nullptr; }
}
#if defined(TH20_WEB)
void browser_frame_loop() {
    auto& w = window_state;
    auto& g = graphics_state;
    static bool first_frame = true;
    constexpr double frame_milliseconds = 1000.0 / 60.0;
    constexpr double maximum_backlog = frame_milliseconds * 5.0;
    static double previous_callback = -1.0;
    static double accumulated_milliseconds = 0.0;
    const double now = emscripten_get_now();
    if (w.quit_requested != 0 || device(g) == nullptr) {
        emscripten_cancel_main_loop();
        return;
    }

    // RAF follows the display (165 Hz, for example), whereas each recovered
    // scheduler update advances one 1/60-second game tick. Accumulate wall time
    // here instead of relying on D3D Present or native Sleep to limit the game.
    if (EM_ASM_INT({return document.hidden ? 1 : 0;})) {
        previous_callback = now;
        accumulated_milliseconds = 0.0;
        return;
    }
    if (previous_callback < 0.0) {
        accumulated_milliseconds = frame_milliseconds;
    } else {
        const double elapsed = now - previous_callback;
        // Resource decoding and a resumed tab must not cause an unbounded
        // catch-up burst. Short stalls still retain their fractional remainder.
        if (elapsed < 0.0 || elapsed > 250.0) accumulated_milliseconds = frame_milliseconds;
        else accumulated_milliseconds += elapsed;
        if (accumulated_milliseconds > maximum_backlog) accumulated_milliseconds = maximum_backlog;
    }
    previous_callback = now;

    const auto cooperative_status = device(g)->TestCooperativeLevel();
    if (cooperative_status != S_OK || needs_device_reset(w)) {
        accumulated_milliseconds = 0.0;
        return;
    }

    static unsigned browser_frame = 0;
    while (accumulated_milliseconds + 0.000001 >= frame_milliseconds) {
        accumulated_milliseconds -= frame_milliseconds;
        if (first_frame) {
            report_browser_stage("frame-callback-enter");
            report_browser_stage("frame-scheduler-enter");
        }
        // Browser pacing replaces the native timer/vblank choice. This path
        // preserves update/draw order and the configured drawing frame skip.
        const int status = run_present_paced_frame(w);
        if (first_frame) report_browser_stage("frame-scheduler-returned");
        report_browser_frame(++browser_frame, w.draw_counter, g.event_flags, g.field_0b0c, status);
        if (status != 0) {
            emscripten_cancel_main_loop();
            return;
        }
        graphics_event_flags &= ~8u;
        first_frame = false;
    }
}
#endif
}

// Recovered control flow of 0x0041e7d0..0x0041f1b8. Original arguments 2..4
// are passed by CRT but unused. The labels preserve the original restart and
// cleanup edges; unavailable engine operations remain unresolved declarations.
int WINAPI recovered_win_main(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    auto& w = window_state;
    auto& g = graphics_state;
    auto& config = g.configuration;
    int loop_status = 0;
    MSG message{};
#if defined(TH20_WEB)
    static u::RuntimeListenerStorage browser_listener;
    auto& listener = browser_listener;
#else
    u::RuntimeListenerStorage listener;
#endif
    char log_path[0x1000];
    void* allocation = nullptr;
    HRESULT cooperative_status = S_OK;

    // 0x0041e7e8..0x0041e89b, process-local initialization.
    w.instance = instance;
    w.startup_status = 0;
    timeBeginPeriod(1);
    u::set_rounding_mode(0);
    u::lock_registry_enable(lock_registry);
    allocation = u::allocate(8);
    allocations = allocation ? u::construct_allocations(allocation) : nullptr;
    u::construct_listener(listener);
    u::install_listener(&listener);
    u::log_append(log_buffer, text_constants::start);
    if (u::fn_0041c020(instance) == -1) goto release_runtime;
    u::fn_004117a0(g, instance);
    if (u::fn_0041b4f0(w) != 0) {
        u::log_error(log_buffer, text_constants::cannot_save);
        goto release_runtime;
    }
    if (u::load_configuration(g, "th20.cfg") != 0) goto release_runtime;
    u::fn_00420f80();
    u::fn_00421040();
    u::fn_0041ae70(instance);
    if (((w.flags >> 3) & 3u) != 0) goto release_runtime;
    w.display_mode = config.saved_display_mode;
    // The original call 0x004111e0 returns constant -1; its result is unused.
    u::fn_00416d20();

restart_runtime: // 0x0041e93d
#if defined(TH20_WEB)
    report_browser_stage("create-direct3d");
#endif
    if (u::fn_0041c320() != 0) goto release_runtime;
#if defined(TH20_WEB)
    report_browser_stage("create-window");
#endif
    if (u::create_window(w, instance) != 0) goto release_runtime;
#if defined(TH20_WEB)
    report_browser_stage("prepare-presentation");
#endif
    if (u::fn_0041c3e0(w) != 0) goto release_runtime;
#if defined(TH20_WEB)
    report_browser_stage("create-controllers");
#endif
    function_controller = u::make_function_controller(allocations, text_constants::function_allocation_site);
    sprite_controller = u::make_sprite_controller(allocations, text_constants::sprite_allocation_site);
#if defined(TH20_WEB)
    report_browser_stage("controllers-created");
#endif
    if (!is_windowed(g) || u::fn_0041d0f0(g) != 0) {
#if !defined(TH20_WEB)
        WINNLSEnableIME(nullptr, FALSE);
#endif
        hide_cursor();
    }
    w.clock_offset = 0.0;
    w.next_update_time = u::read_clock(w);
    w.current_time = w.next_update_time;
    w.previous_time = w.current_time;
    w.current_draw_time = u::read_clock(w);
    w.previous_draw_time = w.current_draw_time;
#if defined(TH20_WEB)
    focus_browser_canvas();
#else
    SetForegroundWindow(w.window); // complete original 0x0041b480 wrapper
#endif
#if defined(TH20_WEB)
    report_browser_stage("register-graphics-callbacks");
#endif
    loop_status = u::fn_004de1f0();
#if defined(TH20_WEB)
    report_browser_stage("graphics-callbacks-registered");
#endif
    if (loop_status == -1) goto finish_session;
    if (loop_status != 0) { loop_status = 2; goto finish_session; }
    loop_status = 0;
    set_draw_counter(w, 0xfc); // assembly PUSH -4; setter stores low byte
    w.flags |= 1u;
#if defined(TH20_WEB)
    report_browser_stage("enter-frame-loop");
    th20_web_set_frame_loop_active(1);
    emscripten_set_main_loop(browser_frame_loop, 0, false);
    report_browser_stage("frame-loop-registered");
    return 0;
#endif

poll_message: // 0x0041ea9c
    if (w.quit_requested != 0) goto finish_session;
#if defined(TH20_WEB)
    // Browser events are delivered between the recovered scheduler's timed
    // frames.  Its Sleep calls are Asyncify suspension points, so an extra
    // suspension here would stall before the first frame is rendered.
#else
    if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
        goto poll_message;
    }
#endif
    // The original calls the getter twice, so preserve the two reads.
    if (device(g) == nullptr) goto poll_message;
    cooperative_status = device(g)->TestCooperativeLevel(); // vtable +0x0c
    if (cooperative_status == S_OK) {
        if (!needs_device_reset(w)) {
            if ((w.flags & 4u) != 0) {
                loop_status = run_unlimited_frame(w);
            } else if (g.presentation.PresentationInterval == 1 && config.frame_skip == 0) {
                loop_status = run_present_paced_frame(w);
            } else {
                loop_status = run_timed_frame(w);
            }
#if defined(TH20_WEB)
            static unsigned browser_frame=0;
            report_browser_frame(++browser_frame,w.draw_counter,g.event_flags,g.field_0b0c,loop_status);
#endif
            if (loop_status != 0) goto finish_session;
            graphics_event_flags &= ~8u;
            goto poll_message;
        }
    } else if (cooperative_status != D3DERR_DEVICENOTRESET) {
        goto poll_message;
    }

    // 0x0041ebde..0x0041eeef, device recovery and display-mode transition.
    set_reset_delay(w, 10);
    if (needs_device_reset(w)) {
        if (display_mode(w) < 3) {
#if defined(TH20_WEB)
            g.window_rectangle={0,0,w.client_width,w.client_height};
#else
            GetWindowRect(w.window, &g.window_rectangle);
#endif
            g.presentation.Windowed = FALSE;
            g.presentation.BackBufferFormat = config.alternate_pixel_format ? D3DFMT_R5G6B5 : D3DFMT_A8R8G8B8;
        } else {
            g.presentation.BackBufferFormat = g.previous_backbuffer_format;
            g.presentation.FullScreen_RefreshRateInHz = 0;
            g.presentation.Windowed = TRUE;
        }
        u::fn_0041e050(w, 0);
    }
    u::fn_004dd840(g);
    u::fn_0041dbe0(sprite_controller);
    if (u::fn_0041c730(1) != 0) goto finish_session;
    u::fn_0041a2c0();
    u::fn_0041d9f0(sprite_controller);
    device_reset_countdown = 3;
    graphics_event_flags |= 8u;
    if (needs_device_reset(w)) {
#if defined(TH20_WEB)
        resize_browser_canvas(w.client_width,w.client_height);
        if(display_mode(w)<3 || display_mode(w)==8 || display_mode(w)==9) hide_cursor();
        else show_cursor();
        w.cursor_latch=0;
#else
        u::fn_004dbce0(g);
        if (display_mode(w) == 8 || display_mode(w) == 9) {
            SetWindowLongW(w.window, GWL_STYLE, 0);
            ShowWindow(w.window, SW_SHOW);
            const auto width = w.client_width;
            const auto height = w.client_height;
            // 0x0041ed4d calls 0x0040c6b0 with printf-like arguments, but
            // 0x0040c6b0 is an actual five-byte no-op. It does not print.
            SetWindowPos(w.window, nullptr, 0, 0, width, height, 0x40);
            WINNLSEnableIME(nullptr, FALSE);
            hide_cursor();
            w.cursor_latch = 0;
        } else if (display_mode(w) == 3) {
            SetWindowLongW(w.window, GWL_STYLE, 0x10cb0000);
            const auto width = w.client_width + 2 * GetSystemMetrics(7);
            auto height = w.client_height + 2 * GetSystemMetrics(8);
            height += GetSystemMetrics(4);
            SetWindowPos(w.window, nullptr, g.window_rectangle.left, g.window_rectangle.top, width, height, 0x60);
            ShowWindow(w.window, SW_SHOWNORMAL);
            WINNLSEnableIME(nullptr, TRUE);
            show_cursor();
        } else {
            const auto width = w.scaled_width;
            const auto height = w.scaled_height;
            SetWindowLongW(w.window, GWL_STYLE, static_cast<LONG>(0x90000000u));
            SetWindowPos(w.window, nullptr, 0, 0, width, height, 0x20);
            WINNLSEnableIME(nullptr, FALSE);
            hide_cursor();
            w.cursor_latch = 0;
        }
#endif
    }
#if defined(TH20_WEB)
    g.window_rectangle={0,0,w.client_width,w.client_height};
#else
    GetWindowRect(w.window, &g.window_rectangle);
#endif
    u::fn_004dbd70(g);
    set_device_reset(w, 0);
    goto poll_message;

finish_session: // 0x0041eef5
    config.saved_display_mode = display_mode(w);
    if (config.saved_display_mode >= 3) {
#if defined(TH20_WEB)
        g.window_rectangle={0,0,w.client_width,w.client_height};
#else
        GetWindowRect(w.window, &g.window_rectangle);
#endif
        config.saved_window_x = g.window_rectangle.left;
        config.saved_window_y = g.window_rectangle.top;
    }
    u::fn_004dd490(g);
    u::free_function_controller(allocations, function_controller);
    function_controller = nullptr;

release_runtime: // 0x0041ef5e, also reached by initial configuration failures
    u::fn_00426170(thread_registry);
    u::free_sprite_controller(allocations, sprite_controller);
    sprite_controller = nullptr;
    release_resource(g.resource_019c);
    release_resource(g.resource_01a0);
    release_resource(g.resource_01a4);
    release_device(g);
    release_direct3d(g);
    if (w.window) {
#if defined(TH20_WEB)
        w.window=nullptr;
#else
        ShowWindow(w.window, SW_HIDE);
        DestroyWindow(w.window);
        w.window = nullptr;
#endif
    }
    show_cursor();
    if (loop_status == 2) {
        u::log_restart(log_buffer);
        u::log_append(log_buffer, text_constants::restart);
        if (!is_windowed(g)) {
#if !defined(TH20_WEB)
            WINNLSEnableIME(nullptr, TRUE);
#endif
        }
        for (int i = 0; i < 60; ++i) {
#if !defined(TH20_WEB)
            if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
#endif
        }
        graphics_event_flags &= 0xffffff9fu;
        goto restart_runtime;
    }
    u::save_configuration(config);
    timeEndPeriod(1);
    // The original calls at 0x00548610/0x005486a0 are strcpy_s/strcat_s.
    // The assembled path is intentionally not passed to the following log
    // method: that is what the original 0x0041f106..0x0041f13d does.
    strcpy_s(log_path, sizeof(log_path), w.user_data_directory);
#if defined(TH20_WEB)
    const auto log_directory_length=std::strlen(log_path);
    strcpy_s(log_path+log_directory_length,sizeof(log_path)-log_directory_length,"log.txt");
#else
    strcat_s(log_path, sizeof(log_path), "log.txt");
#endif
    u::log_flush(log_buffer);
    restore_system_settings(w);
    if (allocations) u::destroy_allocations(allocations, 1);
    u::lock_registry_disable(lock_registry);
    u::destroy_listener(listener);
    // Proved MOV [ebp-0x1064],0 at 0x0041f18d, loaded into EAX at 0x0041f1a2.
    return 0;
}
}

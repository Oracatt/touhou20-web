#include "loading.hpp"
#include <stdexcept>
#include <xmmintrin.h>
#if defined(TH20_WEB)
#include "../program_entry/program_entry.hpp"
#include "../sprite_renderer/animation_file.hpp"
#include <coroutine>
#include <exception>
#include <cstdio>
#include <string>
#include <emscripten/emscripten.h>
#endif
namespace th20::source::gameplay {
namespace ps=player_state;
namespace {
game_session::Player& current(game_session::Session& session) {
    if(!session.contexts[0].current_player)throw std::logic_error("Loading requires selected current Player");
    return *session.contexts[0].current_player;
}
Profile& current_profile(LoadingServices& host,game_session::Session& session) {
    return host.profile(ps::read<int>(current(session),8),ps::read<int>(current(session),0xc)); //50fc50
}
int difficulty_index(const game_session::PlayerTable& table) {
    const int result=ps::difficulty(table);
    if(result<0||result>=6)throw std::out_of_range("Difficulty table index");
    return result;
}
std::size_t checked_offset(std::int64_t offset,std::size_t size) {
    // The original dereferences these offsets directly; corrupt saves fail
    // explicitly here. Valid original profile records retain their behavior.
    if(offset<0||static_cast<std::uint64_t>(offset)+size>sizeof(Profile))throw std::out_of_range("Profile field offset");
    return static_cast<std::size_t>(offset);
}
void session_score(game_session::Session& session,std::uint64_t value) {ps::write(session,0x60,value);} //4b8960
#if defined(TH20_WEB)
struct WebLoadingTask {
    struct promise_type {
        std::exception_ptr error;
        int result{};
        WebLoadingTask get_return_object(){return {std::coroutine_handle<promise_type>::from_promise(*this)};}
        std::suspend_always initial_suspend() noexcept{return {};}
        std::suspend_always final_suspend() noexcept{return {};}
        void return_value(int value) noexcept{result=value;}
        void unhandled_exception() noexcept{error=std::current_exception();}
    };
    std::coroutine_handle<promise_type> handle;
};
struct WebLoadingContinuation {
    GameController* game;
    WebLoadingTask task;
    scheduler::Node* node{};
    ~WebLoadingContinuation(){if(task.handle)task.handle.destroy();}
};
WebLoadingContinuation* web_loading=nullptr;
void web_loading_phase(const char* phase){EM_ASM({
    const value=UTF8ToString($0);const data=document.documentElement.dataset;
    data.th20LoadingStage=value;
    data.th20LoadingStatus=value==='complete'||value==='failed'?value:'loading';
},phase);}
bool web_animation_files_ready(){
    return sprite::animation_files_ready(*program_entry::sprite_controller,program_entry::graphics_state.event_flags);
}
void web_preload_animation(int slot,const char* name){
    auto& pe=program_entry::graphics_state;
    if(!sprite::load_animation_file(*program_entry::sprite_controller,slot,name,program_entry::log_buffer,pe.event_flags))
        throw std::runtime_error(std::string("Gameplay animation could not be loaded: ")+name);
}
int __cdecl resume_web_loading(void* value){
    auto* continuation=static_cast<WebLoadingContinuation*>(value);
    auto handle=continuation->task.handle;
    handle.resume();
    if(!handle.done())return 1;
    const auto error=handle.promise().error;
    const auto result=handle.promise().result;
    if(web_loading==continuation)web_loading=nullptr;
    delete continuation;
    if(error)std::rethrow_exception(error);
    web_loading_phase(result==0?"complete":"failed");
    return 0; // Scheduler retires this continuation callback after it returns.
}
#endif
}
#if defined(TH20_WEB)
#define TH20_LOADING_RETURN(value) co_return (value)
static WebLoadingTask load_gameplay_steps(GameController& game,LoadingServices& host) {
#else
#define TH20_LOADING_RETURN(value) return (value)
std::int32_t load_gameplay(GameController& game,LoadingServices& host) {
#endif
    auto& session=host.session();auto& table=session.player_table;auto& services=*game.services;
    const auto fail=[&]() {
        game.game_flags|=8u;host.finish_loading_display(false);
        runtime::detach_worker(services.worker());game.enable_callbacks();return -1;
    };
    host.begin_frame_interval();game.load_stage=0;game.clear_flag_6();++game.load_stage;game.game_flags|=4u;
#if defined(TH20_WEB)
    web_loading_phase("wait-sprites");
#endif
    while(host.sprite_task_pending()) {
        if(((host.graphics_events()>>5)&3u)!=0)TH20_LOADING_RETURN(fail());
#if defined(TH20_WEB)
        co_await std::suspend_always{};
#else
        host.sleep(1);
#endif
    }
#if defined(TH20_WEB)
    if(host.new_game_state()==0){const double until=host.read_clock()+0.060;while(host.read_clock()<until)co_await std::suspend_always{};}
#else
    if(host.new_game_state()==0)host.sleep(60);
#endif
    host.interrupt_surface(0,true);services.reset_clock_scale();
    controller->game_flags&=~0x80u;controller->game_flags&=~0x40u;game.game_flags&=~0x800000u;
    ps::set_table_counter(table,0x1ec,0,999999999);ps::set_table_counter(table,0x1f0,0,999999999);
    constexpr std::size_t selected_offsets[]={0x1c,0x24,0x20,0x28,0xc,0x14,0x10,0x18};
    for(int i=0;i<8;++i) {
        auto& destination=current(session);
        const auto value=host.selected_profile(i%4,ps::read<int>(current(session),8));
        ps::write(destination,selected_offsets[i],value);
    }
    host.configure_overlay(ps::read<int>(current(session),8));
    session.contexts[0].current_player=&table.players[0]; //40bbc0->488720->41df50
    if(host.new_game_state()==0) {
        if(ps::score(table.players[0])>ps::read<std::uint64_t>(session,0x60))session_score(session,ps::score(table.players[0]));
    } else {
        session.fields_74[1]=initial_stage_parameter[difficulty_index(table)];
        if(!selected_stage)throw std::logic_error("Selected stage must exist before loading");
        ps::set_table_counter(table,0x1f8,selected_stage->id,999); //4be3f0, does not change current+1f4
        ps::reset_players(table,host.bomb_observer());
        if(ps::stage(table)==7&&ps::difficulty(table)<4)table.field_1e0=4;
        const int character=ps::read<int>(current(session),8);
        // All three original branches select slot0 again, independently of the
        // eight earlier queries; the manager verifies its checksum per query.
        auto& profile=host.profile(character,host.selected_profile(0,character));
        const auto difficulty=ps::difficulty(table);
        if(session.mode==2) {
            const auto offset=checked_offset(0xb08ll+std::int64_t(ps::spell(table))*0xe0+0xd8,8);
            session_score(session,ps::read<std::uint64_t>(profile,offset));session.field_68=0;
        } else if(session.mode!=0) {
            const auto offset=checked_offset(0x76f8ll+std::int64_t(difficulty)*0x90+std::int64_t(ps::stage(table)-1)*0x10,8);
            session_score(session,ps::read<std::uint64_t>(profile,offset));session.field_68=0;
        } else {
            const auto offset=checked_offset(0x18ll+std::int64_t(difficulty)*400,8);
            session_score(session,ps::read<std::uint64_t>(profile,offset));
            auto& selected=current_profile(host,session);
            session.field_68=static_cast<std::int32_t>(ps::read<std::int8_t>(selected,checked_offset(std::int64_t(difficulty)*400+0x21,1)));
        }
        if(!(session.flags&0x10u))ps::set_table_counter(table,0x1e8,0,9);
        ps::set(current(session),0x4bdc60,0);ps::write(table.players[0],0,std::uint64_t{0});
        ps::reset_players(table,host.bomb_observer());ps::set(current(session),0x4be940,0);
        const auto index=difficulty_index(table);
        ps::write(table,0x218,meter_minimum[index]*100);ps::write(table,0x21c,meter_maximum[index]*100);
        const float lower=_mm_cvtss_f32(_mm_div_ss(_mm_cvtsi32_ss(_mm_setzero_ps(),ps::read<int>(table,0x218)),_mm_set_ss(100.0f)));
        ps::set_meter(table,lower);
        if(session.mode==2) {
            ps::set(current(session),0x4bdee0,7);ps::set(current(session),0x4bda30,7);ps::set(current(session),0x4bde90,0);
            ps::set_bombs(current(session),0,host.bomb_observer());
        } else if(session.mode!=0) {
            if(session.fields_74[0]==0) {
                ps::set(current(session),0x4bdee0,7);ps::set(current(session),0x4bda30,7);ps::set(current(session),0x4bde90,7);
            } else {
                ps::set(current(session),0x4bde90,static_cast<int>(session.fields_74[0]-1u));ps::set(current(session),0x4bdee0,7);
            }
        } else {
            ps::set(current(session),0x4bdee0,7);ps::set(current(session),0x4bda30,7);ps::set(current(session),0x4bde90,2);
            ps::set_bombs(current(session),2,host.bomb_observer());
        }
#if defined(TH20_WEB)
        // Player::initialize binds script zero inside its factory. Native file
        // loading waits for templates; explicitly stage that prerequisite here.
        char player_animation[32];
        std::snprintf(player_animation,sizeof(player_animation),"pl%02d.anm",ps::read<int>(current(session),8));
        web_loading_phase("player-resources");
        web_preload_animation(9,player_animation);
        while(!web_animation_files_ready())co_await std::suspend_always{};
#endif
        if(!host.create(Entity::fn_004ffff0,0,nullptr))TH20_LOADING_RETURN(fail());
        const int stage=ps::stage(table);
        const auto power=ps::starting_power(current(session));
        ps::set(current(session),0x4be0a0,session.mode==2||(stage>=2&&stage!=7)?power*4:power);
        session.flags&=~8u;
        if(game.restart()==0&&host.replay_selection()<0) {
            auto& selected=current_profile(host,session);
            if(ps::read<int>(selected,0x76a8)<9999999) {
                auto& selected_again=current_profile(host,session);
                ps::write(selected_again,0x76a8,ps::read<int>(selected_again,0x76a8)+1);
            }
        }
    }
    host.register_callbacks(game);game.configuration=host.configuration();
    if(!selected_stage)throw std::logic_error("Selected stage must exist before loading");
    select_stage(table,selected_stage->id);
    const char* background=stage_background(*selected_stage,current(session));
#if defined(TH20_WEB)
    web_loading_phase("stage-resources");
    web_preload_animation(6,selected_stage->logo);
    web_preload_animation(7,"bullet.anm");
    while(!web_animation_files_ready())co_await std::suspend_always{};
    web_loading_phase("create-entities");
#endif
    if(!(session.flags&4u)) {
        if(!host.create(Entity::fn_0050a930,game.restart_mode,host.replay_path())||
           !host.create(Entity::fn_00477880,0,background)||
           !host.create(Entity::fn_004b9090,0,nullptr)||
           !host.create(Entity::fn_00486630,0,nullptr)||
           !host.create(Entity::fn_004c5010,0,nullptr)||
           !host.create(Entity::fn_004d7e40,0,nullptr)||
           !host.create(Entity::fn_004e6b80,0,nullptr)||
           !host.create(Entity::fn_005108d0,0,nullptr))TH20_LOADING_RETURN(fail());
        host.create_auxiliary_owner();
    } else {
        host.reset_replay_owner();host.reset_hud_owner();
        if(!host.create(Entity::fn_00477880,0,background))TH20_LOADING_RETURN(fail());
    }
    if(!(session.flags&1u)&&!(session.flags&0x10u)) {
        if(!host.create(Entity::fn_004aba70,0,selected_stage->ecl_file))TH20_LOADING_RETURN(fail());
    } else host.reset_existing_stage();
#if defined(TH20_WEB)
    // Background and ECL factories queue stage-specific files; their callbacks
    // remain disabled until every texture and ANM template has been completed.
    web_loading_phase("stage-animation-templates");
    while(!web_animation_files_ready())co_await std::suspend_always{};
#endif
    if(!host.create(Entity::fn_00478300,0,nullptr)||!host.create(Entity::fn_00488b20,0,nullptr))TH20_LOADING_RETURN(fail());
    host.commit_progress();
    if(!(session.flags&0x20u)) {
        if(session.mode!=2)services.stop_music();
        host.queue_track(0,selected_stage->tracks[0]);host.queue_track(1,selected_stage->tracks[1]);
    }
    host.reset_frame_statistics();recovered::timer_set(game.frame_timer,0);
#if defined(TH20_WEB)
    web_loading_phase("audio-and-effects");
    while(host.audio_commands_pending())co_await std::suspend_always{};
    while(!host.effects_ready()||!web_animation_files_ready())co_await std::suspend_always{};
#else
    while(host.audio_commands_pending())host.sleep(16);
    while(!host.effects_ready())host.sleep(1);
#endif
    host.finish_entity_initialization();game.field_f4=60;host.finish_loading_display(true);
    game.game_flags&=~4u;session.flags&=~1u;session.flags&=~4u;session.flags&=~0x10u;
    runtime::detach_worker(services.worker());host.clear_slowdown_counter();services.clear_surface_callbacks();
    for(unsigned i=0;i<4;++i)host.interrupt_surface(i,false);
    if(controller->restart()==0&&session.mode==0) {
        auto& selected=current_profile(host,session);
        ps::write(selected,checked_offset(0x7701ll+std::int64_t(ps::difficulty(table))*0x90+std::int64_t(ps::stage(table)-1)*0x10,1),std::uint8_t{1});
    }
    if(controller->restart()==0)ps::write(session,0x2b0,host.read_clock());
    game.enable_callbacks();TH20_LOADING_RETURN(0);
}
#undef TH20_LOADING_RETURN
#if defined(TH20_WEB)
std::int32_t load_gameplay(GameController& game,LoadingServices& host){
    if(web_loading)throw std::logic_error("A gameplay loading continuation is already active");
    auto* continuation=new WebLoadingContinuation{&game,load_gameplay_steps(game,host)};
    web_loading=continuation;
    // Graphics priority 1 processes queued images/audio before this callback.
    // Game priority 19 stays disabled until load_gameplay_steps reaches its end.
    continuation->node=scheduler::register_callback(*program_entry::function_controller,
        program_entry::scheduler_environment,2,resume_web_loading,continuation,false,true);
    web_loading_phase("queued");
    return 0;
}
void cancel_web_gameplay_loading(GameController& game){
    if(!web_loading||web_loading->game!=&game)return;
    auto* continuation=web_loading;web_loading=nullptr;
    scheduler::remove(*program_entry::function_controller,program_entry::scheduler_environment,continuation->node);
    delete continuation;
}
#endif
}

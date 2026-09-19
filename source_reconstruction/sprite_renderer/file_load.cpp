#include "animation_file.hpp"
#include <stdexcept>
#include <atomic>
namespace th20::source::sprite {
AnimationFile* load_animation_file(Controller& controller,std::int32_t index,const char* name,runtime::Log& log,std::uint32_t& graphics_flags) {
    if(index<0||index>=42)throw std::out_of_range("ANM file slot outside original array");
    if(controller.files[index])return controller.files[index];
    auto* file=preload_animation_file(controller,index,name,log);if(!file)return nullptr;
    auto stage=std::atomic_ref<std::uint32_t>(file->fields_5c[0]);stage.store(1);
#if defined(TH20_WEB)
    // Native callers run on loading workers while the main thread performs
    // postload texture creation. In a single-threaded browser build the wait
    // would deadlock that main-thread task, so leave the staged file queued;
    // update_animation_file_tasks completes it on the next animation frame.
    (void)graphics_flags;
#else
    auto flags=std::atomic_ref<std::uint32_t>(graphics_flags);
    while(stage.load()!=0&&(flags.load()&0x60u)==0)Sleep(1);
#endif
    return file;
}
}

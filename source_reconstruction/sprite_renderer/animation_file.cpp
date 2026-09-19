#include "animation_file.hpp"
#include "../archive/resource_manager.hpp"
#include "file_text.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#if defined(TH20_WEB)
#include <emscripten/emscripten.h>
#endif
namespace th20::source::sprite {
namespace {
#if defined(TH20_WEB)
EM_JS(void, report_animation_preload, (const char* stage, const char* name), {
    void stage; void name;
});
EM_JS(void, report_animation_counts, (const char* name, unsigned textures, unsigned sprites, unsigned scripts, unsigned bytes), {
    void name; void textures; void sprites; void scripts; void bytes;
});
#endif
std::uint8_t* copy_bytes(const th20::source::Bytes& bytes) {
    auto* memory=static_cast<std::uint8_t*>(runtime::allocate_bytes(bytes.size()));
    if(!memory)throw std::bad_alloc();std::memcpy(memory,bytes.data(),bytes.size());return memory;
}
}
AnmCounts count_animation_entries(std::span<const std::uint8_t> data) {
    AnmCounts result;std::size_t offset=0;
    for(;;) {
        if(offset>data.size()||data.size()-offset<sizeof(AnmHeader))throw std::runtime_error("Truncated ANM header");
        const auto* header=reinterpret_cast<const AnmHeader*>(data.data()+offset);
        ++result.textures;result.sprites+=header->sprite_count;result.scripts+=header->script_count;
        if(!header->next_offset)break;
        if(header->next_offset<sizeof(AnmHeader)||header->next_offset>data.size()-offset)throw std::runtime_error("Invalid ANM entry link");
        offset+=header->next_offset;
    }
    return result;
}
AnimationFile* create_animation_file() {
    std::lock_guard<std::recursive_mutex> lock(runtime::shared_locks().slot(1));
    auto* storage=::operator new(sizeof(AnimationFile));
    std::memset(storage,0,sizeof(AnimationFile)); // original 4477b0 zeroes before 448a30
    return new(storage) AnimationFile{};
}
int preload_external_texture(AnimationFile& file,std::uint32_t index,AnmHeader* header,runtime::Log& log) {
    if(!header){runtime::log_error(log,file_text::s0056e428);return -1;}
    if(header->version!=8){runtime::log_error(log,file_text::s0056e464);return -1;}
    auto* name=reinterpret_cast<const char*>(header)+header->name_offset;
    if(!header->has_data&&name[0]!='@') {
        auto bytes=resources::read(name,true);
        if(!bytes){runtime::log_error(log,file_text::s0056e488,name);return -1;}
        file.textures[index].unknown_08=static_cast<std::uint32_t>(bytes->size());
        file.textures[index].unknown_04=reinterpret_cast<std::uint32_t>(copy_bytes(*bytes));
    }
    return 1; // Original 0x44ed2b returns ONE on every successful path.
}
int preload_animation_data(AnimationFile& file,const char* name,runtime::Log& log) {
#if defined(TH20_WEB)
    report_animation_preload("anm-read-enter",name);
#endif
    auto bytes=resources::read(name,false);if(!bytes)return -1;
#if defined(TH20_WEB)
    report_animation_preload("anm-read-returned",name);
#endif
    const auto counts=count_animation_entries(*bytes);
#if defined(TH20_WEB)
    report_animation_preload("anm-counted",name);
    report_animation_counts(name,counts.textures,counts.sprites,counts.scripts,bytes->size());
#endif
    file.bytes=copy_bytes(*bytes);
#if defined(TH20_WEB)
    report_animation_preload("anm-copied",name);
#endif
#if defined(TH20_WEB)
    const std::string_view full_name(name);
    const auto separator=full_name.find_last_of("/\\");
    const auto filename=separator==std::string_view::npos?full_name:full_name.substr(separator+1);
    const auto extension=filename.find_last_of('.');
    const auto stem=extension==std::string_view::npos?filename:filename.substr(0,extension);
    // The packed x86-compatible file structure leaves the 32-bit libc++ PMR
    // string with its ten-byte inline payload. Keep these diagnostic/name-match
    // fields inline; archive lookup continues to use the original `name` above.
    const auto inline_filename=filename.substr(0,std::min<std::size_t>(filename.size(),10));
    const auto inline_stem=stem.substr(0,std::min<std::size_t>(stem.size(),10));
    file.filename.assign(inline_filename.data(),inline_filename.size());
    file.stem.assign(inline_stem.data(),inline_stem.size());
#else
    const std::filesystem::path path(name);
    file.filename=path.filename().string();file.stem=path.stem().string();
#endif
    file.texture_count=static_cast<std::int32_t>(counts.textures);
    {
#if defined(TH20_WEB)
        report_animation_preload("anm-arrays-enter",name);
#endif
        std::lock_guard<std::recursive_mutex> lock(runtime::shared_locks().slot(1));
        file.textures=new TextureRecord[counts.textures]{};
        file.sprites=new SpriteData[counts.sprites]{};
        file.scripts=new AnmInstruction*[counts.scripts];
#if defined(TH20_WEB)
        report_animation_preload("anm-arrays-returned",name);
#endif
    }
    file.script_count=counts.scripts;file.sprite_count=counts.sprites;
    auto* header=reinterpret_cast<AnmHeader*>(file.bytes);std::uint32_t index=0;
#if defined(TH20_WEB)
    report_animation_preload("anm-external-enter",name);
#endif
    for(;;) {
        if(preload_external_texture(file,index,header,log)<0)return 0;
        // This surprising zero on an external-texture error is the actual
        // 0x44aa46..68 result. Templates remain null on that path.
        ++index;if(!header->next_offset)break;
        header=reinterpret_cast<AnmHeader*>(reinterpret_cast<std::uint8_t*>(header)+header->next_offset);
    }
#if defined(TH20_WEB)
    report_animation_preload("anm-external-returned",name);
#endif
    {
        std::lock_guard<std::recursive_mutex> lock(runtime::shared_locks().slot(1));
        file.templates=new Animation[counts.scripts];
        for(std::uint32_t i=0;i<counts.scripts;++i)construct_animation(file.templates[i]);
    }
#if defined(TH20_WEB)
    report_animation_preload("anm-preload-returned",name);
#endif
    return 0;
}
void release_texture_data(TextureRecord& record) {
    if(record.texture){record.texture->Release();record.texture=nullptr;}
    if(record.unknown_04){runtime::release_bytes(reinterpret_cast<void*>(record.unknown_04));record.unknown_04=0;}
}
}

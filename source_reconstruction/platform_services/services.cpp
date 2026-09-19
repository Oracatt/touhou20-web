#include "services.hpp"
#include "clock.hpp"
#include "text_constants.hpp"
#include <mmsystem.h>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#if defined(TH20_WEB)
#include <emscripten/emscripten.h>
#endif

namespace th20::source::platform {
namespace {
struct Handle {
    HANDLE value;
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
std::string configuration_path(const program_entry::WindowStatePrefix& w, const char* filename) {
    // The original uses MSVC std::filesystem's active file-API code page for
    // path construction/string(). The writer below separately forces CP932.
    return (std::filesystem::path(w.user_data_directory) / filename).string();
}
void copy_path(char* destination, const std::filesystem::path& source) {
    const auto value = source.string();
    if (strcpy_s(destination, MAX_PATH, value.c_str()) != 0)
        throw std::length_error("Path exceeds the original 260-byte bound");
}
void cp932_path(char* destination, const wchar_t* source) {
    if (!WideCharToMultiByte(932,0,source,-1,destination,MAX_PATH,nullptr,nullptr))
        throw std::runtime_error("Path could not be converted within the original CP932 buffer");
}
}
std::optional<Bytes> read_loose_file(const char* name) {
    std::lock_guard<std::recursive_mutex> lock(runtime::shared_locks().slot(2));
#if defined(TH20_WEB)
    std::ifstream file(name,std::ios::binary|std::ios::ate);if(!file)return std::nullopt;
    const auto end=file.tellg();if(end<0||static_cast<std::uint64_t>(end)>0xffffffe0u)return std::nullopt;
    Bytes result(static_cast<std::size_t>(end));file.seekg(0);
    if(!result.empty())file.read(reinterpret_cast<char*>(result.data()),static_cast<std::streamsize>(result.size()));
    result.resize(static_cast<std::size_t>(file.gcount()));return result;
#else
    const std::filesystem::path path(name);
    Handle file{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN|FILE_ATTRIBUTE_NORMAL,nullptr)};
    if(file.value==INVALID_HANDLE_VALUE) return std::nullopt;
    const auto size=GetFileSize(file.value,nullptr);
    // The original allocation wrapper rejects >0xffffffe0 bytes. Its exception
    // diagnostics call 0x40c6b0, a verified no-op in this release binary.
    if(size>0xffffffe0u) return std::nullopt;
    Bytes result;
    try { result.resize(size); } catch(const std::bad_alloc&) { return std::nullopt; }
    DWORD read=0; ReadFile(file.value,result.data(),size,&read,nullptr);
    result.resize(read); return result;
#endif
}
int write_loose_file(const char* name, const void* data, std::uint32_t size) {
    std::lock_guard<std::recursive_mutex> lock(runtime::shared_locks().slot(2));
#if defined(TH20_WEB)
    const std::filesystem::path path(name);std::error_code error;
    if(path.has_parent_path())std::filesystem::create_directories(path.parent_path(),error);
    std::ofstream file(path,std::ios::binary|std::ios::trunc);if(!file)return -1;
    file.write(static_cast<const char*>(data),size);return file?0:-2;
#else
    wchar_t path[MAX_PATH+2]{};
    MultiByteToWideChar(932,0,name,-1,path,MAX_PATH);
    Handle file{CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if(file.value==INVALID_HANDLE_VALUE) {
        wchar_t* message=nullptr;
        FormatMessageW(0x1300,nullptr,GetLastError(),0x400,reinterpret_cast<LPWSTR>(&message),0,nullptr);
        // Original 0x40c6b0 consumes no arguments and emits no output.
        LocalFree(message); return -1;
    }
    DWORD written=0; WriteFile(file.value,data,size,&written,nullptr);
    return written==size?0:-2;
#endif
}
int load_configuration(program_entry::GraphicsStatePrefix& g, const program_entry::WindowStatePrefix& w,
                       runtime::Log& log, const char* filename) {
    g.configuration=default_configuration();
    const auto path=configuration_path(w,filename);
    auto data=read_loose_file(path.c_str());
    if(!data) runtime::log_printf(log,text::s00571c40);
    else decode_configuration(g.configuration,data->data(),data->size());
    g.disable_vsync=0;
    const auto flags=g.configuration.flags;
    if(flags&(1u<<2)) runtime::log_printf(log,text::s00571cb0);
    // 0x412540 is a recovered constant-zero release-build capability query;
    // its associated 16-bit-texture log is unreachable in this binary.
    if(g.presentation.Windowed) runtime::log_printf(log,text::s00571cf4);
    if(flags&(1u<<1)) runtime::log_printf(log,text::s00571d14);
    if(flags&(1u<<3)) runtime::log_printf(log,text::s00571d3c);
    if(flags&(1u<<4)) runtime::log_printf(log,text::s00571d74);
    if(flags&(1u<<5)) { runtime::log_printf(log,text::s00571d94); g.disable_vsync=1; }
    if(flags&(1u<<6)) runtime::log_printf(log,text::s00571dac);
    if(write_loose_file(path.c_str(),&g.configuration,sizeof(g.configuration))==0) return 0;
    // 0x4dc496 incorrectly omits the argument for its "%s" failure diagnostic;
    // supply the actual path instead of reproducing an undefined stack read.
    runtime::log_error(log,text::s00571dd0,path.c_str());
    runtime::log_error(log,text::s00571df0); return -1;
}
void save_configuration(const Configuration& configuration, const program_entry::WindowStatePrefix& w) {
    const auto path=configuration_path(w,"th20.cfg");
    write_loose_file(path.c_str(),&configuration,sizeof(configuration));
}
void initialize_directories(program_entry::WindowStatePrefix& w, runtime::Log& log,
                            const wchar_t* appdata, const wchar_t* module_filename) {
#if defined(TH20_WEB)
    (void)appdata;(void)module_filename;
    strcpy_s(w.user_data_directory,sizeof w.user_data_directory,"/th20-data");
    strcpy_s(w.module_directory,sizeof w.module_directory,"/game");
    std::filesystem::create_directories("/th20-data/replay");
    std::filesystem::create_directories("/th20-data/snapshot");
    runtime::log_printf(log,"%s\n",w.user_data_directory);
#else
    cp932_path(w.user_data_directory,appdata);
    if(*appdata) {
        auto path=std::filesystem::path(w.user_data_directory)/"ShanghaiAlice";
        std::filesystem::create_directory(path);
        path/="th20"; std::filesystem::create_directory(path);
        copy_path(w.user_data_directory,path);
        std::filesystem::create_directory(std::filesystem::path(w.user_data_directory)/"replay");
        std::filesystem::create_directory(std::filesystem::path(w.user_data_directory)/"snapshot");
        runtime::log_printf(log,"%s\n",w.user_data_directory);
    }
    cp932_path(w.module_directory,module_filename);
    copy_path(w.module_directory,std::filesystem::path(w.module_directory).parent_path());
#endif
}
int initialize_platform(program_entry::WindowStatePrefix& w, runtime::Log& log) {
#if defined(TH20_WEB)
    w.saved_screen_saver=w.saved_low_power=w.saved_power_off=0;
    w.performance_frequency=1000000;
    w.performance_origin=static_cast<std::uint64_t>(emscripten_get_now()*1000.0);
    initialize_directories(w,log,L"/th20-data",L"/game/th20.wasm");return 0;
#else
    // The original passes adjacent BYTE addresses to BOOL-output Win32 APIs.
    // Each four-byte write stays inside this recovered WindowState object;
    // preserve that ordering, including overwritten display-mode bytes.
    auto* storage=reinterpret_cast<unsigned char*>(&w);
    SystemParametersInfoW(0x10,0,storage+offsetof(program_entry::WindowStatePrefix,saved_screen_saver),0);
    SystemParametersInfoW(0x53,0,storage+offsetof(program_entry::WindowStatePrefix,saved_low_power),0);
    SystemParametersInfoW(0x54,0,storage+offsetof(program_entry::WindowStatePrefix,saved_power_off),0);
    SystemParametersInfoW(0x11,0,nullptr,2);
    SystemParametersInfoW(0x55,0,nullptr,2);
    SystemParametersInfoW(0x56,0,nullptr,2);
    QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&w.performance_frequency));
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&w.performance_origin));
    wchar_t appdata[4096]{}; GetEnvironmentVariableW(L"APPDATA",appdata,4096);
    wchar_t module[4096]{}; GetModuleFileNameW(nullptr,module,4096);
    initialize_directories(w,log,appdata,module);
    return 0; // Original 0x41b85c returns zero after both path operations.
#endif
}
double read_clock(program_entry::WindowStatePrefix& w) {
    std::lock_guard<std::recursive_mutex> lock(runtime::shared_locks().slot(5));
#if defined(TH20_WEB)
    const auto sampled=static_cast<std::uint64_t>(emscripten_get_now()*1000.0);
    return performance_clock(sampled,w.performance_origin,w.performance_frequency,w.clock_offset);
#else
    if(w.performance_frequency) {
        LARGE_INTEGER sampled{}; QueryPerformanceCounter(&sampled);
        std::uint64_t value; std::memcpy(&value,&sampled,8);
        return performance_clock(value,w.performance_origin,w.performance_frequency,w.clock_offset);
    }
    return multimedia_clock(timeGetTime(),w.clock_offset);
#endif
}
}

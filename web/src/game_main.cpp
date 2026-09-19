namespace th20::source::program_entry {
int recovered_win_main(void*,void*,char*,int);
}
#include <emscripten/emscripten.h>
#include <exception>

namespace {
EM_JS(void,report_startup_exception,(const char* message),{
    const text=UTF8ToString(message);
    console.error('TH20 startup exception:',text);
    window.dispatchEvent(new CustomEvent('th20-error',{detail:text}));
});
}

int main() {
    try {
        return th20::source::program_entry::recovered_win_main(nullptr,nullptr,nullptr,0);
    } catch(const std::exception& error) {
        report_startup_exception(error.what());return 1;
    } catch(...) {
        report_startup_exception("Unknown C++ exception during recovered game startup");return 2;
    }
}

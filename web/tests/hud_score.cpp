// Exercise the actual recovered formatting path with the WebAssembly CRT
// compatibility layer. No renderer startup or original game code is required.
#include "../../source_reconstruction/text_renderer/text.hpp"
#include "../../source_reconstruction/program_entry/program_entry.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace th20::source::program_entry { WindowStatePrefix window_state{}; }

int main() {
    namespace t = th20::source::text;
    namespace pe = th20::source::program_entry;
    // Like the existing native text oracle, this fixture uses only the recovered
    // Line storage and formatting fields, without constructing graphics owners.
    auto* renderer = static_cast<t::Renderer*>(std::calloc(1, sizeof(t::Renderer)));
    if (!renderer) return 2;
    pe::window_state.scale = 2.f;
    renderer->fields_1a1d4[3] = 10;
    renderer->fields_1a1d4[6] = 3;
    renderer->fields_1a1d4[8] = 2;
    renderer->fields_1a1d4[9] = 1;
    renderer->color = 0xff001080;
    renderer->shadow_color = 0xd0ffffff;
    renderer->scale_x = renderer->scale_y = 1.f;

    struct Case { std::uint64_t score; int digit; const char* expected; };
    const Case cases[] {
        {0, 0, "0"}, {1, 0, "10"}, {99, 9, "999"},
        {100, 0, "1,000"}, {123456789, 7, "1,234,567,897"},
        {999999999, 9, "9,999,999,999"}
    };
    unsigned checks = 0;
    for (const auto& entry : cases) {
        renderer->line_count = 0;
        renderer->write_grouped_score({620, 64, 0}, entry.score, entry.digit);
        if (renderer->line_count != 2) return 3;
        for (int i = 0; i < 2; ++i) {
            const auto& line = renderer->lines[i];
            if (std::strcmp(line.text, entry.expected) != 0) {
                std::fprintf(stderr, "score %llu/%d: expected '%s', got '%s'\n",
                    static_cast<unsigned long long>(entry.score), entry.digit,
                    entry.expected, line.text);
                return 4;
            }
            if (line.font != (i == 0 ? 11 : 10) || line.layer != 3 ||
                line.color != (i == 0 ? renderer->shadow_color : renderer->color) ||
                line.position.x != 1240.f || line.position.y != 128.f ||
                line.align_x != 2 || line.align_y != 1) return 5;
            ++checks;
        }
    }
    renderer->line_count = 0;
    renderer->write_ascii({620, 42, 0}, "9,876,543,210");
    if (renderer->line_count != 2 ||
        std::strcmp(renderer->lines[0].text, "9,876,543,210") != 0 ||
        std::strcmp(renderer->lines[1].text, "9,876,543,210") != 0) return 6;
    ++checks;
    renderer->line_count = 320;
    renderer->write_grouped_score({620, 64, 0}, 123456, 7);
    if (renderer->line_count != 320) return 7;
    ++checks;
    std::free(renderer);
    std::printf("HUD score formatting: %u checks passed\n", checks);
}

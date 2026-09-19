#include "archive.hpp"

#include <emscripten/emscripten.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace {
std::unique_ptr<th20::source::Archive> archive;
th20::source::Bytes extracted;
std::string last_error;

void set_error(const char* message) {
    last_error = message != nullptr ? message : "Unknown WebAssembly error";
}

template <typename Function>
int guarded(Function&& function) noexcept {
    try {
        last_error.clear();
        function();
        return 1;
    } catch (const std::exception& error) {
        set_error(error.what());
    } catch (...) {
        set_error("Unknown C++ exception in the WebAssembly runtime");
    }
    return 0;
}
} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* th20_runtime_version() noexcept {
    return "th20-web-resource-runtime/1";
}

EMSCRIPTEN_KEEPALIVE
const char* th20_last_error() noexcept {
    return last_error.c_str();
}

EMSCRIPTEN_KEEPALIVE
int th20_load_archive(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size < 16) {
        set_error("The selected file is too small to be a THA1 archive");
        return 0;
    }
    if (size > 0x7fffffffU) {
        set_error("The selected archive exceeds the recovered 32-bit format limit");
        return 0;
    }
    return guarded([&] {
        th20::source::Bytes bytes(data, data + size);
        auto parsed = std::make_unique<th20::source::Archive>(std::move(bytes));
        archive = std::move(parsed);
        extracted.clear();
    });
}

EMSCRIPTEN_KEEPALIVE
std::size_t th20_archive_entry_count() noexcept {
    return archive != nullptr ? archive->entries().size() : 0;
}

EMSCRIPTEN_KEEPALIVE
const char* th20_archive_entry_name(std::size_t index) noexcept {
    if (archive == nullptr || index >= archive->entries().size()) {
        return nullptr;
    }
    return archive->entries()[index].name.c_str();
}

EMSCRIPTEN_KEEPALIVE
std::uint32_t th20_archive_entry_size(std::size_t index) noexcept {
    if (archive == nullptr || index >= archive->entries().size()) {
        return 0;
    }
    return archive->entries()[index].size;
}

EMSCRIPTEN_KEEPALIVE
std::uint32_t th20_archive_catalog_offset() noexcept {
    return archive != nullptr ? archive->catalog_offset() : 0;
}

EMSCRIPTEN_KEEPALIVE
int th20_extract_archive_entry(std::size_t index) noexcept {
    if (archive == nullptr) {
        set_error("No THA1 archive is loaded");
        return 0;
    }
    if (index >= archive->entries().size()) {
        set_error("The requested archive entry index is out of range");
        return 0;
    }
    return guarded([&] { extracted = archive->read(index); });
}

EMSCRIPTEN_KEEPALIVE
const std::uint8_t* th20_extracted_data() noexcept {
    return extracted.empty() ? nullptr : extracted.data();
}

EMSCRIPTEN_KEEPALIVE
std::size_t th20_extracted_size() noexcept {
    return extracted.size();
}

EMSCRIPTEN_KEEPALIVE
std::uint32_t th20_extracted_fnv1a() noexcept {
    std::uint32_t hash = 2166136261U;
    for (const auto byte : extracted) {
        hash ^= byte;
        hash *= 16777619U;
    }
    return hash;
}

EMSCRIPTEN_KEEPALIVE
void th20_unload_archive() noexcept {
    extracted.clear();
    archive.reset();
    last_error.clear();
}

} // extern "C"

int main() {
    return 0;
}


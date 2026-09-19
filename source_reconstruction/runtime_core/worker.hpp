#pragma once
#include <atomic>
#include <thread>
#include <cstddef>

namespace th20::source::runtime {
// Original constructor 0x0040b780: std::jthread default construction at+0,
// then atomic<bool>(false) at+12. MSVC's x86 jthread is thread8+stop_source4.
// This type owns a real C++ thread; it contains no original instruction bytes.
struct Worker {
    std::jthread thread;
#if defined(TH20_WEB)
    // libc++ wasm32 jthread is eight bytes; keep the recovered close flag at
    // +0x0c so every object embedding Worker retains its original offsets.
    std::uint32_t web_jthread_abi_padding;
#endif
    std::atomic<bool> close_requested{false};
    // User-provided construction prevents value-initialization from clearing
    // tail padding. Original40b780 initializes12 thread bytes and bool+12 only.
    Worker() noexcept : thread(), close_requested(false) {}
    Worker(const Worker&)=delete;
    Worker& operator=(const Worker&)=delete;
    ~Worker(); // 40b980: close_and_join, then normal jthread destruction
};
#if defined(TH20_WEB)
static_assert(offsetof(Worker,close_requested)==12 && sizeof(Worker)==16);
#else
static_assert(sizeof(std::jthread)==12 && offsetof(Worker,close_requested)==12 && sizeof(Worker)==16);
#endif
void join_worker(Worker&);                  // 40bcf0
void detach_worker(Worker&);                // 4ba8b0 -> 40bc60
void sync_close_worker(Worker&);            // 4d9e30 composition
}

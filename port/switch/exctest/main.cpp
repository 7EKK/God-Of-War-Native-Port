// Switch probe: do C++ exceptions unwind, and how many std::threads can the process create? Log: sdmc:/switch/gow/exctest.txt
#include <switch.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

static std::FILE *out;
__attribute__((noinline)) static void thrower(int v) { if (v) throw std::runtime_error("probe"); }

int main()
{
    out = std::fopen("sdmc:/switch/gow/exctest.txt", "w");
    std::setvbuf(out, nullptr, _IONBF, 0);
    try { thrower(1); std::fprintf(out, "exception: not thrown?\n"); }
    catch (const std::exception &e) { std::fprintf(out, "exception: caught '%s'\n", e.what()); }
    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;
    for (int i = 0; i < 64; ++i)
    {
        try { threads.emplace_back([&stop] { while (!stop) std::this_thread::sleep_for(std::chrono::duration<double>(0.01)); }); }
        catch (const std::system_error &e) { std::fprintf(out, "thread %d failed: %d %s\n", i, e.code().value(), e.what()); break; }
    }
    std::fprintf(out, "threads created: %zu\n", threads.size());
    stop = true;
    for (auto &t : threads) t.join();
    std::fprintf(out, "done\n");
    std::fclose(out);
    return 0;
}

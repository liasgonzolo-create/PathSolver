#pragma once
// cfg `profsample=1` (off by default): where the main thread spends a session, by module.
//
// A second thread wakes about once a millisecond, suspends the main thread, reads its instruction
// pointer and resumes it. At the session's end the samples are attributed to the module they fell
// in (this mod, Geode, the game, cocos2d, the system) and the counts are printed; every sample is
// also written to prof_samples.tsv (address, module, module base, tick) so the addresses inside a
// module can be resolved offline against its PDB.
//
// Only the leaf is read: a GD function the mod calls is counted as GD. Nothing is allocated or
// locked while the main thread is suspended -- the buffer is reserved up front -- because the
// main thread may be holding the heap lock at the moment it is stopped.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "mod/itermap.hpp"

#ifdef GEODE_IS_WINDOWS
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

namespace p1::profsample {

#ifdef GEODE_IS_WINDOWS
struct Sample { unsigned long long ip; long long tick; };
constexpr size_t kMax = 1u << 20;   // ~17 minutes at 1 kHz
inline std::vector<Sample> g_buf;
inline std::atomic<size_t> g_n{0};
inline std::atomic<bool> g_run{false};
inline std::thread g_thr;
inline HANDLE g_main = nullptr;
inline long long g_tick0 = 0;
inline std::chrono::steady_clock::time_point g_t0{};

inline void start() {
    if (g_run) return;
    g_buf.assign(kMax, Sample{});
    g_n = 0;
    g_main = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                        FALSE, GetCurrentThreadId());
    if (!g_main) { writeResult("profsample: OpenThread failed - off"); return; }
    g_tick0 = itermap::g_nowTick;
    g_t0 = std::chrono::steady_clock::now();
    timeBeginPeriod(1);
    g_run = true;
    g_thr = std::thread([] {
        while (g_run) {
            Sleep(1);
            if (SuspendThread(g_main) == (DWORD)-1) continue;
            CONTEXT c{};
            c.ContextFlags = CONTEXT_CONTROL;
            const BOOL ok = GetThreadContext(g_main, &c);
            const long long t = itermap::g_nowTick;
            ResumeThread(g_main);
            if (!ok) continue;
            const size_t i = g_n.load(std::memory_order_relaxed);
            if (i >= kMax) continue;
            g_buf[i] = Sample{c.Rip, t};
            g_n.store(i + 1, std::memory_order_relaxed);
        }
    });
    writeResult("profsample: started at t=" + std::to_string(g_tick0));
}

inline std::string moduleOf(unsigned long long ip, unsigned long long& base) {
    HMODULE m = nullptr;
    base = 0;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(ip), &m) || !m)
        return "?";
    base = reinterpret_cast<unsigned long long>(m);
    wchar_t w[MAX_PATH];
    const DWORD n = GetModuleFileNameW(m, w, MAX_PATH);
    std::wstring ws(w, n);
    const size_t s = ws.find_last_of(L"\\/");
    if (s != std::wstring::npos) ws = ws.substr(s + 1);
    std::string out;
    for (wchar_t ch : ws) out += (ch < 128) ? (char)ch : '?';
    return out;
}

inline void stopAndReport(const char* why) {
    if (!g_run) return;
    g_run = false;
    if (g_thr.joinable()) g_thr.join();
    timeEndPeriod(1);
    CloseHandle(g_main);
    g_main = nullptr;
    const double ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - g_t0).count();
    const size_t n = g_n.load();
    std::map<std::string, long long> byMod;
    std::map<unsigned long long, std::pair<std::string, unsigned long long>> cache;
    FILE* f = std::fopen((std::string(DATA_DIR) + "/prof_samples.tsv").c_str(), "w");
    if (f) std::fprintf(f, "ip\tmodule\tbase\ttick\n");
    for (size_t i = 0; i < n; ++i) {
        const auto& s = g_buf[i];
        const unsigned long long page = s.ip >> 12;
        auto it = cache.find(page);
        if (it == cache.end()) {
            unsigned long long base = 0;
            std::string m = moduleOf(s.ip, base);
            it = cache.emplace(page, std::make_pair(m, base)).first;
        }
        ++byMod[it->second.first];
        if (f) std::fprintf(f, "%llx\t%s\t%llx\t%lld\n", s.ip, it->second.first.c_str(),
                            it->second.second, s.tick);
    }
    if (f) std::fclose(f);
    std::string line = "profsample: " + std::string(why) + " - " + std::to_string(n)
        + " samples over " + std::to_string((long long)ms) + " ms, t=" + std::to_string(g_tick0)
        + ".." + std::to_string(itermap::g_nowTick) + ":";
    std::vector<std::pair<long long, std::string>> v;
    for (auto& [m, c] : byMod) v.push_back({c, m});
    std::sort(v.rbegin(), v.rend());
    for (auto& [c, m] : v) {
        char b[96];
        std::snprintf(b, sizeof(b), " %s=%lld (%.1f%%)", m.c_str(), c, n ? 100.0 * c / n : 0.0);
        line += b;
    }
    writeResult(line);
    g_buf.clear();
    g_buf.shrink_to_fit();
}

// Called once a frame from the update hook, on the main thread.
inline void frame(bool active) {
    if (!itermap::g_profSample) return;
    if (active && !g_run) start();
    else if (!active && g_run) stopAndReport("session over");
}
#else
inline void frame(bool) {}
#endif

}  // namespace p1::profsample

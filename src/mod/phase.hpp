#pragma once
// What the solve loop is doing right now, by name, and how long each stretch took.
//
// The second HUD bar (progress_bars.hpp) only knows four measurable states -- the dp search, a
// recording pass, a section search, a flight. Everything else the loop does (installing a plan,
// resetting, replaying through the model, the fixup recorder...) used to show nothing, and a
// stretch of ten seconds looked like a hang. A phase is a name plus the time it began; the HUD
// prints it when none of the four applies, and a stretch that took a second or more is written to
// result.txt as one line:
//
//     phase: <name> <ms> ms (t=<tick>, iter <n>)
//
// Two threads are in play (the render/main thread resets and swaps the level, the solver thread
// searches and replays), so a phase is an ENTRY of its own: it is added when it begins and removed
// when it ends, and nothing is ever "restored" -- an earlier version saved the interrupted phase and
// put it back with a fresh start time, which turned a reset on the main thread into a phantom
// "resetting" of seconds whenever a solver-thread phase ended inside it. Every interval has an id handed out at
// begin and every end targets that id only, so a late end of an old interval cannot close a later one of the
// same name. Each entry is logged with
// its own whole duration (a phase inside another is counted in both), and the HUD shows the entry
// that began last. The log lines cover only the phases that ran a second or more: their sum is not
// the time spent.
// Read by the render thread, written by both: one mutex, held only for a copy.
#include <algorithm>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace p1 {
namespace phase {

constexpr long long kLogMs = 1000;

struct Cur {
    std::string name;
    std::chrono::steady_clock::time_point t0{};
    long long tick = -1;
    int iter = -1;
};
using Id = unsigned long long;
struct Entry {
    Id id = 0;
    std::thread::id tid;
    bool loose = false;   // begin()/end(): one per thread, replaced by the next begin() of that thread
    Cur c;
};
inline std::mutex g_mu;
inline std::vector<Entry> g_active;
inline Id g_nextId = 1;

// Test seam (phase_fixture.cpp): called where a check and its action must not be separable. Null in
// the mod.
inline void (*g_testGap)() = nullptr;

inline Cur make(const char* name, long long tick, int iter) {
    Cur c;
    c.name = name ? name : "";
    c.t0 = std::chrono::steady_clock::now();
    c.tick = tick;
    c.iter = iter;
    return c;
}

// The caller holds g_mu.
inline void logEnd(const Cur& c) {
    const long long ms = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - c.t0).count();
    if (ms >= kLogMs)
        writeResult("phase: " + c.name + " " + std::to_string(ms) + " ms (t=" + std::to_string(c.tick)
                    + ", iter " + std::to_string(c.iter) + ")");
}
inline bool removeLocked(std::vector<Entry>::iterator it) {
    logEnd(it->c);
    g_active.erase(it);
    return true;
}

// begin()/end(): the calling thread's one loose phase (the slice's waits span several frames, so no block
// holds them). A begin() while the thread has one ends it first.
inline Id begin(const char* name, long long tick = -1, int iter = -1) {
    std::lock_guard<std::mutex> lk(g_mu);
    const auto me = std::this_thread::get_id();
    for (auto it = g_active.begin(); it != g_active.end(); ++it)
        if (it->loose && it->tid == me) { removeLocked(it); break; }
    Entry e;
    e.id = g_nextId++;
    e.tid = me;
    e.loose = true;
    e.c = make(name, tick, iter);
    const Id id = e.id;
    g_active.push_back(std::move(e));
    return id;
}
inline void end() {
    std::lock_guard<std::mutex> lk(g_mu);
    const auto me = std::this_thread::get_id();
    for (auto it = g_active.begin(); it != g_active.end(); ++it)
        if (it->loose && it->tid == me) { removeLocked(it); return; }
}

// Ends the interval `id` and nothing else: an id is handed out once at begin, so a late end for an old
// interval cannot close a later interval of the same name. The watcher that sees the dp's first
// progress report holds the id of the search start it is ending; the Scope that began it then finds its
// entry gone and does nothing.
inline void endId(Id id) {
    std::lock_guard<std::mutex> lk(g_mu);   // the check and the end under one hold
    for (auto it = g_active.begin(); it != g_active.end(); ++it)
        if (it->id == id) {
            if (g_testGap) g_testGap();
            removeLocked(it);
            return;
        }
}

// The phase to show and the seconds it has run: the entry that began last; false when none is active.
inline bool current(std::string& name, double& seconds) {
    std::lock_guard<std::mutex> lk(g_mu);
    const Entry* best = nullptr;
    for (const Entry& e : g_active)
        if (!best || e.c.t0 >= best->c.t0) best = &e;
    if (!best) return false;
    name = best->c.name;
    seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - best->c.t0).count();
    return true;
}

// A phase for the length of a block, on whichever thread runs it.
class Scope {
public:
    Scope(const char* name, long long tick = -1, int iter = -1) {
        std::lock_guard<std::mutex> lk(g_mu);
        Entry e;
        e.id = m_id = g_nextId++;
        e.tid = std::this_thread::get_id();
        e.c = make(name, tick, iter);
        g_active.push_back(std::move(e));
    }
    Id id() const { return m_id; }
    ~Scope() {
        std::lock_guard<std::mutex> lk(g_mu);
        for (auto it = g_active.begin(); it != g_active.end(); ++it)
            if (it->id == m_id) { removeLocked(it); return; }   // gone already when endId took it
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    Id m_id = 0;
};

}  // namespace phase
}  // namespace p1

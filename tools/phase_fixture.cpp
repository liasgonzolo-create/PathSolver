// GD-free fixture for src/mod/phase.hpp.
//
//   cl /std:c++17 /EHsc /I src tools\phase_fixture.cpp   (from a VS developer prompt)
//
// 1. Two threads. The main thread is "resetting"; the solver thread enters and leaves "resim" inside
//    it. Nothing may be left behind (an earlier version saved the interrupted phase on the second
//    thread and put it back with a fresh start time: a phantom "resetting" until the next switch).
//    Both orders of leaving are checked, and the log carries each phase once with its own duration.
// 2. endId ends one interval by its id, under one hold; the test seam forces another thread to begin a phase at
//    the point between the check and the end, and that phase survives. A late end for an old interval never
//    closes a later interval of the same name.
// 3. A phase of a second or more is logged once, with its tick and iter; a short one is not.
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace p1 {
inline std::vector<std::string> g_lines;
inline void writeResult(const std::string& text, bool = false) { g_lines.push_back(text); }
}  // namespace p1

#include "mod/phase.hpp"

using namespace p1;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++g_fail; } } while (0)

static std::string cur() {
    std::string n; double s = 0;
    return phase::current(n, s) ? n : std::string();
}
static void ms(int n) { std::this_thread::sleep_for(std::chrono::milliseconds(n)); }

static std::thread g_other;
static void switchFromGap() {
    g_other = std::thread([] { phase::begin("next"); });
    ms(100);   // under endIf the lock is held, so the other thread cannot get in before it has finished
}

int main() {
    // 1a. main resets; the solver thread's resim begins and ends inside it. Main still shows "resetting",
    //     and after main leaves nothing is left.
    {
        phase::Scope reset("resetting", 10, 1);
        ms(20);
        std::thread worker([] {
            phase::Scope s("resim", 20, 1);
            ms(30);
        });
        ms(10);
        CHECK(cur() == "resim");   // the entry that began last
        worker.join();
        CHECK(cur() == "resetting");   // the worker's end restores nothing and invents nothing
    }
    CHECK(cur().empty());

    // 1b. the other order: the solver's phase outlives the reset; when it ends nothing is left of "resetting".
    {
        std::thread worker;
        {
            phase::Scope reset("resetting", 10, 1);
            worker = std::thread([] {
                phase::Scope s("resim", 20, 1);
                ms(80);
            });
            ms(20);
        }   // main leaves while the worker is inside "resim"
        CHECK(cur() == "resim");
        worker.join();
        CHECK(cur().empty());   // the old version restored "resetting" here, with a fresh t0
    }

    // 1c. a long phase on the worker inside a short one on main: each is logged once with its own time.
    p1::g_lines.clear();
    {
        phase::Scope reset("resetting", 5, 2);
        std::thread worker([] {
            phase::Scope s("resim", 6, 2);
            ms(1100);
        });
        worker.join();
    }
    int nReset = 0, nResim = 0;
    for (const std::string& l : p1::g_lines) {
        if (l.rfind("phase: resetting ", 0) == 0) ++nReset;
        if (l.rfind("phase: resim ", 0) == 0) ++nResim;
    }
    // "resetting" lasted as long as the resim here (main waited), so both are real second-long phases; the
    // point is each appears once and nothing is logged after both are gone.
    CHECK(nReset == 1 && nResim == 1);
    const size_t before = p1::g_lines.size();
    ms(50);
    CHECK(p1::g_lines.size() == before && cur().empty());

    // 2. endId under one hold: another thread begins a phase at the gap; it is not ended.
    phase::end();
    const phase::Id startId = phase::begin("start");
    phase::g_testGap = switchFromGap;
    phase::endId(startId);
    g_other.join();
    phase::g_testGap = nullptr;
    CHECK(cur() == "next");
    phase::end();   // main has no loose phase left; "next" belongs to the other thread
    CHECK(cur() == "next");
    {
        std::thread t([] { phase::end(); });   // a new thread has no loose phase either
        t.join();
    }
    CHECK(cur() == "next");
    // (the other thread's entry is ended by its id, which the fixture does not hold: end it through a Scope-less
    // route by letting the process-wide state be replaced below.)
    phase::g_active.clear();
    CHECK(cur().empty());

    // 2b. a stale id does not close the next interval of the same name.
    {
        phase::Scope first("x");
        const phase::Id old = first.id();
        phase::endId(old);                // the watcher ended it...
        CHECK(cur().empty());
        phase::Scope second("x");         // ...a same-name restart gets a new id
        CHECK(second.id() != old && cur() == "x");
        phase::endId(old);                // a late end for the OLD interval
        CHECK(cur() == "x");              // must not close the new one
    }
    CHECK(cur().empty());   // both scopes have exited; the first was already gone

    // 3. only a phase of a second or more is logged.
    p1::g_lines.clear();
    phase::begin("short", 5, 1);
    phase::end();
    CHECK(p1::g_lines.empty());
    phase::begin("long", 123, 4);
    ms(1100);
    phase::end();
    CHECK(p1::g_lines.size() == 1);
    if (!p1::g_lines.empty()) {
        std::printf("log: %s\n", p1::g_lines[0].c_str());
        CHECK(p1::g_lines[0].rfind("phase: long ", 0) == 0);
        CHECK(p1::g_lines[0].find("(t=123, iter 4)") != std::string::npos);
    }

    std::printf(g_fail ? "FAILED %d\n" : "all ok\n", g_fail);
    return g_fail ? 1 : 0;
}

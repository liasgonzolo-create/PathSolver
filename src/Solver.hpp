#pragma once
#include <Geode/Geode.hpp>
#include <array>
#include <chrono>
#include <climits>
#include <random>
#include <string>
#include <vector>

namespace ps {

constexpr float TICKF = 1.f / 240.f;

enum class Phase { Idle, Search, Verify, Prune, Playback };
enum class End { Died, Completed, Converged };

struct Sample { float x = 0, y = 0; };

struct Stats {
    int attempts = 0;   // simulated tries
    int deaths = 0;
    int rounds = 0;     // times a full path reached the end
    int fixups = 0;     // times a from-the-start replay died and had to be repaired
    int pruned = 0;     // clicks removed or merged
    int clicksBefore = 0;
    int window = 0;     // current look-back window in ticks
    int cpFaults = 0;   // respawns that landed off the plan and forced a full replay
};

// Where the time goes, so the overlay can show real speed numbers.
struct Perf {
    uint64_t ticks = 0, usSim = 0;      // game ticks simulated and time spent in them
    uint64_t resets = 0, usReset = 0;   // rewinds and time spent in them
    uint64_t cps = 0, usCp = 0;         // checkpoints made and time spent making them
};

// How it works
//  The game itself is the simulator. The solver keeps one plan (hold / release
//  for every tick). It replays the plan, and when the player dies it edits the
//  plan a little way before the death (add a press, cut a press short, or
//  clear what follows), rewinds to a practice checkpoint and tries again. An
//  edit is kept only if the player gets further. When the end is reached the
//  plan is replayed from tick 0 with no rewinds (Verify), then every click is
//  test-deleted and test-merged (Prune), then verified once more.
class Solver {
public:
    static Solver& get();

    // ---- state the overlay reads ----
    Phase phase = Phase::Idle;
    std::vector<uint8_t> plan, cand;     // accepted plan / plan being tried
    std::vector<Sample> path, candPath;  // player position per tick for each
    Stats stats;
    int curTick = 0;          // tick being simulated right now
    int from = 0;             // first tick where cand differs from plan
    int bestD = -1000;        // how far the accepted plan gets
    int totalTicks = 0;       // length of a finished plan
    int stuck = 0;
    bool solved = false;      // plan beats the level from tick 0
    std::array<int, 200> deathBins{};
    std::vector<Sample> recentDeaths;
    std::string status = "idle";
    int budgetMs = 25;
    Perf perf;
    bool paused = false;        // F2
    bool renderOff = true;      // F5: hide the level while solving (much faster)
    bool showOverlay = true;    // F1
    bool showMap = true;        // F10
    int anchorTick = 0;         // tick the current try was rewound to
    std::chrono::steady_clock::time_point startTime;
    float pruneProgress() const { return plan.empty() ? 1.f : 1.f - static_cast<float>(m_pruneCursor) / plan.size(); }
    void applyRender(PlayLayer* pl);

    bool active() const { return phase != Phase::Idle; }
    bool solving() const { return phase == Phase::Search || phase == Phase::Verify || phase == Phase::Prune; }
    bool injecting() const { return m_injecting; }
    bool usedMacro() const { return m_usedMacro; }
    int clickCount(const std::vector<uint8_t>& p) const;

    // ---- control ----
    void onLevelEnter(GJGameLevel* level);
    void onQuit(PlayLayer* pl);
    void startSolve();
    void startPlayback();
    void stop(PlayLayer* pl);

    // ---- called from hooks ----
    bool preStep(PlayLayer* pl);     // true = run one game tick, then postStep
    void postStep(PlayLayer* pl);
    int playbackSteps(PlayLayer* pl, float dt);
    void playbackPre(PlayLayer* pl);
    void onDeath() { m_died = true; }
    void onComplete(PlayLayer* pl);
    void onReset(PlayLayer* pl);

    bool saveMacro();
    bool loadMacro();

private:
    void init(PlayLayer* pl);
    void beginAttempt(PlayLayer* pl);
    void endAttempt(PlayLayer* pl, End e, int D);
    void startVerify();
    void startPrune();
    void nextPruneTest();
    void makeMutation();
    void humanize(std::vector<uint8_t>& p, int fromTick);
    int sampleHold();
    void registerDeath(PlayLayer* pl, int D);
    void finish(PlayLayer* pl);
    void setHeld(PlayLayer* pl, bool want);
    void syncCheckpoints(PlayLayer* pl);
    void popAll(PlayLayer* pl);
    void bail(PlayLayer* pl);

    bool m_needInit = false, m_needRewind = false, m_needPlaybackReset = false;
    bool m_held = false, m_injecting = false, m_died = false, m_completed = false;
    bool m_pruneDone = false, m_everCompleted = false, m_usedMacro = false;
    bool m_inReset = false, m_prune = true;
    bool m_endFlagUsable = true;  // false if the game's end flag survives a reset
public:
    bool m_budgetTouched = false; // Up/Down was used; keep that speed
private:
    std::vector<int> m_cpTicks;      // tick of every checkpoint we own, oldest first
    int m_cpValidLimit = INT_MAX;    // checkpoints after this tick are off the accepted path
    int m_recordD = -1000, m_slack = 0, m_tickOffset = 0, m_lastCpTick = 0;
    int m_convergeFrom = 0, m_streak = 0;
    int m_pruneCursor = 0, m_pruneOp = 0, m_pruneRunStart = 0;
    int m_cpInterval = 24, m_minPress = 6, m_minGap = 6;
    int m_savedAttempts = -1;
    double m_accum = 0;
    std::mt19937 m_rng;
    std::string m_levelKey, m_levelName;
    int m_levelId = 0;
    geode::Ref<GJGameLevel> m_level;
};

}

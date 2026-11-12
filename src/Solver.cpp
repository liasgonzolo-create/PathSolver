#include "Solver.hpp"
#include "compat.hpp"
#include <Geode/ui/Notification.hpp>
#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>

using namespace geode::prelude;

namespace ps {

static const int MAX_TICKS = 240 * 60 * 20; // 20 minutes of level

Solver& Solver::get() {
    static Solver s;
    return s;
}

using Clock = std::chrono::steady_clock;
static uint64_t usSince(Clock::time_point t0) {
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
}

static int settingInt(const char* key) {
    return static_cast<int>(Mod::get()->getSettingValue<int64_t>(key));
}

static bool samePlan(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    size_t n = std::max(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        uint8_t x = i < a.size() ? a[i] : 0, y = i < b.size() ? b[i] : 0;
        if (x != y) return false;
    }
    return true;
}

int Solver::clickCount(const std::vector<uint8_t>& p) const {
    int n = 0;
    for (size_t i = 0; i < p.size(); ++i)
        if (p[i] && (i == 0 || !p[i - 1])) ++n;
    return n;
}

// ------------------------------------------------------------------ control

void Solver::onLevelEnter(GJGameLevel* level) {
    *this = Solver();
    m_level = level;
    m_levelId = compat::levelId(level);
    std::string name = level->m_levelName;
    for (char& c : name)
        if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
    m_levelName = name;
    m_levelKey = m_levelId > 0 ? fmt::format("{}-{}", m_levelId, name) : fmt::format("local-{}", name);
    if (loadMacro()) {
        solved = true;
        status = "macro loaded";
    }
}

void Solver::onQuit(PlayLayer* pl) {
    if (m_savedAttempts >= 0 && m_level) compat::setAttempts(m_level, m_savedAttempts);
    m_savedAttempts = -1;
    phase = Phase::Idle;
    m_level = nullptr;
}

void Solver::startSolve() {
    phase = Phase::Verify; // replaying whatever plan exists is always step one
    m_needInit = true;
    status = "starting";
}

void Solver::startPlayback() {
    if (plan.empty()) return;
    phase = Phase::Playback;
    m_needPlaybackReset = true;
    m_accum = 0;
    status = "playing macro";
}

void Solver::popAll(PlayLayer* pl) {
    int guard = 0;
    while (compat::cpCount(pl) > 0 && guard++ < 100000) compat::popCheckpoint(pl);
    m_cpTicks.clear();
}

void Solver::stop(PlayLayer* pl) {
    if (!pl) { phase = Phase::Idle; return; }
    setHeld(pl, false);
    if (solving()) {
        popAll(pl);
        if (m_savedAttempts >= 0) compat::setAttempts(pl->m_level, m_savedAttempts);
        m_savedAttempts = -1;
        status = "stopped";
    }
    phase = Phase::Idle;
    paused = false;
    applyRender(pl);
}

void Solver::applyRender(PlayLayer* pl) {
    if (pl) compat::setWorldVisible(pl, !(solving() && renderOff));
}

void Solver::setHeld(PlayLayer* pl, bool want) {
    if (want == m_held) return;
    m_injecting = true;
    compat::press(pl, want);
    m_injecting = false;
    m_held = want;
    if (want && phase == Phase::Playback) m_usedMacro = true;
}

void Solver::onReset(PlayLayer* pl) {
    if (m_inReset) return;
    if (m_held) { // make sure the game agrees the button is up
        m_injecting = true;
        compat::press(pl, false);
        m_injecting = false;
        m_held = false;
    }
    m_usedMacro = false;
    if (phase == Phase::Playback && compat::cpCount(pl) == 0)
        m_tickOffset = -compat::rawTick(pl);
}

// ------------------------------------------------------------------ solving

void Solver::init(PlayLayer* pl) {
    m_needInit = false;
    if (compat::isPlatformer(pl)) {
        phase = Phase::Idle;
        status = "platformer levels are not supported";
        Notification::create("Path Solver: platformer levels are not supported", NotificationIcon::Error)->show();
        return;
    }
    m_cpInterval = settingInt("checkpoint-interval");
    m_minPress = settingInt("min-press-ticks");
    m_minGap = settingInt("min-gap-ticks");
    if (!m_budgetTouched) budgetMs = settingInt("sim-budget-ms");
    m_prune = Mod::get()->getSettingValue<bool>("prune-clicks");
    m_rng.seed(static_cast<unsigned>(settingInt("seed")));

    m_savedAttempts = compat::getAttempts(pl->m_level);
    if (!compat::isPractice(pl)) pl->togglePracticeMode(true);
    popAll(pl);

    stats = Stats();
    deathBins.fill(0);
    recentDeaths.clear();
    solved = false;
    m_pruneDone = false;
    m_everCompleted = false;
    stuck = 0;
    perf = Perf();
    paused = false;
    startTime = Clock::now();
    startVerify();
    applyRender(pl);
}

void Solver::startVerify() {
    phase = Phase::Verify;
    cand = plan;
    candPath = path;
    from = 0;
    m_needRewind = true;
    status = "verifying from the start";
}

void Solver::syncCheckpoints(PlayLayer* pl) {
    // Drop checkpoints the game added on its own (auto-checkpoints).
    int guard = 0;
    while (compat::cpCount(pl) > m_cpTicks.size() && guard++ < 1000) compat::popCheckpoint(pl);
    if (compat::cpCount(pl) < m_cpTicks.size()) m_cpTicks.resize(compat::cpCount(pl));
}

void Solver::beginAttempt(PlayLayer* pl) {
    m_needRewind = false;
    setHeld(pl, false);
    syncCheckpoints(pl);
    int limit = std::min(from, m_cpValidLimit);
    while (!m_cpTicks.empty() && m_cpTicks.back() > limit) {
        compat::popCheckpoint(pl);
        m_cpTicks.pop_back();
    }
    m_cpValidLimit = INT_MAX;

    auto t0 = Clock::now();
    m_inReset = true;
    pl->resetLevel(); // practice mode: respawns at the newest remaining checkpoint
    m_inReset = false;
    m_held = false;
    perf.usReset += usSince(t0);
    ++perf.resets;

    int expect = m_cpTicks.empty() ? 0 : m_cpTicks.back();
    // Trust nothing: the respawn point must be where the plan was at that
    // tick. If it is not, the checkpoints are wrong; drop them all and replay
    // from the start.
    if (!m_cpTicks.empty() && expect < static_cast<int>(candPath.size())) {
        auto pos = pl->m_player1->getPosition();
        if (std::fabs(pos.x - candPath[expect].x) > 0.5f || std::fabs(pos.y - candPath[expect].y) > 0.5f) {
            ++stats.cpFaults;
            popAll(pl);
            m_inReset = true;
            pl->resetLevel();
            m_inReset = false;
            expect = 0;
        }
    }
    m_tickOffset = expect - compat::rawTick(pl);
    anchorTick = expect;
    m_lastCpTick = expect;
    m_streak = 0;
    m_died = m_completed = false;
    ++stats.attempts;
}

bool Solver::preStep(PlayLayer* pl) {
    if (m_needInit) { init(pl); return false; }
    if (m_needRewind) { beginAttempt(pl); return false; }
    syncCheckpoints(pl);

    int t = std::max(0, compat::rawTick(pl) + m_tickOffset);
    curTick = t;
    if (t > MAX_TICKS) {
        status = "gave up: level never ends";
        stop(pl);
        return false;
    }

    auto pos = pl->m_player1->getPosition();
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y)) { bail(pl); return false; }
    if (static_cast<int>(candPath.size()) <= t) candPath.resize(t + 1, Sample{pos.x, pos.y});
    candPath[t] = Sample{pos.x, pos.y};

    // Prune tests end early once the player is back on the known-good path.
    if (phase == Phase::Prune && t >= m_convergeFrom && t < static_cast<int>(path.size())) {
        bool same = std::fabs(pos.x - path[t].x) < 0.01f && std::fabs(pos.y - path[t].y) < 0.01f;
        m_streak = same ? m_streak + 1 : 0;
        if (m_streak >= 8) {
            endAttempt(pl, End::Converged, t);
            return false;
        }
    }

    // Save states are only taken with the button up, so respawning (which
    // releases the button) reproduces the exact same state.
    if (!m_held && t > 0 && t - m_lastCpTick >= m_cpInterval) {
        size_t before = compat::cpCount(pl);
        auto t0 = Clock::now();
        compat::makeCheckpoint(pl);
        perf.usCp += usSince(t0);
        ++perf.cps;
        size_t after = compat::cpCount(pl);
        if (after == before + 1) {
            m_cpTicks.push_back(t);
        } else if (after == before && !m_cpTicks.empty()) {
            // The game caps how many checkpoints exist: it stored the new one
            // and threw away the oldest. Mirror that so the lists stay aligned.
            m_cpTicks.erase(m_cpTicks.begin());
            m_cpTicks.push_back(t);
        }
        m_lastCpTick = t;
    }

    setHeld(pl, t < static_cast<int>(cand.size()) && cand[t]);
    m_died = m_completed = false;
    return true;
}

void Solver::postStep(PlayLayer* pl) {
    if (m_completed) endAttempt(pl, End::Completed, curTick + 1);
    else if (m_died) endAttempt(pl, End::Died, curTick);
}

void Solver::registerDeath(PlayLayer* pl, int D) {
    ++stats.deaths;
    auto pos = pl->m_player1->getPosition();
    float len = std::max(1.f, compat::levelLength(pl));
    int bin = std::clamp(static_cast<int>(pos.x / len * deathBins.size()), 0, static_cast<int>(deathBins.size()) - 1);
    ++deathBins[bin];
    recentDeaths.push_back(Sample{pos.x, pos.y});
    if (recentDeaths.size() > 48) recentDeaths.erase(recentDeaths.begin());
}

void Solver::endAttempt(PlayLayer* pl, End e, int D) {
    switch (phase) {
    case Phase::Verify:
        plan = cand;
        path = candPath;
        m_cpValidLimit = INT_MAX;
        if (e == End::Completed) {
            totalTicks = D;
            plan.resize(D, 0);
            bestD = D;
            if (m_prune && !m_pruneDone) startPrune();
            else finish(pl);
        } else {
            registerDeath(pl, D);
            if (m_everCompleted) ++stats.fixups;
            bestD = m_recordD = D;
            stuck = 0;
            m_slack = 0;
            phase = Phase::Search;
            status = "searching";
            makeMutation();
        }
        break;

    case Phase::Search:
        if (e == End::Completed) {
            plan = cand;
            path = candPath;
            totalTicks = D;
            plan.resize(D, 0);
            bestD = D;
            m_everCompleted = true;
            ++stats.rounds;
            m_cpValidLimit = INT_MAX;
            startVerify();
        } else {
            registerDeath(pl, D);
            if (D >= bestD + 2 - m_slack) {
                plan = cand;
                path = candPath;
                bestD = D;
                m_cpValidLimit = INT_MAX;
                if (D > m_recordD) { m_recordD = D; stuck = 0; m_slack = 0; }
                else ++stuck;
            } else {
                ++stuck;
                m_cpValidLimit = from; // later checkpoints belong to the rejected try
            }
            // Stuck for a long time: allow plans that die a bit earlier, to
            // get out of dead-end routes.
            if (stuck > 0 && stuck % 150 == 0) m_slack = std::min(m_slack + 48, 1440);
            makeMutation();
        }
        break;

    case Phase::Prune:
        if (e == End::Died) {
            registerDeath(pl, D);
            m_cpValidLimit = from;
            if (++m_pruneOp > 1) { m_pruneCursor = m_pruneRunStart; m_pruneOp = 0; }
        } else {
            plan = cand;
            if (e == End::Completed) {
                path = candPath;
                totalTicks = D;
                plan.resize(D, 0);
                bestD = D;
            } else {
                int hi = std::min<int>(D, std::min(path.size(), candPath.size()) - 1);
                for (int i = from; i <= hi; ++i) path[i] = candPath[i];
            }
            m_cpValidLimit = INT_MAX;
            ++stats.pruned;
            m_pruneCursor = m_pruneRunStart;
            m_pruneOp = 0;
        }
        nextPruneTest();
        break;

    default: break;
    }
}

// Lengths a person would actually press for: mostly taps, some short holds,
// a few long ones.
int Solver::sampleHold() {
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    double r = uni(m_rng);
    if (r < 0.40) return m_minPress + static_cast<int>(uni(m_rng) * 4);
    if (r < 0.75) return m_minPress + static_cast<int>(uni(m_rng) * 26);
    return 20 + static_cast<int>(uni(m_rng) * 90);
}

// From `fromTick` onward: no hold shorter than min-press, no gap between two
// holds shorter than min-gap. Never changes anything before `fromTick`.
void Solver::humanize(std::vector<uint8_t>& p, int fromTick) {
    int n = static_cast<int>(p.size());
    int i = std::clamp(fromTick, 0, n);
    while (i > 0 && i < n && p[i - 1] == p[i]) --i;
    int guard = 0;
    while (i < n && guard++ < 400000) {
        int j = i;
        while (j < n && p[j] == p[i]) ++j;
        if (p[i]) {
            if (j - i < m_minPress && j < n) {
                int e = std::min(n, i + m_minPress);
                std::fill(p.begin() + j, p.begin() + e, 1);
                continue;
            }
        } else if (i >= fromTick && i > 0 && j < n && j - i < m_minGap) {
            std::fill(p.begin() + i, p.begin() + j, 1);
            while (i > 0 && p[i - 1]) --i;
            continue;
        }
        i = j;
    }
}

void Solver::makeMutation() {
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    int D = std::max(bestD, 1);
    // Look-back window: usually tiny, periodically larger, and larger still
    // the longer we are stuck (20 ticks up to ~10 s).
    int lvl = std::min<int>(std::countr_zero(static_cast<unsigned>(stuck + 1)) + stuck / 96, 7);
    int W = 20 << lvl;
    stats.window = W;

    int M = 0;
    for (int tries = 0; tries < 12; ++tries) {
        cand = plan;
        size_t need = static_cast<size_t>(D) + 400;
        if (cand.size() < need) cand.resize(need, 0);
        int n = static_cast<int>(cand.size());

        M = std::clamp(D - 1 - static_cast<int>(W * std::pow(uni(m_rng), 1.6)), 0, n - 1);
        bool holdAfter = std::find(cand.begin() + M, cand.end(), 1) != cand.end();
        double r = uni(m_rng);
        int type = (!holdAfter || r < 0.55) ? 0 : (r < 0.85 ? 1 : 2);
        int h = sampleHold();

        if (type == 0) { // press at M for h ticks
            if (M > 0 && !cand[M - 1]) {
                int e = M;
                while (e > 0 && !cand[e - 1]) --e;
                if (e > 0 && M - e < m_minGap) M = e; // too close to the last click: extend it
            }
            if (M + h + 64 > n) { cand.resize(M + h + 64, 0); n = static_cast<int>(cand.size()); }
            std::fill(cand.begin() + M, cand.begin() + M + h, 1);
        } else {
            if (cand[M]) {
                int s = M;
                while (s > 0 && cand[s - 1]) --s;
                if (M - s < m_minPress) M = s; // would leave a too-short click: drop it whole
            }
            int end = type == 1 ? std::min(n, M + std::max(h, m_minGap)) : n;
            std::fill(cand.begin() + M, cand.begin() + end, 0);
        }
        humanize(cand, M);
        if (!samePlan(cand, plan)) break;
    }
    from = M;
    candPath = path;
    m_needRewind = true;
}

// ------------------------------------------------------------------ pruning

void Solver::startPrune() {
    phase = Phase::Prune;
    stats.clicksBefore = clickCount(plan);
    m_pruneCursor = static_cast<int>(plan.size());
    m_pruneOp = 0;
    status = "removing extra clicks";
    nextPruneTest();
}

// Walks the clicks from the last to the first. For each: try deleting it,
// and if that dies, try merging it with the next click.
void Solver::nextPruneTest() {
    int n = static_cast<int>(plan.size());
    for (;;) {
        int i = std::min(m_pruneCursor, n) - 1;
        while (i >= 0 && !plan[i]) --i;
        if (i < 0) {
            m_pruneDone = true;
            startVerify();
            return;
        }
        int e = i + 1, s = i;
        while (s > 0 && plan[s - 1]) --s;
        m_pruneRunStart = s;

        if (m_pruneOp == 0) {
            cand = plan;
            std::fill(cand.begin() + s, cand.begin() + e, 0);
            from = s;
            m_convergeFrom = e + 12;
            break;
        }
        if (m_pruneOp == 1) {
            int ns = e;
            while (ns < n && !plan[ns]) ++ns;
            if (ns < n && ns - e <= 48) {
                int ne = ns;
                while (ne < n && plan[ne]) ++ne;
                cand = plan;
                std::fill(cand.begin() + e, cand.begin() + ns, 1);
                from = e;
                m_convergeFrom = ne + 12;
                break;
            }
        }
        m_pruneCursor = s;
        m_pruneOp = 0;
    }
    candPath = path;
    m_streak = 0;
    m_needRewind = true;
}

void Solver::finish(PlayLayer* pl) {
    setHeld(pl, false);
    popAll(pl);
    if (m_savedAttempts >= 0) compat::setAttempts(pl->m_level, m_savedAttempts);
    m_savedAttempts = -1;
    solved = true;
    phase = Phase::Idle;
    applyRender(pl);
    bool saved = saveMacro();
    status = fmt::format("solved: {} clicks", clickCount(plan));
    Notification::create(
        fmt::format("Solved with {} clicks{}", clickCount(plan), saved ? " (macro saved)" : ""),
        NotificationIcon::Success
    )->show();
    startPlayback(); // show the result straight away
}

// ------------------------------------------------------------------ playback

int Solver::playbackSteps(PlayLayer* pl, float dt) {
    if (m_needPlaybackReset) {
        m_needPlaybackReset = false;
        pl->resetLevel();
        m_accum = 0;
        return 0;
    }
    m_accum += dt;
    int n = static_cast<int>(m_accum / TICKF);
    if (n > 24) { n = 24; m_accum = 0; } // don't spiral after a lag spike
    else m_accum -= n * TICKF;
    return n;
}

// The player's position became NaN/infinite. The game crashes on its next
// tick if that is left alone. This happens when another mod that changes
// physics stepping (Click Between Frames, TPS/physics bypass) is active.
void Solver::bail(PlayLayer* pl) {
    stop(pl);
    status = "stopped: another physics mod is interfering";
    Notification::create("Path Solver stopped: turn off Click Between Frames / physics bypass mods", NotificationIcon::Error)->show();
    pl->resetLevel();
}

void Solver::playbackPre(PlayLayer* pl) {
    auto pos = pl->m_player1->getPosition();
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y)) { bail(pl); return; }
    int t = compat::rawTick(pl) + m_tickOffset;
    curTick = std::max(0, t);
    setHeld(pl, t >= 0 && t < static_cast<int>(plan.size()) && plan[t]);
}

// ------------------------------------------------------------------ files

// Path types differ between Geode 5 (std::filesystem) and Geode 6 (asp::fs),
// so everything here goes through `auto` and Geode's own file helpers.

bool Solver::saveMacro() {
    if (plan.empty() || m_levelKey.empty()) return false;
    int off = settingInt("export-frame-offset");
    std::string inputs;
    bool prev = false;
    auto add = [&](int frame, bool down) {
        if (!inputs.empty()) inputs += ',';
        inputs += fmt::format("{{\"frame\":{},\"btn\":1,\"2p\":false,\"down\":{}}}", std::max(0, frame + off), down ? "true" : "false");
    };
    for (size_t t = 0; t < plan.size(); ++t) {
        bool h = plan[t];
        if (h != prev) add(static_cast<int>(t), h);
        prev = h;
    }
    if (prev) add(static_cast<int>(plan.size()), false);

    std::string out = fmt::format(
        "{{\"author\":\"PathSolver\",\"bot\":{{\"name\":\"PathSolver\",\"version\":\"1.0.0\"}},"
        "\"description\":\"\",\"duration\":{:.4f},\"framerate\":240.0,\"gameVersion\":2.2081,"
        "\"level\":{{\"id\":{},\"name\":\"{}\"}},\"seed\":0,\"coins\":0,\"ldm\":false,\"version\":1.0,"
        "\"inputs\":[{}]}}",
        plan.size() / 240.0, m_levelId, m_levelName, inputs
    );
    auto dir = Mod::get()->getSaveDir() / "macros";
    (void)utils::file::createDirectoryAll(dir);
    auto file = dir / (m_levelKey + ".gdr.json");
    return utils::file::writeString(file, out).isOk();
}

bool Solver::loadMacro() {
    if (m_levelKey.empty()) return false;
    auto file = Mod::get()->getSaveDir() / "macros" / (m_levelKey + ".gdr.json");
    auto text = utils::file::readString(file);
    if (!text.isOk()) return false;
    auto parsed = matjson::parse(text.unwrap());
    if (!parsed.isOk()) return false;
    auto root = parsed.unwrap();
    int off = settingInt("export-frame-offset");

    std::vector<std::pair<int, bool>> events;
    for (auto& in : root["inputs"]) {
        if (in["btn"].asInt().unwrapOr(1) != 1) continue;
        if (in["2p"].asBool().unwrapOr(false)) continue;
        int frame = static_cast<int>(in["frame"].asInt().unwrapOr(0)) - off;
        events.emplace_back(std::max(0, frame), in["down"].asBool().unwrapOr(false));
    }
    if (events.empty()) return false;
    std::sort(events.begin(), events.end(), [](auto& a, auto& b) { return a.first < b.first; });
    int last = std::min(events.back().first, MAX_TICKS);
    plan.assign(last + 1, 0);
    bool held = false;
    size_t k = 0;
    for (int t = 0; t <= last; ++t) {
        while (k < events.size() && events[k].first <= t) held = events[k++].second;
        plan[t] = held;
    }
    totalTicks = last + 1;
    return true;
}

}

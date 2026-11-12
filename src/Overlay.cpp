#include "Solver.hpp"
#include "Overlay.hpp"
#include "compat.hpp"
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace ps {

static const float BUCKET = 300.f;

Overlay* Overlay::create() {
    auto ret = new Overlay();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool Overlay::init() {
    if (!CCNode::init()) return false;
    m_draw = CCDrawNode::create();
    m_draw->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});
    this->addChild(m_draw);

    auto mk = [this](const char* font, float scale) {
        auto l = CCLabelBMFont::create("", font);
        l->setAnchorPoint({0.f, 1.f});
        l->setScale(scale);
        this->addChild(l, 1);
        return l;
    };
    m_title = mk("bigFont.fnt", 0.36f);
    m_head = mk("chatFont.fnt", 0.62f);
    m_cap1 = mk("chatFont.fnt", 0.5f);
    m_cap2 = mk("chatFont.fnt", 0.5f);
    m_body = mk("chatFont.fnt", 0.5f);
    m_mapCap = mk("chatFont.fnt", 0.45f);
    this->setVisible(false);
    this->scheduleUpdate();
    return true;
}

void Overlay::buildIndex(PlayLayer* pl) {
    m_built = true;
    m_buckets.clear();
    if (!pl->m_objects) return;
    for (auto obj : CCArrayExt<GameObject*>(pl->m_objects)) {
        if (!obj) continue;
        int kind = compat::objectKind(obj);
        if (!kind) continue;
        int b = std::max(0, static_cast<int>(obj->getPositionX() / BUCKET));
        if (b > 20000) continue;
        if (static_cast<int>(m_buckets.size()) <= b) m_buckets.resize(b + 1);
        m_buckets[b].push_back({obj, kind});
    }
}

CCPoint Overlay::toScreen(float wx, float wy) const {
    return {m_view.origin.x + (wx - m_camX) * m_scale, m_view.origin.y + (wy - m_camY) * m_scale};
}

void Overlay::rawRect(CCRect r, const ccColor4F& c) {
    CCPoint v[4] = {{r.getMinX(), r.getMinY()}, {r.getMaxX(), r.getMinY()}, {r.getMaxX(), r.getMaxY()}, {r.getMinX(), r.getMaxY()}};
    m_draw->drawPolygon(v, 4, c, 0.f, c);
}

// Clipped to the map view.
void Overlay::fillRect(CCRect r, const ccColor4F& c) {
    float x0 = std::max(r.getMinX(), m_view.getMinX()), x1 = std::min(r.getMaxX(), m_view.getMaxX());
    float y0 = std::max(r.getMinY(), m_view.getMinY()), y1 = std::min(r.getMaxY(), m_view.getMaxY());
    if (x1 <= x0 || y1 <= y0) return;
    rawRect(CCRect(x0, y0, x1 - x0, y1 - y0), c);
}

// White outlined bar like a loading bar; returns the y just below it.
float Overlay::drawBar(float x, float top, float w, float frac, const ccColor4F& fill) {
    const float h = 8.f;
    ccColor4F white = ccc4f(1.f, 1.f, 1.f, 1.f);
    rawRect(CCRect(x, top - h, w, h), white);
    rawRect(CCRect(x + 1.2f, top - h + 1.2f, w - 2.4f, h - 2.4f), ccc4f(0.03f, 0.03f, 0.04f, 1.f));
    frac = std::clamp(frac, 0.f, 1.f);
    if (frac > 0.f) rawRect(CCRect(x + 2.f, top - h + 2.f, (w - 4.f) * frac, h - 4.f), fill);
    return top - h;
}

void Overlay::line(CCPoint a, CCPoint b, float w, const ccColor4F& c) {
    if (!m_view.containsPoint(a) || !m_view.containsPoint(b)) return;
    m_draw->drawSegment(a, b, w, c);
}

void Overlay::drawPath(const std::vector<Sample>& pts, const std::vector<uint8_t>& holds, int lo, int hi,
                       const ccColor4F& rel, const ccColor4F& hold, float w, bool dots) {
    hi = std::min(hi, static_cast<int>(pts.size()) - 1);
    lo = std::max(lo, 0);
    const int step = 2;
    for (int t = lo; t + step <= hi; t += step) {
        const auto& a = pts[t];
        const auto& b = pts[t + step];
        if (std::fabs(b.x - a.x) > 90.f || std::fabs(b.y - a.y) > 90.f) continue; // teleport
        bool h = t < static_cast<int>(holds.size()) && holds[t];
        line(toScreen(a.x, a.y), toScreen(b.x, b.y), h ? w * 1.7f : w, h ? hold : rel);
    }
    if (!dots) return;
    for (int t = std::max(lo, 1); t <= hi && t < static_cast<int>(holds.size()); ++t) {
        if (holds[t] && !holds[t - 1]) {
            auto p = toScreen(pts[t].x, pts[t].y);
            if (m_view.containsPoint(p)) m_draw->drawDot(p, 2.4f, ccc4f(1.f, 0.85f, 0.2f, 1.f));
        }
    }
}

void Overlay::update(float dt) {
    auto& s = Solver::get();
    auto pl = PlayLayer::get();
    bool show = pl && s.active() && s.showOverlay;
    if (show && s.phase == Phase::Playback) show = Mod::get()->getSettingValue<bool>("playback-map");
    if (show && s.solving()) show = Mod::get()->getSettingValue<bool>("show-map") || s.renderOff;
    this->setVisible(show);
    if (!show) return;

    m_rateTimer += dt;
    if (m_rateTimer >= 0.5f) {
        const Perf& p = s.perf;
        uint64_t dTicks = p.ticks - m_lastPerf.ticks, dRes = p.resets - m_lastPerf.resets, dCps = p.cps - m_lastPerf.cps;
        m_tps = dTicks / m_rateTimer;
        if (dRes) m_resetMs = (p.usReset - m_lastPerf.usReset) / 1000.f / dRes;
        if (dCps) m_cpMs = (p.usCp - m_lastPerf.usCp) / 1000.f / dCps;
        m_tries = (s.stats.attempts - m_lastAttempts) / m_rateTimer;
        m_lastPerf = p;
        m_lastAttempts = s.stats.attempts;
        m_rateTimer = 0;
    }
    if (++m_frame % 2) return; // redraw every other frame
    if (!m_built) buildIndex(pl);
    redraw(pl);
}

void Overlay::redraw(PlayLayer* pl) {
    auto& s = Solver::get();
    auto win = CCDirector::get()->getWinSize();
    m_draw->clear();

    bool playback = s.phase == Phase::Playback;
    // Backdrop: solid while the level is hidden, dimmed otherwise.
    if (!playback) rawRect(CCRect(0, 0, win.width, win.height), ccc4f(0.02f, 0.02f, 0.03f, s.renderOff ? 1.f : 0.72f));

    auto pp = pl->m_player1->getPosition();
    float len = std::max(1.f, compat::levelLength(pl));
    auto xAt = [&](const std::vector<Sample>& v, int tick) {
        if (v.empty()) return 0.f;
        return v[std::clamp(tick, 0, static_cast<int>(v.size()) - 1)].x;
    };
    float bestX = xAt(s.path, s.phase == Phase::Search ? s.bestD : static_cast<int>(s.path.size()) - 1);
    float levelFrac = s.solved ? 1.f : std::clamp(bestX / len, 0.f, 1.f);
    float nowFrac = std::clamp(pp.x / len, 0.f, 1.f);

    // ---------------- left column ----------------
    const float x = 10.f, colW = win.width * 0.44f - 16.f;
    float y = win.height - 8.f;
    auto place = [&](CCLabelBMFont* l, const std::string& text, float gap) {
        l->setVisible(true);
        l->setString(text.c_str());
        l->setPosition({x, y});
        y -= l->getContentSize().height * l->getScale() + gap;
    };
    const char* phase =
        s.paused ? "PAUSED" :
        s.phase == Phase::Search ? "SOLVING" :
        s.phase == Phase::Verify ? "VERIFYING" :
        s.phase == Phase::Prune ? "REMOVING CLICKS" : "PLAYING MACRO";
    place(m_title, fmt::format("PATH SOLVER V1.2 {} - {}", s.mode == Mode::Absolute ? "ABSOLUTE" : "ACCURATE", phase), 3.f);

    if (playback) {
        place(m_head, fmt::format("t={} / {}   {:.2f}s of {:.2f}s   clicks {}",
            s.curTick, s.plan.size(), s.curTick / 240.f, s.plan.size() / 240.f, s.clickCount(s.plan)), 3.f);
        m_cap1->setVisible(false);
        m_cap2->setVisible(false);
        m_body->setVisible(false);
    } else {
        int secs = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - s.startTime).count());
        const char* what =
            s.phase == Phase::Search ? "finding a path: trying edits just before each death" :
            s.phase == Phase::Verify ? "replaying the whole plan from the start" :
            "testing the level without each click";
        place(m_head, fmt::format(
            "try {}   {}   {}s\nlevel   {:.1f}%   best x {:.0f} / {:.0f}\nnow     x {:.0f}  ({:.1f}%)   anchor t={} x={:.0f}",
            s.stats.attempts, what, secs, levelFrac * 100.f, bestX, len,
            pp.x, nowFrac * 100.f, s.anchorTick, xAt(s.candPath, s.anchorTick)), 6.f);

        place(m_cap1, fmt::format("level {:.1f}%   full runs {}", levelFrac * 100.f, s.stats.rounds), 1.f);
        y = drawBar(x, y, colW, levelFrac, ccc4f(0.40f, 0.90f, 0.35f, 1.f)) - 5.f;

        float stage = 0.f;
        std::string cap;
        if (s.phase == Phase::Verify) { stage = nowFrac; cap = fmt::format("verifying from the start {:.1f}%", stage * 100.f); }
        else if (s.phase == Phase::Prune) { stage = s.pruneProgress(); cap = fmt::format("removing extra clicks {:.1f}%   removed {} of {}", stage * 100.f, s.stats.pruned, s.stats.clicksBefore); }
        else {
            int span = std::max(1, s.bestD - s.anchorTick);
            stage = static_cast<float>(s.curTick - s.anchorTick) / span;
            cap = fmt::format("option {}/{} at this death   {} ticks back   depth {}   backtracks {}",
                s.stuck, s.optionCount(), s.stats.window, s.depth(), s.stats.backtracks);
        }
        place(m_cap2, cap, 1.f);
        y = drawBar(x, y, colW, stage, ccc4f(0.35f, 0.65f, 1.f, 1.f)) - 5.f;

        place(m_body, fmt::format(
            "SPEED  {:.0f} ticks/s  ({} ms/frame)   {:.0f} tries/s   RENDER {}\n"
            "rewind {:.1f} ms   checkpoint {:.2f} ms\n"
            "deaths {}   clicks {}   repairs {}   bad respawns {}\n"
            "F1   hide this overlay\n"
            "F2   pause / resume\n"
            "F5   render on / off\n"
            "F10  map: {}\n"
            "Up / Down   speed up / down",
            m_tps, s.budgetMs, m_tries, s.renderOff ? "off" : "on",
            m_resetMs, m_cpMs,
            s.stats.deaths, s.clickCount(s.plan), s.stats.fixups, s.stats.cpFaults,
            s.showMap ? "on" : "off"), 0.f);
    }

    // ---------------- map ----------------
    if (s.showMap) {
        CCRect panel = playback
            ? CCRect(win.width * 0.5f, win.height * 0.55f, win.width * 0.5f - 8.f, win.height * 0.45f - 8.f)
            : CCRect(win.width * 0.44f, 8.f, win.width * 0.56f - 8.f, win.height - 16.f);
        drawMap(pl, panel);
    } else {
        m_mapCap->setVisible(false);
    }
}

void Overlay::drawMap(PlayLayer* pl, CCRect panel) {
    auto& s = Solver::get();
    bool playback = s.phase == Phase::Playback;
    auto pp = pl->m_player1->getPosition();
    float len = std::max(1.f, compat::levelLength(pl));

    const float capH = 12.f, barH = 16.f;
    rawRect(panel, ccc4f(0.75f, 0.75f, 0.70f, 1.f)); // thin frame
    rawRect(CCRect(panel.origin.x + 1.f, panel.origin.y + 1.f, panel.size.width - 2.f, panel.size.height - 2.f), ccc4f(0.07f, 0.08f, 0.10f, 1.f));
    m_view = CCRect(panel.origin.x + 3.f, panel.origin.y + barH + 3.f, panel.size.width - 6.f, panel.size.height - capH - barH - 6.f);
    m_scale = m_view.size.width / (30.f * 22.f); // 22 blocks across

    // camera follows the simulated player
    m_camX = pp.x - m_view.size.width * 0.35f / m_scale;
    float wantY = std::max(pp.y - m_view.size.height * 0.45f / m_scale, -40.f);
    m_camY += (wantY - m_camY) * 0.25f;
    float worldW = m_view.size.width / m_scale;

    // level geometry
    fillRect(CCRect(m_view.getMinX(), toScreen(0, -6).y, m_view.size.width, 6 * m_scale), ccc4f(0.35f, 0.40f, 0.48f, 1.f));
    int b0 = std::max(0, static_cast<int>((m_camX - 120.f) / BUCKET));
    int b1 = std::min(static_cast<int>(m_buckets.size()) - 1, static_cast<int>((m_camX + worldW + 120.f) / BUCKET));
    for (int b = b0; b <= b1; ++b) {
        for (auto& g : m_buckets[b]) {
            CCRect wr = compat::objectRect(g.obj);
            auto o = toScreen(wr.origin.x, wr.origin.y);
            CCRect r(o.x, o.y, std::max(1.5f, wr.size.width * m_scale), std::max(1.5f, wr.size.height * m_scale));
            switch (g.kind) {
                case 1: fillRect(r, ccc4f(0.45f, 0.85f, 0.25f, 0.95f)); break;   // solid
                case 6: fillRect(r, ccc4f(0.60f, 0.85f, 0.40f, 0.55f)); break;   // slope
                case 2: fillRect(r, ccc4f(1.00f, 0.35f, 0.10f, 1.00f)); break;   // hazard
                case 3: fillRect(r, ccc4f(1.00f, 0.85f, 0.20f, 0.55f)); break;   // orb
                case 4: fillRect(r, ccc4f(0.30f, 0.90f, 0.95f, 0.90f)); break;   // pad
                case 5: fillRect(r, ccc4f(0.75f, 0.45f, 1.00f, 0.40f)); break;   // portal
            }
        }
    }

    // paths
    int t = s.curTick;
    int lo = t - 900, hi = t + 2400;
    ccColor4F blue = ccc4f(0.35f, 0.75f, 1.f, 0.9f), orange = ccc4f(1.f, 0.75f, 0.2f, 1.f);
    if (playback) {
        drawPath(s.path, s.plan, lo, hi, blue, orange, 0.7f, true);
    } else {
        int end = s.phase == Phase::Search ? s.bestD : static_cast<int>(s.path.size()) - 1;
        drawPath(s.path, s.plan, lo, std::min(hi, end), blue, orange, 0.7f, true);
        drawPath(s.candPath, s.cand, std::max(lo, s.from), std::min(hi, t), ccc4f(1.f, 1.f, 1.f, 0.9f), ccc4f(1.f, 1.f, 1.f, 1.f), 0.5f, false);
    }
    for (auto& d : s.recentDeaths) {
        auto p = toScreen(d.x, d.y);
        ccColor4F red = ccc4f(1.f, 0.2f, 0.25f, 0.9f);
        line({p.x - 2.5f, p.y - 2.5f}, {p.x + 2.5f, p.y + 2.5f}, 0.5f, red);
        line({p.x - 2.5f, p.y + 2.5f}, {p.x + 2.5f, p.y - 2.5f}, 0.5f, red);
    }
    {
        auto c = toScreen(pp.x, pp.y);
        float h = 15.f * m_scale;
        ccColor4F w = ccc4f(1.f, 1.f, 1.f, 1.f);
        line({c.x - h, c.y - h}, {c.x + h, c.y - h}, 0.5f, w);
        line({c.x + h, c.y - h}, {c.x + h, c.y + h}, 0.5f, w);
        line({c.x + h, c.y + h}, {c.x - h, c.y + h}, 0.5f, w);
        line({c.x - h, c.y + h}, {c.x - h, c.y - h}, 0.5f, w);
    }

    // whole-level bar: green = verified, blue = planned, red ticks = where it dies
    auto xAt = [&](const std::vector<Sample>& v, int tick) {
        if (v.empty()) return 0.f;
        return v[std::clamp(tick, 0, static_cast<int>(v.size()) - 1)].x;
    };
    float planned = std::clamp(xAt(s.path, s.phase == Phase::Search ? s.bestD : static_cast<int>(s.path.size()) - 1) / len, 0.f, 1.f);
    float verified = 0.f;
    if (s.solved) verified = 1.f;
    else if (s.phase == Phase::Verify) verified = std::clamp(pp.x / len, 0.f, 1.f);
    else if (s.phase == Phase::Prune) verified = planned;

    CCRect bar(panel.origin.x + 5.f, panel.origin.y + 4.f, panel.size.width - 10.f, 5.f);
    rawRect(bar, ccc4f(0.22f, 0.24f, 0.28f, 1.f));
    rawRect(CCRect(bar.origin.x, bar.origin.y, bar.size.width * planned, bar.size.height), ccc4f(0.30f, 0.70f, 1.f, 1.f));
    rawRect(CCRect(bar.origin.x, bar.origin.y, bar.size.width * verified, bar.size.height), ccc4f(0.40f, 0.90f, 0.35f, 1.f));
    int bins = static_cast<int>(s.deathBins.size());
    for (int i = 0; i < bins; ++i) {
        int n = s.deathBins[i];
        if (!n) continue;
        float h = std::min(6.f, 1.f + std::log2(1.f + n));
        rawRect(CCRect(bar.origin.x + bar.size.width * i / bins, bar.getMaxY() + 1.f, std::max(1.f, bar.size.width / bins), h),
                ccc4f(1.f, 0.25f, 0.25f, 0.85f));
    }
    float cx = bar.origin.x + bar.size.width * std::clamp(pp.x / len, 0.f, 1.f);
    rawRect(CCRect(cx - 0.75f, bar.origin.y - 2.f, 1.5f, bar.size.height + 4.f), ccc4f(1.f, 1.f, 1.f, 1.f));

    m_mapCap->setVisible(true);
    m_mapCap->setPosition({panel.origin.x + 5.f, panel.getMaxY() - 2.f});
    m_mapCap->setString(fmt::format(
        "{}  t {}/{}  x {:.0f}   blue = release  orange = hold  white = this try  x = deaths",
        playback ? "macro" : (s.phase == Phase::Search ? "search" : "plan"),
        t, s.phase == Phase::Search ? std::max(0, s.bestD) : static_cast<int>(s.plan.size()), pp.x).c_str());
}

}

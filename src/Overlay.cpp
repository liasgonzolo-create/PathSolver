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

    auto mk = [this](float scale) {
        auto l = CCLabelBMFont::create("", "chatFont.fnt");
        l->setAnchorPoint({0.f, 1.f});
        l->setScale(scale);
        this->addChild(l, 1);
        return l;
    };
    m_line1 = mk(0.55f);
    m_line2 = mk(0.45f);
    m_line3 = mk(0.45f);
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

void Overlay::fillRect(CCRect r, const ccColor4F& c) {
    float x0 = std::max(r.getMinX(), m_view.getMinX()), x1 = std::min(r.getMaxX(), m_view.getMaxX());
    float y0 = std::max(r.getMinY(), m_view.getMinY()), y1 = std::min(r.getMaxY(), m_view.getMaxY());
    if (x1 <= x0 || y1 <= y0) return;
    CCPoint v[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    m_draw->drawPolygon(v, 4, c, 0.f, c);
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
    bool show = pl && s.active() &&
        Mod::get()->getSettingValue<bool>(s.phase == Phase::Playback ? "playback-map" : "show-map");
    this->setVisible(show);
    if (!show) return;

    m_rateTimer += dt;
    if (m_rateTimer >= 0.5f) {
        m_rate = (s.stats.attempts - m_lastAttempts) / m_rateTimer;
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

    // ---- layout ----
    bool compact = s.phase == Phase::Playback;
    CCRect panel = compact
        ? CCRect(win.width * 0.5f, win.height * 0.55f, win.width * 0.5f - 8.f, win.height * 0.45f - 8.f)
        : CCRect(14.f, win.height * 0.30f, win.width - 28.f, win.height * 0.70f - 10.f);
    const float textH = 34.f, barH = 16.f;
    m_view = CCRect(panel.origin.x + 4.f, panel.origin.y + barH + 4.f, panel.size.width - 8.f, panel.size.height - textH - barH - 8.f);
    m_scale = m_view.size.width / (30.f * (compact ? 20.f : 26.f)); // 26 blocks across

    CCRect whole = m_view; // let fillRect draw the panel itself
    m_view = panel;
    fillRect(panel, ccc4f(0.04f, 0.05f, 0.07f, compact ? 0.80f : 0.90f));
    m_view = whole;
    fillRect(m_view, ccc4f(0.09f, 0.10f, 0.13f, 1.f));

    // ---- camera follows the simulated player ----
    auto pp = pl->m_player1->getPosition();
    m_camX = pp.x - m_view.size.width * 0.35f / m_scale;
    float wantY = pp.y - m_view.size.height * 0.45f / m_scale;
    wantY = std::max(wantY, -40.f);
    m_camY += (wantY - m_camY) * 0.25f;
    float worldW = m_view.size.width / m_scale;

    // ---- level geometry ----
    fillRect(CCRect(m_view.getMinX(), toScreen(0, -6).y, m_view.size.width, 6 * m_scale), ccc4f(0.35f, 0.40f, 0.48f, 1.f)); // ground
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

    // ---- paths ----
    int t = s.curTick;
    int lo = t - 900, hi = t + 2400;
    if (s.phase == Phase::Playback) {
        drawPath(s.path, s.plan, lo, hi, ccc4f(0.35f, 0.75f, 1.f, 0.9f), ccc4f(1.f, 0.75f, 0.2f, 1.f), 0.7f, true);
    } else {
        int end = s.phase == Phase::Search ? s.bestD : static_cast<int>(s.path.size()) - 1;
        // accepted plan: blue when released, orange while holding, dot at each click
        drawPath(s.path, s.plan, lo, std::min(hi, end), ccc4f(0.35f, 0.75f, 1.f, 0.9f), ccc4f(1.f, 0.75f, 0.2f, 1.f), 0.7f, true);
        // the try being simulated: white
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

    // ---- whole-level bar: green = verified, blue = planned, red ticks = where it dies ----
    float len = std::max(1.f, compat::levelLength(pl));
    auto xAt = [&](const std::vector<Sample>& v, int tick) {
        if (v.empty()) return 0.f;
        return v[std::clamp(tick, 0, static_cast<int>(v.size()) - 1)].x;
    };
    float planned = std::clamp(xAt(s.path, s.phase == Phase::Search ? s.bestD : static_cast<int>(s.path.size()) - 1) / len, 0.f, 1.f);
    float verified = 0.f;
    if (s.solved) verified = 1.f;
    else if (s.phase == Phase::Verify) verified = std::clamp(pp.x / len, 0.f, 1.f);
    else if (s.phase == Phase::Prune) verified = planned;

    CCRect save = m_view;
    m_view = panel;
    CCRect bar(panel.origin.x + 6.f, panel.origin.y + 5.f, panel.size.width - 12.f, 5.f);
    fillRect(bar, ccc4f(0.22f, 0.24f, 0.28f, 1.f));
    fillRect(CCRect(bar.origin.x, bar.origin.y, bar.size.width * planned, bar.size.height), ccc4f(0.30f, 0.70f, 1.f, 1.f));
    fillRect(CCRect(bar.origin.x, bar.origin.y, bar.size.width * verified, bar.size.height), ccc4f(0.40f, 0.90f, 0.35f, 1.f));
    int bins = static_cast<int>(s.deathBins.size());
    for (int i = 0; i < bins; ++i) {
        int n = s.deathBins[i];
        if (!n) continue;
        float h = std::min(6.f, 1.f + std::log2(1.f + n));
        fillRect(CCRect(bar.origin.x + bar.size.width * i / bins, bar.getMaxY() + 1.f, std::max(1.f, bar.size.width / bins), h),
                 ccc4f(1.f, 0.25f, 0.25f, 0.85f));
    }
    float cx = bar.origin.x + bar.size.width * std::clamp(pp.x / len, 0.f, 1.f);
    fillRect(CCRect(cx - 0.75f, bar.origin.y - 2.f, 1.5f, bar.size.height + 4.f), ccc4f(1.f, 1.f, 1.f, 1.f));
    m_view = save;

    // ---- text ----
    const char* phase =
        s.phase == Phase::Search ? "SEARCHING" :
        s.phase == Phase::Verify ? "VERIFYING" :
        s.phase == Phase::Prune ? "REMOVING EXTRA CLICKS" : "PLAYING MACRO";
    float tx = panel.origin.x + 6.f, ty = panel.getMaxY() - 3.f;
    m_line1->setPosition({tx, ty});
    m_line2->setPosition({tx, ty - 11.f});
    m_line3->setPosition({tx, ty - 21.f});
    m_line1->setString(fmt::format("{}   {:.1f}%   tick {}   clicks {}", phase, planned * 100.f, t, s.clickCount(s.plan)).c_str());
    if (s.phase == Phase::Playback) {
        m_line2->setString(fmt::format("{:.2f}s of {:.2f}s", t / 240.f, s.plan.size() / 240.f).c_str());
        m_line3->setString("blue = release   orange = hold   dot = click");
    } else {
        m_line2->setString(fmt::format(
            "tries {} ({:.0f}/s)   deaths {}   full runs {}   repairs {}   clicks removed {}",
            s.stats.attempts, m_rate, s.stats.deaths, s.stats.rounds, s.stats.fixups, s.stats.pruned).c_str());
        m_line3->setString(fmt::format(
            "look-back {:.2f}s   stuck {}   blue/orange = plan (release/hold)   white = current try   red x = deaths",
            s.stats.window / 240.f, s.stuck).c_str());
    }
}

}

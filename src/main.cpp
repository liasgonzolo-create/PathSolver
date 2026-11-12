#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <chrono>
#include "Solver.hpp"
#include "Overlay.hpp"
#include "compat.hpp"

using namespace geode::prelude;
using ps::Phase;
using ps::Solver;

// While the solver or a macro is running, this mod drives the game itself:
// one call of the original update == exactly one 240 Hz tick, with the
// plan's input applied just before it. Solving and playback use the same
// stepping, so what the solver saw is what playback does.
class $modify(PSBaseLayer, GJBaseGameLayer) {
    void update(float dt) {
        auto& s = Solver::get();
        auto pl = PlayLayer::get();
        if (!pl || pl != static_cast<GJBaseGameLayer*>(this) || !s.active()) {
            GJBaseGameLayer::update(dt);
            return;
        }

        if (s.phase == Phase::Playback) {
            int n = s.playbackSteps(pl, dt);
            for (int i = 0; i < n && s.phase == Phase::Playback; ++i) {
                s.playbackPre(pl);
                GJBaseGameLayer::update(ps::TICKF);
            }
            return;
        }

        auto start = std::chrono::steady_clock::now();
        auto budget = std::chrono::milliseconds(s.budgetMs);
        do {
            if (s.preStep(pl)) {
                GJBaseGameLayer::update(ps::TICKF);
                s.postStep(pl);
            }
        } while (s.solving() && std::chrono::steady_clock::now() - start < budget);
    }

    // Real key presses are ignored while the bot owns the button.
    void handleButton(bool down, int button, bool isPlayer1) {
        auto& s = Solver::get();
        if (s.active() && !s.injecting() && PlayLayer::get() == static_cast<GJBaseGameLayer*>(this)) return;
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

class $modify(PSPlayLayer, PlayLayer) {
    struct Fields {
        bool restoreTestMode = false;
        bool oldTestMode = false;
    };

#if GEODE_COMP_GD_VERSION >= 22091
    bool init(GJGameLevel* level, bool dontCreateObjects) {
        Solver::get().onLevelEnter(level);
        if (!PlayLayer::init(level, dontCreateObjects)) return false;
#else
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        Solver::get().onLevelEnter(level);
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
#endif
        auto overlay = ps::Overlay::create();
        overlay->setID("progress-map"_spr);
        this->addChild(overlay, 10000);
        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto& s = Solver::get();
        if (object != m_anticheatSpike && s.solving()) {
            s.onDeath(); // no death animation; the solver rewinds right away
            return;
        }
        PlayLayer::destroyPlayer(player, object);
    }

    void levelComplete() {
        auto& s = Solver::get();
        if (s.solving()) {
            s.onComplete();
            return;
        }
        // Safe mode: a run the macro played is never saved as a real completion.
        if (s.phase == Phase::Playback || s.usedMacro()) {
            m_fields->oldTestMode = ps::compat::testMode(this);
            m_fields->restoreTestMode = true;
            ps::compat::testMode(this) = true;
        }
        PlayLayer::levelComplete();
    }

    void resetLevel() {
        if (m_fields->restoreTestMode) {
            ps::compat::testMode(this) = m_fields->oldTestMode;
            m_fields->restoreTestMode = false;
        }
        PlayLayer::resetLevel();
        Solver::get().onReset(this);
    }

    void onQuit() {
        Solver::get().stop(this);
        Solver::get().onQuit(this);
        PlayLayer::onQuit();
    }
};

class $modify(PSPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto& s = Solver::get();
        auto menu = CCMenu::create();
        menu->setID("menu"_spr);
        menu->setPosition({0.f, 0.f});
        this->addChild(menu, 20);

        float x = 48.f;
        auto add = [&](const char* text, SEL_MenuHandler cb) {
            auto spr = ButtonSprite::create(text);
            spr->setScale(0.5f);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
            float w = spr->getContentSize().width * 0.5f;
            btn->setPosition({x + w / 2.f, 22.f});
            x += w + 6.f;
            menu->addChild(btn);
        };
        add(s.solving() ? "Stop solver" : (s.plan.empty() ? "Solve" : "Re-solve"), menu_selector(PSPauseLayer::onSolve));
        if (!s.plan.empty() && !s.solving())
            add(s.phase == Phase::Playback ? "Stop macro" : "Play macro", menu_selector(PSPauseLayer::onPlay));

        auto info = CCLabelBMFont::create(
            fmt::format("Path Solver: {}{}", s.status,
                s.plan.empty() ? "" : fmt::format(" ({} clicks)", s.clickCount(s.plan))).c_str(),
            "chatFont.fnt");
        info->setAnchorPoint({0.f, 0.5f});
        info->setScale(0.5f);
        info->setPosition({48.f, 40.f});
        info->setID("status"_spr);
        this->addChild(info, 20);
    }

    void onSolve(CCObject*) {
        auto& s = Solver::get();
        if (s.solving()) s.stop(PlayLayer::get());
        else {
            s.stop(PlayLayer::get());
            s.plan.clear();
            s.path.clear();
            s.startSolve();
        }
        this->onResume(nullptr);
    }

    void onPlay(CCObject*) {
        auto& s = Solver::get();
        if (s.phase == Phase::Playback) s.stop(PlayLayer::get());
        else s.startPlayback();
        this->onResume(nullptr);
    }
};

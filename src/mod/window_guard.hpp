#pragma once
// The game window held at the size it was launched with (cfg windowguard; off in the mod as shipped,
// on for the verification workers, whose launcher writes windowguard=1).
//
// A worker's save fixes the resolution GD comes up at (py/gdtas/gdsave.py), but nothing kept the
// window there afterwards: moving it across displays resizes it, and two workers launched from the
// same resolution index were measured at 1,660x934 and 1,706x960 client pixels on 2026-10-14. The
// size is not cosmetic. The screen width moves the firing tick of camera-driven triggers (gdsave.py's
// note, lv19 on 2026-08-07), and the height picks the saw radii a level loads with (heights up to
// 864 against 900 and up).
//
// The launch size is taken when the mod loads, from the window GD created out of the save. Before a
// level is entered and before every head reset of a driven session, a window that is not at that
// size is put back, the way GD's own resolution setting resizes it (PlatformToolbox::resizeWindow:
// CCEGLView::resizeWindow, then centerWindow), and result.txt gets a `window:` line saying so.
#include <Geode/Geode.hpp>
#include <cmath>
#include <string>

namespace windowguard {

inline cocos2d::CCSize g_launch{0.f, 0.f};

inline void noteLaunch() {
    if (auto* v = cocos2d::CCEGLView::sharedOpenGLView()) g_launch = v->getFrameSize();
}

// `where` names the caller in the line; `say` writes one for a window already at its size too (the
// level entry, so every level's log records the size it ran at). Returns whether it was put back.
template <class Write>
inline bool enforce(const char* where, bool say, Write&& write) {
    auto* v = cocos2d::CCEGLView::sharedOpenGLView();
    if (!v || g_launch.width <= 0.f || g_launch.height <= 0.f) {
        if (say) write("window: " + std::string(where) + " no launch size recorded");
        return false;
    }
    const cocos2d::CCSize now = v->getFrameSize();
    if (std::fabs(now.width - g_launch.width) < 0.5f && std::fabs(now.height - g_launch.height) < 0.5f) {
        if (say)
            write("window: " + std::string(where) + " " + std::to_string((int)now.width) + "x"
                  + std::to_string((int)now.height) + " (the launch size)");
        return false;
    }
#ifdef GEODE_IS_WINDOWS
    v->resizeWindow((int)std::lround(g_launch.width), (int)std::lround(g_launch.height));
    v->centerWindow();
#endif
    const cocos2d::CCSize after = v->getFrameSize();
    write("window: " + std::string(where) + " found " + std::to_string((int)now.width) + "x"
          + std::to_string((int)now.height) + ", put back to " + std::to_string((int)g_launch.width) + "x"
          + std::to_string((int)g_launch.height) + " (now " + std::to_string((int)after.width) + "x"
          + std::to_string((int)after.height) + ")");
    return true;
}

}  // namespace windowguard

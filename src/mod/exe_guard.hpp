#pragma once
// Puts nothing into a Geometry Dash executable the mod was not built for.
//
// A hook, a call and a member offset of this mod are the numbers of one build of the game. Geometry Dash 2.209 exists as two:
// the first Steam build and a hotfix a few days later that moved almost every function and shrank GJBaseGameLayer by 0x40 bytes, with
// the same version string. A mod built for the one and run on the other writes its hooks into the middle of other functions and the
// game dies at start (the log just stops after "hooks enabled"). Geode checks the version string only, so the check is the mod's own.
//
// The build's identity is the TimeDateStamp of the PE headers of GeometryDash.exe, libcocos2d.dll and libExtensions.dll (the number Geode
// prints as "GD v<n>" is the exe's). Each build of the mod lists the set it was made for (kBuilds below, chosen by GEODE_COMP_GD_VERSION: 22090 the
// first, 22091 the hotfix). If the running game is none of them, every `$modify` of the mod leaves its hooks off (GDSOLVER_HOOK_GUARD() in each
// class: Geode asks a class's onModify before it enables the class's hooks), the mod's start-up code does not run, and the player is told once, in
// the log and with a toast of the mod's own that uses nothing from the game but cocos2d.
//
// Only while Geode runs in forward compatibility mode (its own GD version differs from the game's): a Geode that supports this GD version
// checks the mod's version itself and the guard stays out of the way. GDSOLVER_EXE_GUARD (CMake option; default on for 2.209 builds, off for the
// others, which also do not exist as Android) turns the whole thing off at build time -- the way out once Geode names this GD version.
#include <Geode/Geode.hpp>
#include <cstdint>
#include <string>

#ifndef GDSOLVER_EXE_GUARD
#if GEODE_COMP_GD_VERSION >= 22090 && defined(GEODE_IS_WINDOWS)
#define GDSOLVER_EXE_GUARD 1
#else
#define GDSOLVER_EXE_GUARD 0
#endif
#endif
#if GDSOLVER_EXE_GUARD && GEODE_COMP_GD_VERSION < 22090
#error "GDSOLVER_EXE_GUARD lists executables of GD 2.209 only; build for 2.209 or leave the guard off"
#endif

namespace gdsolver::exeguard {

struct Build {
    std::uint32_t exe, cocos, extensions;   // PE TimeDateStamp of GeometryDash.exe, libcocos2d.dll, libExtensions.dll
    const char* name;
};

// The executables this build of the mod is made for.
#if GEODE_COMP_GD_VERSION >= 22091
inline constexpr Build kBuilds[] = {
    {0x6ad2d987u, 0x6ad2d6fcu, 0x6ad2d956u, "2.209, Steam build 26002420 (exe 4EEE06D5, 2026-10-17)"},
};
#else
inline constexpr Build kBuilds[] = {
    {0x6ace3633u, 0x6acbff38u, 0x6acbefc5u, "2.209, Steam build 25909607 (exe 524265B3, 2026-10-13)"},
};
#endif

struct Verdict {
    bool guarded = false;          // the guard applies here (forward compatibility mode, GDSOLVER_EXE_GUARD)
    bool match = true;             // the running game is one of kBuilds
    std::uint32_t exe = 0, cocos = 0, extensions = 0;
};

const Verdict& verdict();

// May the mod put its code into this game?
inline bool allowed() { return !verdict().guarded || verdict().match; }

// What the log says, once: the three stamps found and the build the mod was made for.
std::string message();

// What the player is told, once, on a single line of the toast.
std::string notice();

// For every `$modify` class: its hooks stay off unless the game is one the mod was built for.
template <class Self>
void onModify(Self& self) {
    if (allowed()) return;
    for (auto& entry : self.m_hooks) entry.second->setAutoEnable(false);
}

}  // namespace gdsolver::exeguard

#define GDSOLVER_HOOK_GUARD() \
    static void onModify(auto& self) { ::gdsolver::exeguard::onModify(self); }

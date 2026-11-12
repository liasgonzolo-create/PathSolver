// The attempt pin (cfg `attemptone`, see g_attemptOne in config.hpp): while the mod drives, the
// attempt count reads what it reads on a session's first attempt, and the items Item Persistence
// keeps are not carried into the next attempt. cfg `attemptcount=k` (g_attemptCount) pins the count
// alone to k on every attempt.
// Windows only: the addresses and the two callers of getItemValue were read from this build.
#include "mod/playlayer_helpers.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GJEffectManager.hpp>

using namespace p1;

#ifdef GEODE_IS_WINDOWS

// What the attempt count reads on a session's first attempt. The layer counts the attempts BEFORE
// the current one: measured on a custom level whose Item Compare at x=0 reads it, attempts 1 to 6
// of one session read 0 to 5. Answering 1 instead turned the first attempt itself into a later
// one (its compare "> 0" came out true and the player was teleported on tick 3).
constexpr double kFirstAttemptCount = 0.0;

class $modify(AttemptPinLayer, GJBaseGameLayer) {
    // Type 5 is the attempt count (+0x3084 on the layer); every other type is the game's.
    double getItemValue(int type, int id) {
        const double v = GJBaseGameLayer::getItemValue(type, id);
        if (type != 5 || !botDriving()) return v;
        // The first reads of a session say what the game itself had (and on which attempt), so
        // what the pin changed can be read back.
        if (g_attemptReadsSeen++ < 20)
            writeResult("attemptone: attempt count read on attempt " + std::to_string(g_attempt)
                        + ": the game has " + std::to_string((long long)v)
                        + (g_attemptOne ? ", answered as on the first attempt"
                           : g_attemptCount >= 0 ? ", answered as " + std::to_string(g_attemptCount)
                                                     + " (cfg attemptcount)"
                                                 : std::string(", left as it is")));
        if (g_attemptOne) {
            ++g_attemptReadsPinned;
            return kFirstAttemptCount;
        }
        // cfg attemptcount (config.hpp): the same count on every attempt
        if (g_attemptCount >= 0) {
            ++g_attemptReadsPinned;
            return (double)g_attemptCount;
        }
        return v;
    }
};

class $modify(AttemptPinEffects, GJEffectManager) {
    // Called by resetEffects (every attempt) and loadFromState (every checkpoint restore) to copy
    // the kept items back. Not copying them leaves those items where a first attempt has them.
    void transferPersistentItems() {
        if (attemptPinActive()) {
            ++g_persistTransfersSkipped;
            return;
        }
        GJEffectManager::transferPersistentItems();
    }
};

#endif

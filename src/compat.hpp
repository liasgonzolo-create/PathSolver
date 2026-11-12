#pragma once
// Every place this mod touches a Geometry Dash member that could be renamed
// between binding versions lives in this file. If the build fails, the fix is
// almost always a one-line change here.
#include <Geode/Geode.hpp>

namespace ps::compat {

// The game's own physics step counter (240 per second).
inline int rawTick(PlayLayer* pl) { return pl->m_gameState.m_currentProgress; }

// Practice-mode checkpoints are the solver's save states.
// Alternative if markCheckpoint is not bound: pl->storeCheckpoint(pl->createCheckpoint());
inline void makeCheckpoint(PlayLayer* pl) { pl->markCheckpoint(); }
inline void popCheckpoint(PlayLayer* pl) { pl->removeCheckpoint(false); }
inline size_t cpCount(PlayLayer* pl) {
    return pl->m_checkpointArray ? pl->m_checkpointArray->count() : 0;
}

inline bool isPractice(PlayLayer* pl) { return pl->m_isPracticeMode; }
inline bool isPlatformer(PlayLayer* pl) { return pl->m_isPlatformer; }
inline float levelLength(PlayLayer* pl) { return pl->m_levelLength; }
inline bool& testMode(PlayLayer* pl) { return pl->m_isTestMode; }

// Show / hide everything the level draws (objects, players, effects).
inline void setWorldVisible(PlayLayer* pl, bool visible) {
    if (pl->m_objectLayer) pl->m_objectLayer->setVisible(visible);
}

inline int getAttempts(GJGameLevel* l) { return l->m_attempts.value(); }
inline void setAttempts(GJGameLevel* l, int v) { l->m_attempts = v; }
inline int levelId(GJGameLevel* l) { return l->m_levelID.value(); }

// Button 1 = jump, last argument = player 1.
inline void press(PlayLayer* pl, bool down) { pl->handleButton(down, 1, true); }

inline cocos2d::CCRect objectRect(GameObject* o) { return o->getObjectRect(); }

// Map colour class of an object, from GameObjectType's numeric value:
// 0 hidden, 1 solid, 2 hazard, 3 orb, 4 pad, 5 portal, 6 slope.
inline int objectKind(GameObject* o) {
    switch (static_cast<int>(o->m_objectType)) {
        case 0: return 1;                                   // Solid
        case 25: return 6;                                  // Slope
        case 2: case 47: return 2;                          // Hazard, AnimatedHazard
        case 11: case 12: case 13: case 29: case 32:
        case 35: case 36: case 37: case 38: case 43:
        case 46: return 3;                                  // rings / orbs
        case 8: case 9: case 10: case 34: case 44: return 4; // pads
        case 3: case 4: case 5: case 6: case 16: case 17:
        case 18: case 19: case 23: case 24: case 26:
        case 27: case 28: case 33: case 41: case 42: return 5; // portals
        default: return 0;
    }
}

}

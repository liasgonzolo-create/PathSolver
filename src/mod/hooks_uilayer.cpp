// UILayer hooks: the practice-mode checkpoint keys and buttons, for the human takeover
// (mod/takeover.hpp).
#include "mod/playlayer_helpers.hpp"
#include <Geode/modify/UILayer.hpp>

class $modify(UILayer) {
    // X while a person plays a takeover: the takeover's newest strict checkpoint goes, never the
    // start point's (takeover::removeByKey).
    //
    // Caught here, at the key, because the key does not go through onDeleteCheck: handleKeypress
    // (0x4cd830) jumps straight to PlayLayer::removeCheckpoint (0x3b7f00) for X, where Z goes to
    // onCheck. And not at removeCheckpoint, which the game also calls itself with the same
    // argument: from checkCollisions (0x214746) and in a loop from removeAllCheckpoints (0x3b8065)
    // that runs until the game's list is empty. Once per press: the key's repeat is not a second
    // removal.
    void handleKeypress(cocos2d::enumKeyCodes key, bool down, double timestamp) {
        if (key == cocos2d::enumKeyCodes::KEY_X && takeover::personDriving()) {
            static bool s_down = false;
            if (down && !s_down) takeover::removeByKey(PlayLayer::get());
            s_down = down;
            return;
        }
        UILayer::handleKeypress(key, down, timestamp);
    }

    // ...and the on-screen delete button, which is onDeleteCheck's (0x4ce0e0).
    void onDeleteCheck(cocos2d::CCObject* sender) {
        if (takeover::personDriving()) {
            takeover::removeByKey(PlayLayer::get());
            return;
        }
        UILayer::onDeleteCheck(sender);
    }
};

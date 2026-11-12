#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include "Solver.hpp"

namespace ps {

// The progress map: a scrolling mini view of the level with the planned path,
// the try being simulated, click marks, recent deaths, and a whole-level bar.
class Overlay : public cocos2d::CCNode {
public:
    static Overlay* create();
    bool init() override;
    void update(float dt) override;

private:
    struct Geo { GameObject* obj; int kind; };
    void buildIndex(PlayLayer* pl);
    void redraw(PlayLayer* pl);
    void fillRect(cocos2d::CCRect r, const cocos2d::ccColor4F& c);
    void line(cocos2d::CCPoint a, cocos2d::CCPoint b, float w, const cocos2d::ccColor4F& c);
    void drawPath(const std::vector<Sample>& pts, const std::vector<uint8_t>& holds, int lo, int hi,
                  const cocos2d::ccColor4F& rel, const cocos2d::ccColor4F& hold, float w, bool dots);
    cocos2d::CCPoint toScreen(float wx, float wy) const;

    cocos2d::CCDrawNode* m_draw = nullptr;
    cocos2d::CCLabelBMFont* m_line1 = nullptr;
    cocos2d::CCLabelBMFont* m_line2 = nullptr;
    cocos2d::CCLabelBMFont* m_line3 = nullptr;
    std::vector<std::vector<Geo>> m_buckets;
    bool m_built = false;
    cocos2d::CCRect m_view;      // map area on screen
    float m_scale = 0.5f, m_camX = 0, m_camY = 0;
    int m_frame = 0, m_lastAttempts = 0;
    float m_rateTimer = 0, m_rate = 0;
};

}

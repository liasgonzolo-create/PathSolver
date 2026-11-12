#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include "Solver.hpp"

namespace ps {

// Solver screen: status text, progress bars and hotkey list on the left,
// the scrolling level map with the planned path on the right.
class Overlay : public cocos2d::CCNode {
public:
    static Overlay* create();
    bool init() override;
    void update(float dt) override;

private:
    struct Geo { GameObject* obj; int kind; };
    void buildIndex(PlayLayer* pl);
    void redraw(PlayLayer* pl);
    void drawMap(PlayLayer* pl, cocos2d::CCRect panel);
    float drawBar(float x, float top, float w, float frac, const cocos2d::ccColor4F& fill);
    void fillRect(cocos2d::CCRect r, const cocos2d::ccColor4F& c);
    void rawRect(cocos2d::CCRect r, const cocos2d::ccColor4F& c);
    void line(cocos2d::CCPoint a, cocos2d::CCPoint b, float w, const cocos2d::ccColor4F& c);
    void drawPath(const std::vector<Sample>& pts, const std::vector<uint8_t>& holds, int lo, int hi,
                  const cocos2d::ccColor4F& rel, const cocos2d::ccColor4F& hold, float w, bool dots);
    cocos2d::CCPoint toScreen(float wx, float wy) const;

    cocos2d::CCDrawNode* m_draw = nullptr;
    cocos2d::CCLabelBMFont* m_title = nullptr;
    cocos2d::CCLabelBMFont* m_head = nullptr;
    cocos2d::CCLabelBMFont* m_cap1 = nullptr;
    cocos2d::CCLabelBMFont* m_cap2 = nullptr;
    cocos2d::CCLabelBMFont* m_body = nullptr;
    cocos2d::CCLabelBMFont* m_mapCap = nullptr;
    std::vector<std::vector<Geo>> m_buckets;
    bool m_built = false;
    cocos2d::CCRect m_view;      // map area on screen
    float m_scale = 0.5f, m_camX = 0, m_camY = 0;
    int m_frame = 0;
    float m_rateTimer = 0;
    Perf m_lastPerf;
    int m_lastAttempts = 0;
    float m_tps = 0, m_resetMs = 0, m_cpMs = 0, m_tries = 0;
};

}

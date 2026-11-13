#include "touch_controls.h"

#include <SDL3/SDL.h>
#include <windows.h>   // platform/compat: compat_set_cursor, compat_set_touch_key, VK_*

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "../src/ui/gdi_text.h"

namespace {

constexpr double COIN_TIME = 0.15;     // how long a credit's coin switch is held
constexpr double START_DELAY = 0.35;   // then start, once the credit is counted
constexpr double START_TIME = 0.15;
constexpr double RELOAD_TAP = 0.12;    // a second finger's tap holds the reload this long

// Premultiplied BGRA "over"
inline void blend(uint32_t& dst, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    if (!a) return;
    uint32_t inv = 255 - a;
    uint32_t db = dst & 0xFF, dg = (dst >> 8) & 0xFF, dr = (dst >> 16) & 0xFF, da = dst >> 24;
    db = (b * a + db * inv) / 255;
    dg = (g * a + dg * inv) / 255;
    dr = (r * a + dr * inv) / 255;
    da = a + da * inv / 255;
    dst = db | dg << 8 | dr << 16 | da << 24;
}

// A filled disc with a ring, antialiased
void disc(std::vector<uint32_t>& px, int w, int h, float cx, float cy, float radius, float ring, uint32_t fill_a,
          uint32_t ring_a, bool pressed) {
    int x0 = std::max(0, int(cx - radius - 2)), x1 = std::min(w, int(cx + radius + 2));
    int y0 = std::max(0, int(cy - radius - 2)), y1 = std::min(h, int(cy + radius + 2));
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            float d = std::hypot(float(x) + 0.5f - cx, float(y) + 0.5f - cy);
            float cover = std::clamp(radius - d + 0.5f, 0.0f, 1.0f);
            if (cover <= 0) continue;
            float in_ring = std::clamp(d - (radius - ring) + 0.5f, 0.0f, 1.0f);
            uint32_t& dst = px[size_t(y) * size_t(w) + size_t(x)];
            // the body: dark, lit up while pressed
            uint32_t fa = uint32_t(float(pressed ? fill_a + 60 : fill_a) * cover);
            uint32_t shade = pressed ? 120 : 20;
            blend(dst, shade * fa / 255, shade * fa / 255, shade * fa / 255, fa);
            uint32_t ra = uint32_t(float(ring_a) * cover * in_ring);
            blend(dst, ra, ra, ra, ra);
        }
}

void bar(std::vector<uint32_t>& px, int w, int h, float x, float y, float bw, float bh, uint32_t a) {
    for (int yy = std::max(0, int(y)); yy < std::min(h, int(y + bh)); yy++)
        for (int xx = std::max(0, int(x)); xx < std::min(w, int(x + bw)); xx++)
            blend(px[size_t(yy) * size_t(w) + size_t(xx)], a, a, a, a);
}

}  // namespace

void TouchControls::set_window(int w, int h, float density, int left, int top, int right, int bottom) {
    if (w == w_ && h == h_ && density == density_ && left == safe_[0] && top == safe_[1] && right == safe_[2] &&
        bottom == safe_[3])
        return;
    w_ = w, h_ = h;
    density_ = density > 0 ? density : 1;
    safe_[0] = left, safe_[1] = top, safe_[2] = right, safe_[3] = bottom;
    layout();
}

void TouchControls::layout() {
    // Sizes in points (an iPhone's 3x screen: 2868 x 1320 pixels is 956 x 440)
    const float pt = density_;
    const float big = 88 * pt, small = 52 * pt, margin = 20 * pt;
    const float left = float(safe_[0]) + margin, right = float(w_ - safe_[2]) - margin;
    const float top = float(safe_[1]) + margin * 0.6f, bottom = float(h_ - safe_[3]) - margin;
    rect_[RELOAD] = {left, bottom - big, big, big};   // the left thumb
    rect_[START] = {right - big, bottom - big, big, big};
    rect_[PAUSE] = {left, top, small, small};
    rect_[BACK] = {left, top, small, small};
    drawn_key_.clear();
}

bool TouchControls::shown(Button b) const {
    if (pad_ || w_ <= 0) return false;
    switch (b) {
    case RELOAD: return in_game_ && playing_;
    case START: return in_game_ && !playing_;
    case PAUSE: return in_game_;
    case BACK: return !in_game_ && menu_back_;
    default: return false;
    }
}

int TouchControls::button_at(float x, float y) const {
    for (int b = 0; b < BUTTON_COUNT; b++)
        if (shown(Button(b)) && rect_[b].contains(x, y, 10 * density_)) return b;   // a little slop around each
    return -1;
}

void TouchControls::set_state(bool in_game, bool menu_back, bool pad, bool playing) {
    if (!in_game || pad) {   // a menu came up or a controller was connected: no gun stays held
        aiming_ = false;
        if (!in_game) held_.clear();
        reload_until_ = 0;
    }
    in_game_ = in_game, menu_back_ = menu_back, pad_ = pad, playing_ = playing;
}

bool TouchControls::on_event(const SDL_Event& e, float x, float y) {
    if (e.type != SDL_EVENT_FINGER_DOWN && e.type != SDL_EVENT_FINGER_MOTION && e.type != SDL_EVENT_FINGER_UP &&
        e.type != SDL_EVENT_FINGER_CANCELED)
        return false;
    uint64_t finger = uint64_t(e.tfinger.fingerID);
    auto held = std::find_if(held_.begin(), held_.end(), [&](const Held& h) { return h.finger == finger; });
    if (e.type == SDL_EVENT_FINGER_DOWN) {
        if (std::find(down_.begin(), down_.end(), finger) == down_.end()) down_.push_back(finger);
        // (a little later, so that a fourth finger can still make it the other gesture)
        if (down_.size() == 3) snap_from_ = now_ + 0.15, snap_until_ = now_ + 0.4;
        if (down_.size() == 4) {   // (not a snapshot): the frame rate on or off
            snap_until_ = 0;
            fps_on_ = !fps_on_;
            if (!fps_on_) fps_.clear();
        }
    } else if (e.type != SDL_EVENT_FINGER_MOTION) {
        down_.erase(std::remove(down_.begin(), down_.end(), finger), down_.end());
    }
    switch (e.type) {
    case SDL_EVENT_FINGER_DOWN: {
        if (pad_) return in_game_;   // the controller plays: touches do nothing in a stage
        int b = button_at(x, y);
        if (b >= 0) {
            held_.push_back({finger, Button(b)});
            if (b == START && start_until_ < now_) start_pending_ = true;   // a credit, then start (update)
            if (b == PAUSE || b == BACK) pending_key_ = VK_ESCAPE;
            return true;
        }
        if (!in_game_) return false;   // a menu click
        if (!aiming_) {
            aiming_ = true;
            aim_finger_ = finger;
            aim_x_ = x, aim_y_ = y;
        } else {
            reload_until_ = now_ + RELOAD_TAP;   // a second finger while firing: reload
        }
        return true;
    }
    case SDL_EVENT_FINGER_MOTION:
        if (aiming_ && finger == aim_finger_) {
            aim_x_ = x, aim_y_ = y;
            return true;
        }
        return held != held_.end() || in_game_;
    default:   // up or cancelled
        if (aiming_ && finger == aim_finger_) {
            aiming_ = false;
            aim_x_ = x, aim_y_ = y;
            return true;
        }
        if (held != held_.end()) {
            held_.erase(held);
            return true;
        }
        return in_game_;
    }
}

void TouchControls::update(double now) {
    now_ = now;
    if (start_pending_) {   // timed from the frame that sees it
        start_pending_ = false;
        coin_from_ = now, coin_until_ = now + COIN_TIME;
        start_from_ = now + START_DELAY, start_until_ = start_from_ + START_TIME;
    }
    bool reload = now < reload_until_;
    for (const Held& h : held_) reload |= h.button == RELOAD;
    bool fire = in_game_ && !pad_ && aiming_;
    if (aiming_) compat_set_cursor(int(std::lround(aim_x_)), int(std::lround(aim_y_)));
    compat_set_touch_key(VK_LBUTTON, fire);
    compat_set_touch_key(VK_RBUTTON, in_game_ && !pad_ && reload);
    compat_set_touch_key('5', now >= coin_from_ && now < coin_until_);
    compat_set_touch_key('1', now >= start_from_ && now < start_until_);
    compat_set_touch_key(VK_F9, now < snap_until_ && now > snap_from_);
}

int TouchControls::take_key() {
    int k = pending_key_;
    pending_key_ = 0;
    return k;
}

bool TouchControls::draw(std::vector<uint32_t>& px, int& w, int& h) {
    // What the picture shows: redrawn only when it changes
    std::string key = std::to_string(w_) + "x" + std::to_string(h_);
    for (int b = 0; b < BUTTON_COUNT; b++) {
        bool down = std::any_of(held_.begin(), held_.end(), [&](const Held& hb) { return hb.button == b; });
        if (b == RELOAD) down |= now_ < reload_until_;
        key += shown(Button(b)) ? (down ? 'D' : 'U') : '-';
    }
    key += "|" + fps_;
    if (key == drawn_key_) return false;
    drawn_key_ = key;
    w = w_, h = h_;
    if (w <= 0 || h <= 0) return false;
    px.assign(size_t(w) * size_t(h), 0);
    if (!text_) text_ = &GdiText::acquire("third_party/fonts");
    for (int b = 0; b < BUTTON_COUNT; b++) {
        if (!shown(Button(b))) continue;
        const Rect& r = rect_[b];
        bool down = key[key.size() - BUTTON_COUNT + size_t(b)] == 'D';
        float cx = r.x + r.w / 2, cy = r.y + r.h / 2, radius = r.w / 2;
        disc(px, w, h, cx, cy, radius, 2.5f * density_, 110, 200, down);
        if (b == PAUSE) {   // two bars
            float bw = radius * 0.22f, bh = radius * 0.8f;
            bar(px, w, h, cx - bw * 1.6f, cy - bh / 2, bw, bh, 230);
            bar(px, w, h, cx + bw * 0.6f, cy - bh / 2, bw, bh, 230);
            continue;
        }
        const wchar_t* label = b == RELOAD ? L"RELOAD" : b == START ? L"START" : L"BACK";
        if (!text_->ready()) continue;
        int size = int(std::lround((b == BACK ? 13.0f : 17.0f) * density_));
        const GdiText::Mask& m = text_->mask(GdiText::SAIRA, size, label);
        int ox = int(std::lround(cx)) - m.text_w / 2 - m.pad, oy = int(std::lround(cy)) - m.text_h / 2 - m.pad;
        for (int y = 0; y < m.h; y++) {
            int yy = oy + y;
            if (yy < 0 || yy >= h) continue;
            for (int x = 0; x < m.w; x++) {
                int xx = ox + x;
                if (xx < 0 || xx >= w) continue;
                uint32_t a = m.a[size_t(y) * size_t(m.w) + size_t(x)];
                blend(px[size_t(yy) * size_t(w) + size_t(xx)], a, a, a, a);
            }
        }
    }
    // The frame rate: white on a dark box in the top right corner, inside the safe area
    if (!fps_.empty() && text_->ready()) {
        std::wstring label(fps_.begin(), fps_.end());
        int size = int(std::lround(12.0f * density_));
        const GdiText::Mask& m = text_->mask(GdiText::SAIRA, size, label.c_str());
        float margin = 6.0f * density_, padx = 6.0f * density_, pady = 3.0f * density_;
        float bw = float(m.text_w) + 2 * padx, bh = float(m.text_h) + 2 * pady;
        float bx = float(w - safe_[2]) - margin - bw, by = float(safe_[1]) + margin;
        for (int y = std::max(0, int(by)); y < std::min(h, int(by + bh)); y++)
            for (int x = std::max(0, int(bx)); x < std::min(w, int(bx + bw)); x++) blend(px[size_t(y) * size_t(w) + size_t(x)], 0, 0, 0, 150);
        int ox = int(bx + padx) - m.pad, oy = int(by + pady) - m.pad;
        for (int y = 0; y < m.h; y++) {
            int yy = oy + y;
            if (yy < 0 || yy >= h) continue;
            for (int x = 0; x < m.w; x++) {
                int xx = ox + x;
                if (xx < 0 || xx >= w) continue;
                uint32_t a = m.a[size_t(y) * size_t(m.w) + size_t(x)];
                blend(px[size_t(yy) * size_t(w) + size_t(xx)], a, a, a, a);
            }
        }
    }
    return true;
}

void TouchControls::shutdown() {
    if (text_) text_->release();
    text_ = nullptr;
}

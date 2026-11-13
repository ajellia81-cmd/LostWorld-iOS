// iOS port: the light gun on a touch screen, for player 1.
//
// In a stage, a finger on the picture is the gun: the crosshair goes under
// it and the trigger is held while it stays down (it follows the finger). A
// second finger tapping anywhere while the first one fires reloads, and so
// does the RELOAD button at the bottom left (the left thumb). Between games a
// START button puts in a credit and starts (it starts alone with Free play),
// and PAUSE opens the pause menu. In the menus a tap is a click, and BACK
// goes back where the menu has a way back.
//
// Everything works through what the Windows code already reads: the pointer
// (GetCursorPos) and the keys (GetAsyncKeyState: VK_LBUTTON fires, VK_RBUTTON
// reloads, '5' is a coin, '1' start), so app.cpp's mouse code aims, reloads off
// the screen and starts as it does for a mouse. The buttons stay clear of the
// screen's rounded corners and camera cut-out (the safe area). With a game
// controller connected they disappear and the touches do nothing in a stage.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

union SDL_Event;
class GdiText;

class TouchControls {
public:
    // The window in pixels, its pixel density (pixels per point) and its
    // safe area's insets from each edge
    void set_window(int w, int h, float density, int left, int top, int right, int bottom);
    // Each frame, before the controls are polled:
    //   in_game: no menu is up (the touches are the gun)
    //   menu_back: a menu that BACK leaves
    //   pad: a game controller is connected (hidden and idle)
    //   playing: player 1 is in the game (RELOAD; otherwise START)
    void set_state(bool in_game, bool menu_back, bool pad, bool playing);
    // A touch, in window pixels; true when it is the controls' (not a menu click)
    bool on_event(const SDL_Event& e, float x, float y);
    // Pushes the gun and the buttons into the pointer and the keys
    void update(double now);
    // A key press for the window's input hook (VK_ESCAPE: PAUSE or BACK), 0 none
    int take_key();
    // The overlay picture (w x h BGRA, premultiplied) when it changed since the
    // last call; false when it did not. An empty picture hides it
    bool draw(std::vector<uint32_t>& px, int& w, int& h);
    // The frame rate in the top right corner (four fingers turn it on and off):
    // the text to show, set once a second while it is on
    bool fps_on() const { return fps_on_; }
    void set_fps(const std::string& text) { fps_ = fps_on_ ? text : std::string(); }
    bool any_shown() const {
        if (!fps_.empty()) return true;
        for (int b = 0; b < BUTTON_COUNT; b++)
            if (shown(Button(b))) return true;
        return false;
    }
    void shutdown();

private:
    enum Button { RELOAD, START, PAUSE, BACK, BUTTON_COUNT };
    struct Rect {
        float x = 0, y = 0, w = 0, h = 0;
        bool contains(float px, float py, float slop) const {
            return px >= x - slop && px < x + w + slop && py >= y - slop && py < y + h + slop;
        }
    };
    bool shown(Button b) const;
    void layout();
    int button_at(float x, float y) const;

    int w_ = 0, h_ = 0;
    float density_ = 1;
    int safe_[4] = {};   // left, top, right, bottom
    bool in_game_ = false, menu_back_ = false, pad_ = false, playing_ = false;
    Rect rect_[BUTTON_COUNT];

    // Fingers (SDL finger ids): the one that aims and fires, the ones on buttons
    uint64_t aim_finger_ = 0;
    bool aiming_ = false;
    float aim_x_ = 0, aim_y_ = 0;
    struct Held { uint64_t finger; Button button; };
    std::vector<Held> held_;
    double now_ = 0;
    double reload_until_ = 0;            // a reload tap: VK_RBUTTON held this long
    // Every finger on the screen; three at once take a debug snapshot (F9: the
    // scene, its polygons and the picture into out/snap)
    std::vector<uint64_t> down_;
    double snap_until_ = 0, snap_from_ = 0;
    bool fps_on_ = false;
    std::string fps_;
    double coin_from_ = -1, coin_until_ = -1, start_from_ = -1, start_until_ = -1;
    bool start_pending_ = false;
    int pending_key_ = 0;

    // The picture: drawn again only when what it shows changes
    std::string drawn_key_;
    GdiText* text_ = nullptr;
};

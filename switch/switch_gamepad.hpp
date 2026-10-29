#pragma once
// Nintendo Switch controller -> InputEvents: a stick-driven virtual mouse plus
// KeyboardMD.ini hotkeys, drained by Platform::poll like the touch gestures.
//
//   Left stick   cursor (ZL held: precision)   Right stick  scroll the map
//   A / B        left / right click             L / R / ZR   Shift / Ctrl / Alt
//   D-pad        team 1..4 (R+: create, L+: add) X / Y       Deploy / TypeSelect
//   L3 / R3      Stop / CenterOnRadarEvent      + / -        Options / CenterBase
//
// Buttons use switch-sdl2's joystick indices (libnx HidNpadButton bit order),
// so the labels above are the ones printed on the Joy-Con / Pro Controller.
#include "platform.hpp"

#include <SDL.h>

#include <cstdint>
#include <deque>

namespace ra2engine {

class SwitchGamepad {
public:
    void init(int logical_w, int logical_h);
    void set_logical_size(int w, int h);
    // SDL joystick events from Platform::poll; true = consumed.
    bool handle(const SDL_Event& e);
    // Integrate the sticks (cursor motion, map scroll); call when idle.
    void tick(uint32_t now_ms);
    bool pop(InputEvent& ev);
    // A finger took over: park the virtual cursor there and hide it.
    void on_touch(int x, int y);
    // Arrow cursor for screens that expect the (absent) OS pointer.
    void draw_cursor(SDL_Renderer* ren) const;
    bool cursor_visible() const { return visible_; }

private:
    void open_all();
    void emit_pointer(InputEvent::Kind k, InputEvent::Button b, uint32_t t);
    void emit_key(int sdl_key, uint32_t t);
    int mod_mask() const;

    int w_ = 1280, h_ = 720;
    float cx_ = 640.0f, cy_ = 360.0f;     // cursor, logical pixels
    float pan_acc_x_ = 0.0f, pan_acc_y_ = 0.0f;
    uint32_t last_tick_ = 0;
    bool visible_ = false;
    bool debug_ = false;   // RA2_PAD_DEBUG: log buttons / sticks (sticks 4x a second)
    uint32_t last_log_ = 0;
    bool shift_ = false, ctrl_ = false, alt_ = false, precision_ = false;
    std::deque<InputEvent> out_;
};

}  // namespace ra2engine

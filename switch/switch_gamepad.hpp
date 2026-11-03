#pragma once
// Nintendo Switch controller -> InputEvents: a stick-driven virtual mouse plus
// KeyboardMD.ini hotkeys, drained by Platform::poll like the touch gestures.
// The button map is in switch/README.md ("Controls"); sdmc:/switch/ra2engine/keymap.keys
// can rebind buttons to keys / mouse buttons and move the cursor to the right stick.
//
// Buttons use switch-sdl2's joystick indices (libnx HidNpadButton bit order),
// so the labels are the ones printed on the Joy-Con / Pro Controller.
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
    // Integrate the sticks (map scroll, cursor) and the hold timers; call when idle.
    void tick(uint32_t now_ms);
    bool pop(InputEvent& ev);
    // A finger took over: park the virtual cursor there and hide it.
    void on_touch(int x, int y);
    // A USB mouse moved: it drives the same cursor (shown) as the left stick.
    void on_mouse_move(int dx, int dy);
    // The cursor position for a mouse event.
    void mouse_pos(int& x, int& y) const { x = static_cast<int>(cx_); y = static_cast<int>(cy_); }
    // Arrow cursor for screens that expect the (absent) OS pointer.
    void draw_cursor(SDL_Renderer* ren) const;
    bool cursor_visible() const { return visible_; }
    bool debug() const { return debug_; }

private:
    // keymap.keys: one binding per button (Default = the built-in action).
    struct Binding {
        enum Kind { Default, None, MouseLeft, MouseRight, Key, Modifier, Sidebar, SidebarHold } kind = Default;
        int sdl_key = 0;   // Key
        int mods = 0;      // Key: the combo's KMOD bits; Modifier: the KMOD bits held
    };
    static constexpr int kButtons = 16;
    void load_keymap(const char* path);
    bool custom_button(int b, bool down, uint32_t t);   // true = a keymap binding handled it
    int held_mods() const;

    void open_all();
    void button(int b, bool down, uint32_t t);
    void emit_pointer(InputEvent::Kind k, InputEvent::Button b, uint32_t t, int x, int y);
    void emit_pointer(InputEvent::Kind k, InputEvent::Button b, uint32_t t);
    // A KeyboardMD.ini hotkey by Windows VK code; `ctrl` for TeamCreate,
    // `key` for the named keys (kKeyEscape, kKeyF1).
    void emit_vk(int vk, uint32_t t, bool ctrl = false, int key = 0);
    void move_cursor(float x, float y, uint32_t t);
    bool in_sidebar() const;
    // R's tap: the cursor to the build strip (remembering its battlefield spot) or back.
    void jump_sidebar(bool to_strip, uint32_t t);

    int w_ = 1280, h_ = 720;
    float cx_ = 640.0f, cy_ = 360.0f;          // cursor, logical pixels
    float map_x_ = 640.0f, map_y_ = 360.0f;    // R: the cursor's last battlefield spot
    float pan_acc_x_ = 0.0f, pan_acc_y_ = 0.0f;
    uint32_t last_tick_ = 0;
    bool visible_ = false;
    bool debug_ = false;   // RA2_PAD_DEBUG: log buttons / sticks (sticks 4x a second)
    uint32_t last_log_ = 0;

    bool zr_ = false;                     // force fire (Ctrl)
    bool zl_ = false, zl_used_ = false;   // attack-move (Ctrl+Shift); a lone tap = GuardObject
    uint32_t zl_down_ = 0;
    bool r_ = false, r_used_ = false;     // R tap: map <-> sidebar; R + D-pad: teams
    uint32_t l_down_ = 0;                 // L: tap = ScatterObject, hold = StopObject
    bool l_fired_ = false;
    bool y_ = false, y_drag_ = false;     // Y: hold + move = band box, tap = CombatantSelect
    int y_x_ = 0, y_y_ = 0;
    uint32_t l3_last_ = 0;                // L3 twice: CenterBase
    int tab_ = 0;                         // D-pad left/right: production tab
    Binding map_[kButtons];
    bool down_[kButtons] = {};
    bool cursor_right_ = false;   // keymap RSTICK_MOUSE: the right stick moves the cursor
    bool dpad_pan_ = false;       // keymap DPAD_ARROWS: the D-pad scrolls the map
    std::deque<InputEvent> out_;
};

}  // namespace ra2engine

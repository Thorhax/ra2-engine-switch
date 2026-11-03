// See switch_gamepad.hpp.
#include "switch_gamepad.hpp"

#include "sidebar.hpp"
#include "switch_platform.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace ra2engine {

namespace {
// switch-sdl2 joystick buttons (HidNpadButton bit order).
enum : int {
    kBtnA = 0, kBtnB = 1, kBtnX = 2, kBtnY = 3, kBtnLStick = 4, kBtnRStick = 5,
    kBtnL = 6, kBtnR = 7, kBtnZL = 8, kBtnZR = 9, kBtnPlus = 10, kBtnMinus = 11,
    kBtnLeft = 12, kBtnUp = 13, kBtnRight = 14, kBtnDown = 15,
};
enum : int { kAxisLX = 0, kAxisLY = 1, kAxisRX = 2, kAxisRY = 3 };

// KeyboardMD.ini [Hotkey] codes (engine/hotkeys.cpp kShipped): Windows VK codes.
enum : int {
    kVkCenterView = 12, kVkSpace = 32 /* CenterOnRadarEvent */, kVkEsc = 27 /* Options */,
    kVkTeam1 = '1', kVkTabInfantry = 'E', kVkGuard = 'G', kVkCenterBase = 'H',
    kVkRepair = 'K', kVkSell = 'L', kVkCombatants = 'P', kVkTabStructures = 'Q',
    kVkTabUnits = 'R', kVkStop = 'S', kVkDeploy = 'D', kVkTabDefenses = 'W', kVkScatter = 'X',
};
// D-pad left/right walk the sidebar tabs in their on-screen order.
constexpr int kTabs[] = {kVkTabStructures, kVkTabDefenses, kVkTabInfantry, kVkTabUnits};

constexpr float kDeadzone = 0.18f;
constexpr float kCursorSpeed = 0.95f;   // full deflection: screen widths / second
constexpr float kPanSpeed = 700.0f;     // logical px / second at full deflection...
constexpr float kPanEdgeBoost = 2.0f;   // ...times this with the stick pushed to the rim
constexpr uint32_t kHoldMs = 300;       // L / ZL: tap vs hold
constexpr uint32_t kDoubleMs = 350;     // L3 twice
constexpr int kDragSlop = 6;            // Y: px before the band box starts

// Radial deadzone + quadratic response, per axis pair; out in [-1, 1]. `raw`
// is the deflection before the curve (for the rim boost).
void stick(SDL_Joystick* j, int ax, int ay, float& x, float& y, float& raw) {
    x = SDL_JoystickGetAxis(j, ax) / 32767.0f;
    y = SDL_JoystickGetAxis(j, ay) / 32767.0f;
    const float m = std::sqrt(x * x + y * y);
    raw = m;
    if (m < kDeadzone) { x = y = 0.0f; return; }
    const float n = std::min(1.0f, (m - kDeadzone) / (1.0f - kDeadzone));
    x = x / m * n * n;
    y = y / m * n * n;
}
// keymap.keys button names -> joystick button.
int button_from_name(const std::string& n) {
    static const struct { const char* name; int b; } k[] = {
        {"A", kBtnA}, {"B", kBtnB}, {"X", kBtnX}, {"Y", kBtnY}, {"L", kBtnL}, {"R", kBtnR},
        {"ZL", kBtnZL}, {"ZR", kBtnZR}, {"PLUS", kBtnPlus}, {"+", kBtnPlus}, {"MINUS", kBtnMinus},
        {"-", kBtnMinus}, {"LSTICK", kBtnLStick}, {"L3", kBtnLStick}, {"LS", kBtnLStick},
        {"RSTICK", kBtnRStick}, {"R3", kBtnRStick}, {"RS", kBtnRStick}, {"DUP", kBtnUp},
        {"DPAD_UP", kBtnUp}, {"UP", kBtnUp}, {"DDOWN", kBtnDown}, {"DPAD_DOWN", kBtnDown},
        {"DOWN", kBtnDown}, {"DLEFT", kBtnLeft}, {"DPAD_LEFT", kBtnLeft}, {"LEFT", kBtnLeft},
        {"DRIGHT", kBtnRight}, {"DPAD_RIGHT", kBtnRight}, {"RIGHT", kBtnRight},
    };
    for (const auto& e : k)
        if (n == e.name) return e.b;
    return -1;
}

// A key name -> SDL keycode (SDL's own names: "h", "2", "Escape", "Home", "Left Ctrl", "F1", ...,
// plus the short forms other ports' keymaps use).
int key_from_name(std::string n) {
    static const struct { const char* alias; const char* sdl; } k[] = {
        {"ESC", "Escape"}, {"ENTER", "Return"}, {"CTRL", "Left Ctrl"}, {"LCTRL", "Left Ctrl"},
        {"RCTRL", "Right Ctrl"}, {"CONTROL", "Left Ctrl"}, {"SHIFT", "Left Shift"},
        {"LSHIFT", "Left Shift"}, {"RSHIFT", "Right Shift"}, {"ALT", "Left Alt"}, {"LALT", "Left Alt"},
        {"RALT", "Right Alt"}, {"DEL", "Delete"}, {"PGUP", "PageUp"}, {"PGDN", "PageDown"},
        {"PAGE UP", "PageUp"}, {"PAGE DOWN", "PageDown"}, {"BKSP", "Backspace"},
    };
    std::string u = n;
    for (char& c : u) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (const auto& e : k)
        if (u == e.alias) return SDL_GetKeyFromName(e.sdl);
    if (n.size() == 1) return std::tolower(static_cast<unsigned char>(n[0]));   // "h", "2"
    return SDL_GetKeyFromName(n.c_str());   // case-insensitive
}

int mod_of_key(int k) {
    switch (k) {
        case SDLK_LCTRL: return KMOD_LCTRL;
        case SDLK_RCTRL: return KMOD_RCTRL;
        case SDLK_LSHIFT: return KMOD_LSHIFT;
        case SDLK_RSHIFT: return KMOD_RSHIFT;
        case SDLK_LALT: return KMOD_LALT;
        case SDLK_RALT: return KMOD_RALT;
        default: return 0;
    }
}

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}
}  // namespace

void SwitchGamepad::load_keymap(const char* path) {
    std::ifstream in(path);
    if (!in) return;
    int n = 0;
    for (std::string line; std::getline(in, line);) {
        const std::size_t c = line.find_first_of("#;");
        if (c != std::string::npos) line.erase(c);
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq)), val = trim(line.substr(eq + 1));
        for (char& ch : key) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        std::string uval = val;
        for (char& ch : uval) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        const bool on = uval == "TRUE" || uval == "1" || uval == "YES" || uval == "ON";
        if (key == "RSTICK_MOUSE") { cursor_right_ = on; ++n; continue; }
        if (key == "LSTICK_MOUSE") { cursor_right_ = !on; ++n; continue; }
        if (key == "DPAD_ARROWS") { dpad_pan_ = on; ++n; continue; }
        // The stick that is not the cursor scrolls the map anyway.
        if (key == "LSTICK_ARROWS" || key == "RSTICK_ARROWS") { ++n; continue; }
        const int b = button_from_name(key);
        if (b < 0) {
            std::fprintf(stderr, "[keymap] %s: unknown setting ignored\n", key.c_str());
            continue;
        }
        Binding bind;
        if (uval.empty() || uval == "DEFAULT") {
            bind.kind = Binding::Default;
        } else if (uval == "NONE") {
            bind.kind = Binding::None;
        } else if (uval == "MOUSE_LEFT" || uval == "LEFT_CLICK") {
            bind.kind = Binding::MouseLeft;
        } else if (uval == "MOUSE_RIGHT" || uval == "RIGHT_CLICK") {
            bind.kind = Binding::MouseRight;
        } else {
            // "h", "ESC", "RIGHT CTRL", "CTRL+1": modifiers joined with '+' before the key.
            int mods = 0, k = 0;
            bool ok = true;
            std::size_t start = 0;
            while (ok) {
                const std::size_t plus = val.find('+', start + 1);   // a lone "+" is the key itself
                const std::string part = trim(val.substr(start, plus == std::string::npos ? std::string::npos
                                                                                          : plus - start));
                const int pk = key_from_name(part);
                if (pk == SDLK_UNKNOWN) ok = false;
                if (plus == std::string::npos) { k = pk; break; }
                if (!mod_of_key(pk)) ok = false;
                mods |= mod_of_key(pk);
                start = plus + 1;
            }
            if (!ok || k == SDLK_UNKNOWN) {
                std::fprintf(stderr, "[keymap] %s=%s: unknown key ignored\n", key.c_str(), val.c_str());
                continue;
            }
            if (mod_of_key(k) && !mods) {
                bind.kind = Binding::Modifier;   // held: Ctrl / Shift / Alt for the other buttons
                bind.mods = mod_of_key(k);
            } else {
                bind.kind = Binding::Key;
                bind.sdl_key = k;
                bind.mods = mods;
            }
        }
        map_[b] = bind;
        ++n;
    }
    std::fprintf(stderr, "[keymap] %s: %d setting(s)%s\n", path, n, cursor_right_ ? ", right stick = cursor" : "");
}

int SwitchGamepad::held_mods() const {
    int m = 0;
    for (int b = 0; b < kButtons; ++b)
        if (down_[b] && map_[b].kind == Binding::Modifier) m |= map_[b].mods;
    return m;
}

bool SwitchGamepad::custom_button(int b, bool down, uint32_t t) {
    if (b < 0 || b >= kButtons) return false;
    const Binding& bind = map_[b];
    switch (bind.kind) {
        case Binding::Default:
            return false;
        case Binding::None:
        case Binding::Modifier:   // read by held_mods()
            return true;
        case Binding::MouseLeft:
        case Binding::MouseRight: {
            const InputEvent::Button pb = bind.kind == Binding::MouseLeft ? InputEvent::Left : InputEvent::Right;
            if (!visible_) {   // first use after touch: announce the pointer first
                visible_ = true;
                emit_pointer(InputEvent::PointerMove, pb, t);
            }
            emit_pointer(down ? InputEvent::PointerDown : InputEvent::PointerUp, pb, t);
            return true;
        }
        case Binding::Key: {
            if (!down) return true;
            InputEvent e;
            key_event_from_sdl(bind.sdl_key, bind.mods | held_mods(), e);
            if (e.kind == InputEvent::None) return true;
            e.t_ms = t;
            if (debug_) std::fprintf(stderr, "[pad] keymap key vk=%d key=%d ctrl=%d shift=%d alt=%d\n", e.vk, e.key,
                                     e.ctrl ? 1 : 0, e.shift ? 1 : 0, e.alt ? 1 : 0);
            out_.push_back(e);
            return true;
        }
    }
    return false;
}

void SwitchGamepad::init(int logical_w, int logical_h) {
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) std::fprintf(stderr, "[pad] joystick init: %s\n", SDL_GetError());
    debug_ = SDL_getenv("RA2_PAD_DEBUG") != nullptr;
    SDL_JoystickEventState(SDL_ENABLE);
    set_logical_size(logical_w, logical_h);
    load_keymap(RA2_SWITCH_BASE_DIR "keymap.keys");
    cx_ = map_x_ = w_ * 0.5f;
    cy_ = map_y_ = h_ * 0.5f;
    open_all();
}

void SwitchGamepad::set_logical_size(int w, int h) {
    if (w <= 0 || h <= 0) return;
    cx_ = cx_ * w / w_;
    cy_ = cy_ * h / h_;
    map_x_ = map_x_ * w / w_;
    map_y_ = map_y_ * h / h_;
    w_ = w;
    h_ = h;
}

void SwitchGamepad::open_all() {
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_JoystickFromInstanceID(SDL_JoystickGetDeviceInstanceID(i))) continue;
        SDL_Joystick* j = SDL_JoystickOpen(i);
        std::fprintf(stderr, "[pad] open %d: %s (%d axes, %d buttons)\n", i,
                     j ? SDL_JoystickName(j) : SDL_GetError(), j ? SDL_JoystickNumAxes(j) : 0,
                     j ? SDL_JoystickNumButtons(j) : 0);
    }
}

bool SwitchGamepad::in_sidebar() const { return cx_ >= w_ - kSidebarW; }

void SwitchGamepad::emit_pointer(InputEvent::Kind k, InputEvent::Button b, uint32_t t, int x, int y) {
    InputEvent e;
    e.kind = k;
    e.x = x;
    e.y = y;
    e.button = b;
    // ZR: force fire (Ctrl+click); ZL: attack-move (Ctrl+Shift+click).
    const int mods = held_mods();   // keymap Ctrl / Shift / Alt buttons
    e.ctrl = zr_ || zl_ || (mods & KMOD_CTRL);
    e.shift = zl_ || (mods & KMOD_SHIFT);
    e.alt = (mods & KMOD_ALT) != 0;
    e.t_ms = t;
    out_.push_back(e);
}

void SwitchGamepad::emit_pointer(InputEvent::Kind k, InputEvent::Button b, uint32_t t) {
    emit_pointer(k, b, t, static_cast<int>(cx_), static_cast<int>(cy_));
}

void SwitchGamepad::emit_vk(int vk, uint32_t t, bool ctrl, int key) {
    if (debug_) std::fprintf(stderr, "[pad] key vk=%d ctrl=%d key=%d\n", vk, ctrl ? 1 : 0, key);
    InputEvent e;
    e.kind = InputEvent::Key;
    e.vk = vk;
    e.key = key;
    e.ctrl = ctrl;
    e.t_ms = t;
    out_.push_back(e);
}

void SwitchGamepad::move_cursor(float x, float y, uint32_t t) {
    const bool moved = static_cast<int>(x) != static_cast<int>(cx_) || static_cast<int>(y) != static_cast<int>(cy_);
    cx_ = x;
    cy_ = y;
    if (!in_sidebar()) {
        map_x_ = x;
        map_y_ = y;
    }
    if (moved || !visible_) {
        visible_ = true;
        emit_pointer(InputEvent::PointerMove, InputEvent::Left, t);
    }
}

bool SwitchGamepad::handle(const SDL_Event& e) {
    switch (e.type) {
        case SDL_JOYDEVICEADDED:
            open_all();
            return true;
        case SDL_JOYDEVICEREMOVED:
        case SDL_JOYAXISMOTION:   // sampled in tick()
            return true;
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
            if (debug_)
                std::fprintf(stderr, "[pad] button %d %s\n", e.jbutton.button,
                             e.type == SDL_JOYBUTTONDOWN ? "down" : "up");
            button(e.jbutton.button, e.type == SDL_JOYBUTTONDOWN, e.jbutton.timestamp);
            return true;
        default:
            return false;
    }
}

void SwitchGamepad::button(int b, bool down, uint32_t t) {
    // Any other button while R / ZL is held makes it a modifier, not a tap.
    if (down && b != kBtnR) r_used_ = true;
    if (down && b != kBtnZL) zl_used_ = true;
    if (b >= 0 && b < kButtons) down_[b] = down;
    if (custom_button(b, down, t)) return;
    if (dpad_pan_ && (b == kBtnUp || b == kBtnDown || b == kBtnLeft || b == kBtnRight) && !r_) return;   // tick()
    switch (b) {
        case kBtnA:
        case kBtnB: {
            if (y_drag_) break;   // the band box owns the left button
            const InputEvent::Button pb = b == kBtnA ? InputEvent::Left : InputEvent::Right;
            if (b == kBtnB && in_sidebar()) {
                // B always cancels / deselects; a right click on the sidebar
                // does neither (X has the cameo right click), so on release
                // click the battlefield where the cursor last was, then point
                // back. One whole click: moving away with the button held
                // would be a right-drag (a map scroll), not a deselect.
                if (down) break;
                const int mx = static_cast<int>(map_x_), my = static_cast<int>(map_y_);
                emit_pointer(InputEvent::PointerMove, pb, t, mx, my);
                emit_pointer(InputEvent::PointerDown, pb, t, mx, my);
                emit_pointer(InputEvent::PointerUp, pb, t, mx, my);
                emit_pointer(InputEvent::PointerMove, pb, t);
                break;
            }
            if (!visible_) {      // first use after touch: announce the pointer first
                visible_ = true;
                emit_pointer(InputEvent::PointerMove, pb, t);
            }
            emit_pointer(down ? InputEvent::PointerDown : InputEvent::PointerUp, pb, t);
            break;
        }
        case kBtnY:
            if (down) {
                y_ = true;
                y_drag_ = false;
                y_x_ = static_cast<int>(cx_);
                y_y_ = static_cast<int>(cy_);
            } else if (y_) {
                y_ = false;
                if (y_drag_) emit_pointer(InputEvent::PointerUp, InputEvent::Left, t);
                else emit_vk(kVkCombatants, t);
                y_drag_ = false;
            }
            break;
        case kBtnX:
            if (in_sidebar()) {   // right click on a cameo: hold / cancel production
                emit_pointer(down ? InputEvent::PointerDown : InputEvent::PointerUp, InputEvent::Right, t);
            } else if (down) {
                emit_vk(kVkDeploy, t);   // deploy / unload / the unit's special ability
            }
            break;
        case kBtnR:
            if (down) {
                r_ = true;
                r_used_ = false;
            } else if (r_) {
                r_ = false;
                if (!r_used_) {   // tap: jump between the battlefield and the build strip
                    if (in_sidebar()) {
                        move_cursor(map_x_, map_y_, t);
                    } else {
                        map_x_ = cx_;
                        map_y_ = cy_;
                        move_cursor(static_cast<float>(w_ - kSidebarW + kCameoX + kCameoW / 2),
                                    static_cast<float>(kStripTop + 1 + kCameoH / 2), t);
                    }
                }
            }
            break;
        case kBtnZR:
            zr_ = down;
            break;
        case kBtnZL:
            if (down) {
                zl_ = true;
                zl_used_ = false;
                zl_down_ = t;
            } else {
                zl_ = false;
                if (!zl_used_ && t - zl_down_ < kHoldMs) emit_vk(kVkGuard, t);
            }
            break;
        case kBtnL:
            if (down) {
                l_down_ = t ? t : 1;
                l_fired_ = false;
            } else if (l_down_) {
                if (!l_fired_) emit_vk(kVkScatter, t);   // tap
                l_down_ = 0;
            }
            break;
        case kBtnLStick:
            if (!down) break;
            if (l3_last_ && t - l3_last_ < kDoubleMs) {
                emit_vk(kVkCenterBase, t);   // twice: the MCV / Construction Yard
                l3_last_ = 0;
            } else {
                emit_vk(kVkSpace, t);        // CenterOnRadarEvent
                l3_last_ = t ? t : 1;
            }
            break;
        case kBtnRStick:
            if (down) emit_vk(kVkCenterView, t);
            break;
        case kBtnUp:
        case kBtnRight:
        case kBtnDown:
        case kBtnLeft: {
            if (!down) break;
            if (r_) {   // R held: control groups 1-4 (+ ZR: create)
                const int n = b == kBtnUp ? 0 : b == kBtnRight ? 1 : b == kBtnDown ? 2 : 3;
                emit_vk(kVkTeam1 + n, t, zr_);
                break;
            }
            if (b == kBtnUp) emit_vk(kVkRepair, t);
            else if (b == kBtnDown) emit_vk(kVkSell, t);
            else {
                tab_ = (tab_ + (b == kBtnRight ? 1 : 3)) % 4;
                emit_vk(kTabs[tab_], t);
            }
            break;
        }
        case kBtnPlus:
            if (down) emit_vk(kVkEsc, t, false, kKeyEscape);
            break;
        case kBtnMinus:
            // Objectives (campaign) / pause menu (skirmish): kKeyF1 with no VK,
            // so it never reaches KeyboardMD.ini's View1 (= F1) binding.
            if (down) emit_vk(0, t, false, kKeyF1);
            break;
        default:
            break;
    }
}

void SwitchGamepad::tick(uint32_t now) {
    const uint32_t prev = last_tick_;
    last_tick_ = now;
    if (l_down_ && !l_fired_ && now - l_down_ >= kHoldMs) {   // L held: Stop
        emit_vk(kVkStop, now);
        l_fired_ = true;
    }
    if (!prev || now <= prev) return;
    const float dt = std::min(0.1f, (now - prev) / 1000.0f);
    float lx = 0, ly = 0, l_raw = 0, rx = 0, ry = 0;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        SDL_Joystick* j = SDL_JoystickFromInstanceID(SDL_JoystickGetDeviceInstanceID(i));
        if (!j) continue;
        float x, y, raw;
        // lx/ly = the panning stick (the physical RIGHT stick), rx/ry = the
        // cursor stick (the physical LEFT stick).
        // keymap RSTICK_MOUSE swaps the two.
        stick(j, cursor_right_ ? kAxisLX : kAxisRX, cursor_right_ ? kAxisLY : kAxisRY, x, y, raw);
        if (x || y) { lx = x; ly = y; l_raw = raw; }
        stick(j, cursor_right_ ? kAxisRX : kAxisLX, cursor_right_ ? kAxisRY : kAxisLY, x, y, raw);
        if (x || y) { rx = x; ry = y; }
    }
    if (debug_ && (lx || ly || rx || ry) && now - last_log_ >= 250 && (last_log_ = now))
        std::fprintf(stderr, "[pad] sticks %.2f %.2f / %.2f %.2f cursor %.0f,%.0f\n", lx, ly, rx, ry, cx_, cy_);
    // Left stick: the cursor.
    if (rx || ry) {
        const float v = kCursorSpeed * w_ * dt;
        move_cursor(std::clamp(cx_ + rx * v, 0.0f, w_ - 1.0f), std::clamp(cy_ + ry * v, 0.0f, h_ - 1.0f), now);
        // Y held and moved: the band box (a left drag from where Y went down).
        if (y_ && !y_drag_ && (std::abs(static_cast<int>(cx_) - y_x_) > kDragSlop ||
                               std::abs(static_cast<int>(cy_) - y_y_) > kDragSlop)) {
            y_drag_ = true;
            emit_pointer(InputEvent::PointerMove, InputEvent::Left, now, y_x_, y_y_);
            emit_pointer(InputEvent::PointerDown, InputEvent::Left, now, y_x_, y_y_);
            emit_pointer(InputEvent::PointerMove, InputEvent::Left, now);
        }
    }
    // keymap DPAD_ARROWS: the D-pad scrolls the map like the arrow keys (at full deflection).
    if (dpad_pan_ && !r_ && !lx && !ly) {
        auto held = [this](int b) { return down_[b] && map_[b].kind == Binding::Default ? 1.0f : 0.0f; };
        lx = held(kBtnRight) - held(kBtnLeft);
        ly = held(kBtnDown) - held(kBtnUp);
    }
    // Right stick: scroll the map, faster with the stick on its rim.
    if (lx || ly) {
        const float boost = l_raw > 0.97f ? kPanEdgeBoost : 1.0f;
        // Pan's dx/dy is a finger drag (the map follows it): pushing the
        // stick right looks right, so the drag goes left.
        pan_acc_x_ -= lx * kPanSpeed * boost * dt;
        pan_acc_y_ -= ly * kPanSpeed * boost * dt;
        const int dx = static_cast<int>(pan_acc_x_), dy = static_cast<int>(pan_acc_y_);
        pan_acc_x_ -= dx;
        pan_acc_y_ -= dy;
        if (dx || dy) {
            InputEvent e;
            e.kind = InputEvent::Pan;
            e.dx = dx;
            e.dy = dy;
            // A pan is routed by where its drag started: start it on the
            // battlefield so it always scrolls the map, never the build strip.
            e.x = e.x0 = (w_ - kSidebarW) / 2;
            e.y = e.y0 = h_ / 2;
            e.t_ms = now;
            out_.push_back(e);
        }
    } else {
        pan_acc_x_ = pan_acc_y_ = 0.0f;
    }
}

bool SwitchGamepad::pop(InputEvent& ev) {
    if (out_.empty()) return false;
    ev = out_.front();
    out_.pop_front();
    return true;
}

void SwitchGamepad::on_touch(int x, int y) {
    cx_ = static_cast<float>(std::clamp(x, 0, w_ - 1));
    cy_ = static_cast<float>(std::clamp(y, 0, h_ - 1));
    visible_ = false;
}

void SwitchGamepad::on_mouse_move(int dx, int dy) {
    cx_ = std::clamp(cx_ + static_cast<float>(dx), 0.0f, static_cast<float>(w_ - 1));
    cy_ = std::clamp(cy_ + static_cast<float>(dy), 0.0f, static_cast<float>(h_ - 1));
    if (!in_sidebar()) {
        map_x_ = cx_;
        map_y_ = cy_;
    }
    visible_ = true;
}

void SwitchGamepad::draw_cursor(SDL_Renderer* ren) const {
    if (!visible_) return;
    // A plain arrow (tip at the hotspot): white fill, black outline.
    const float x = std::floor(cx_), y = std::floor(cy_);
    const SDL_FPoint pts[] = {{x, y}, {x, y + 17}, {x + 4.5f, y + 13}, {x + 8, y + 20},
                              {x + 10.5f, y + 19}, {x + 7, y + 12}, {x + 12, y + 12}};
    const int tris[] = {0, 1, 2, 0, 2, 5, 0, 5, 6, 2, 3, 4, 2, 4, 5};
    SDL_Vertex v[7];
    for (int i = 0; i < 7; ++i) v[i] = SDL_Vertex{pts[i], SDL_Color{255, 255, 255, 255}, SDL_FPoint{0, 0}};
    SDL_BlendMode bm;
    SDL_GetRenderDrawBlendMode(ren, &bm);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_RenderGeometry(ren, nullptr, v, 7, tris, 15);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    const SDL_FPoint outline[] = {pts[0], pts[1], pts[2], pts[3], pts[4], pts[5], pts[6], pts[0]};
    SDL_RenderDrawLinesF(ren, outline, 8);
    SDL_SetRenderDrawBlendMode(ren, bm);
}

}  // namespace ra2engine

// See switch_gamepad.hpp.
#include "switch_gamepad.hpp"

#include "sidebar.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

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
}  // namespace

void SwitchGamepad::init(int logical_w, int logical_h) {
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) std::fprintf(stderr, "[pad] joystick init: %s\n", SDL_GetError());
    debug_ = SDL_getenv("RA2_PAD_DEBUG") != nullptr;
    SDL_JoystickEventState(SDL_ENABLE);
    set_logical_size(logical_w, logical_h);
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
    e.ctrl = zr_ || zl_;
    e.shift = zl_;
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
        stick(j, kAxisLX, kAxisLY, x, y, raw);
        if (x || y) { lx = x; ly = y; l_raw = raw; }
        stick(j, kAxisRX, kAxisRY, x, y, raw);
        if (x || y) { rx = x; ry = y; }
    }
    if (debug_ && (lx || ly || rx || ry) && now - last_log_ >= 250 && (last_log_ = now))
        std::fprintf(stderr, "[pad] sticks %.2f %.2f / %.2f %.2f cursor %.0f,%.0f\n", lx, ly, rx, ry, cx_, cy_);
    // Right stick: the cursor.
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
    // Left stick: scroll the map, faster with the stick on its rim.
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

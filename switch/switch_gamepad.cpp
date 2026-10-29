// See switch_gamepad.hpp.
#include "switch_gamepad.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ra2engine {

namespace {
// switch-sdl2 joystick buttons (HidNpadButton bit order).
enum : int {
    kBtnA = 0, kBtnB = 1, kBtnX = 2, kBtnY = 3, kBtnLStick = 4, kBtnRStick = 5,
    kBtnL = 6, kBtnR = 7, kBtnZL = 8, kBtnZR = 9, kBtnPlus = 10, kBtnMinus = 11,
    kBtnLeft = 12, kBtnUp = 13, kBtnRight = 14, kBtnDown = 15,
};
enum : int { kAxisLX = 0, kAxisLY = 1, kAxisRX = 2, kAxisRY = 3 };

constexpr float kDeadzone = 0.18f;
constexpr float kCursorSpeed = 0.95f;   // full deflection: screen widths / second
constexpr float kPrecision = 0.3f;      // ZL held
constexpr float kPanSpeed = 1400.0f;    // full deflection: logical px / second

// Radial deadzone + quadratic response, per axis pair; out in [-1, 1].
void stick(SDL_Joystick* j, int ax, int ay, float& x, float& y) {
    x = SDL_JoystickGetAxis(j, ax) / 32767.0f;
    y = SDL_JoystickGetAxis(j, ay) / 32767.0f;
    const float m = std::sqrt(x * x + y * y);
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
    cx_ = w_ * 0.5f;
    cy_ = h_ * 0.5f;
    open_all();
}

void SwitchGamepad::set_logical_size(int w, int h) {
    if (w <= 0 || h <= 0) return;
    cx_ = cx_ * w / w_;
    cy_ = cy_ * h / h_;
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

int SwitchGamepad::mod_mask() const {
    return (shift_ ? KMOD_LSHIFT : 0) | (ctrl_ ? KMOD_LCTRL : 0) | (alt_ ? KMOD_LALT : 0);
}

void SwitchGamepad::emit_pointer(InputEvent::Kind k, InputEvent::Button b, uint32_t t) {
    InputEvent e;
    e.kind = k;
    e.x = static_cast<int>(cx_);
    e.y = static_cast<int>(cy_);
    e.button = b;
    e.shift = shift_;
    e.ctrl = ctrl_;
    e.alt = alt_;
    e.t_ms = t;
    out_.push_back(e);
}

void SwitchGamepad::emit_key(int sdl_key, uint32_t t) {
    InputEvent e;
    key_event_from_sdl(sdl_key, mod_mask(), e);
    e.t_ms = t;
    if (e.kind != InputEvent::None) out_.push_back(e);
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
            break;
        default:
            return false;
    }
    const bool down = e.type == SDL_JOYBUTTONDOWN;
    const uint32_t t = e.jbutton.timestamp;
    if (debug_) std::fprintf(stderr, "[pad] button %d %s\n", e.jbutton.button, down ? "down" : "up");
    switch (e.jbutton.button) {
        case kBtnA:
        case kBtnB: {
            const InputEvent::Button b = e.jbutton.button == kBtnA ? InputEvent::Left : InputEvent::Right;
            if (!visible_) {   // first use after touch: announce the pointer first
                visible_ = true;
                emit_pointer(InputEvent::PointerMove, b, t);
            }
            emit_pointer(down ? InputEvent::PointerDown : InputEvent::PointerUp, b, t);
            break;
        }
        case kBtnL: shift_ = down; break;
        case kBtnR: ctrl_ = down; break;
        case kBtnZR: alt_ = down; break;
        case kBtnZL: precision_ = down; break;
        default:
            if (!down) break;
            switch (e.jbutton.button) {
                case kBtnUp: emit_key(SDLK_1, t); break;
                case kBtnRight: emit_key(SDLK_2, t); break;
                case kBtnDown: emit_key(SDLK_3, t); break;
                case kBtnLeft: emit_key(SDLK_4, t); break;
                case kBtnX: emit_key(SDLK_d, t); break;        // DeployObject
                case kBtnY: emit_key(SDLK_t, t); break;        // TypeSelect
                case kBtnLStick: emit_key(SDLK_s, t); break;   // StopObject
                case kBtnRStick: emit_key(SDLK_SPACE, t); break;   // CenterOnRadarEvent
                case kBtnPlus: emit_key(SDLK_ESCAPE, t); break;    // Options / Back
                case kBtnMinus: emit_key(SDLK_h, t); break;        // CenterBase
                default: break;
            }
            break;
    }
    return true;
}

void SwitchGamepad::tick(uint32_t now) {
    const uint32_t prev = last_tick_;
    last_tick_ = now;
    if (!prev || now <= prev) return;
    const float dt = std::min(0.1f, (now - prev) / 1000.0f);
    float lx = 0, ly = 0, rx = 0, ry = 0;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        SDL_Joystick* j = SDL_JoystickFromInstanceID(SDL_JoystickGetDeviceInstanceID(i));
        if (!j) continue;
        float x, y;
        stick(j, kAxisLX, kAxisLY, x, y);
        if (x || y) { lx = x; ly = y; }
        stick(j, kAxisRX, kAxisRY, x, y);
        if (x || y) { rx = x; ry = y; }
    }
    if (debug_ && (lx || ly || rx || ry) && now - last_log_ >= 250 && (last_log_ = now))
        std::fprintf(stderr, "[pad] sticks %.2f %.2f / %.2f %.2f cursor %.0f,%.0f\n", lx, ly, rx, ry, cx_, cy_);
    if (lx || ly) {
        const float v = kCursorSpeed * w_ * dt * (precision_ ? kPrecision : 1.0f);
        const float nx = std::clamp(cx_ + lx * v, 0.0f, w_ - 1.0f);
        const float ny = std::clamp(cy_ + ly * v, 0.0f, h_ - 1.0f);
        const bool moved = static_cast<int>(nx) != static_cast<int>(cx_) ||
                           static_cast<int>(ny) != static_cast<int>(cy_);
        cx_ = nx;
        cy_ = ny;
        if (moved || !visible_) {
            visible_ = true;
            emit_pointer(InputEvent::PointerMove, InputEvent::Left, now);
        }
    }
    if (rx || ry) {
        // Pan's dx/dy is a finger drag (the map follows it): pushing the
        // stick right looks right, so the drag goes left.
        pan_acc_x_ -= rx * kPanSpeed * dt;
        pan_acc_y_ -= ry * kPanSpeed * dt;
        const int dx = static_cast<int>(pan_acc_x_), dy = static_cast<int>(pan_acc_y_);
        pan_acc_x_ -= dx;
        pan_acc_y_ -= dy;
        if (dx || dy) {
            InputEvent e;
            e.kind = InputEvent::Pan;
            e.dx = dx;
            e.dy = dy;
            // Routed by where the "drag" started: over the sidebar it scrolls
            // the cameo strip / radar, elsewhere the map.
            e.x = e.x0 = static_cast<int>(cx_);
            e.y = e.y0 = static_cast<int>(cy_);
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

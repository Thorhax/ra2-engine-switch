// Nintendo Switch platform glue (libnx services, SD-card paths, log file).
#include "switch_platform.hpp"

#include <SDL.h>
#include <arpa/inet.h>   // struct in_addr for __nxlink_host
#include <switch.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <strings.h>
#include <malloc.h>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <thread>

namespace {
constexpr const char* kPrefDir = RA2_SWITCH_BASE_DIR "save/";
constexpr const char* kLogFile = RA2_SWITCH_BASE_DIR "save/ra2engine.log";
}  // namespace

char* ra2_switch_base_path() { return SDL_strdup(RA2_SWITCH_BASE_DIR); }

unsigned long long ra2_switch_mem_used() {
    return mallinfo().uordblks;   // the heap in use (libnx reserves the whole heap up front)
}

unsigned long long ra2_switch_mem_total() {
    u64 v = 0;
    svcGetInfo(&v, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    return v;
}

void ra2_switch_rumble(float strength, int ms) {
    const Uint16 v = static_cast<Uint16>(strength * 0xFFFF);
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
        if (SDL_Joystick* j = SDL_JoystickFromInstanceID(SDL_JoystickGetDeviceInstanceID(i)))
            SDL_JoystickRumble(j, v, v, static_cast<Uint32>(ms));
}

int ra2_switch_kbd_mods() {
    // switch-sdl2 sends a keyboard's Ctrl / Shift / Alt only as key bits; the HID state's modifier
    // field is the reliable one (a USB keyboard can report a held modifier there alone).
    HidKeyboardState st;
    if (hidGetKeyboardStates(&st, 1) == 0) return 0;
    int m = 0;
    if (st.modifiers & HidKeyboardModifier_Control) m |= KMOD_LCTRL;
    if (st.modifiers & HidKeyboardModifier_Shift) m |= KMOD_LSHIFT;
    if (st.modifiers & HidKeyboardModifier_LeftAlt) m |= KMOD_LALT;
    if (st.modifiers & HidKeyboardModifier_RightAlt) m |= KMOD_RALT;
    return m;
}

int ra2_switch_mouse_wheel() {
    // switch-sdl2 has no wheel events: sum the samples' wheel deltas since the last call.
    static u64 last = 0;
    static int acc = 0;
    HidMouseState st[16];
    const size_t n = hidGetMouseStates(st, 16);   // newest first
    u64 newest = last;
    for (size_t i = 0; i < n; ++i) {
        if (st[i].sampling_number <= last) break;
        if (last != 0) acc += st[i].wheel_delta_y;   // the first call only sets the baseline
        if (st[i].sampling_number > newest) newest = st[i].sampling_number;
    }
    last = newest;
    // A notch is 120 on wheels that report WHEEL_DELTA units, 1 on the rest.
    const int unit = (acc >= 120 || acc <= -120) ? 120 : 1;
    const int notches = acc / unit;
    acc -= notches * unit;
    return notches;
}

namespace {
// switch.ini: "UIScale=1.25" style lines (any section); read once.
struct UiScales { bool read = false; double all = 0, handheld = 0, docked = 0; };
UiScales g_ui;
void read_switch_ini() {
    if (g_ui.read) return;
    g_ui.read = true;
    FILE* f = std::fopen(RA2_SWITCH_BASE_DIR "switch.ini", "r");
    if (!f) return;
    char line[256];
    while (std::fgets(line, sizeof line, f)) {
        char key[64];
        double v = 0;
        if (std::sscanf(line, " %63[A-Za-z] = %lf", key, &v) != 2) continue;
        if (!strcasecmp(key, "UIScale")) g_ui.all = v;
        else if (!strcasecmp(key, "UIScaleHandheld")) g_ui.handheld = v;
        else if (!strcasecmp(key, "UIScaleDocked")) g_ui.docked = v;
    }
    std::fclose(f);
    std::fprintf(stderr, "[switch] switch.ini UIScale=%g handheld=%g docked=%g\n", g_ui.all, g_ui.handheld,
                 g_ui.docked);
}
}  // namespace

namespace {
int g_scroll_rate = 3;
}
void ra2_switch_set_scroll_rate(int rate) { g_scroll_rate = rate; }
int ra2_switch_scroll_rate() { return g_scroll_rate; }

int ra2_switch_game_height() {
    read_switch_ini();
    const bool docked = appletGetOperationMode() == AppletOperationMode_Console;
    double s = docked ? g_ui.docked : g_ui.handheld;
    if (s <= 0) s = g_ui.all;
    if (s <= 0) s = 1.0;
    s = std::max(1.0, std::min(1.5, s));
    return static_cast<int>(std::lround(720.0 / s)) & ~1;
}

void ra2_switch_pin_thread(int core) {
    svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, 1u << core);
}

void ra2_switch_parallel(const std::vector<std::function<void()>>& jobs, const std::vector<int>& worker_cores) {
    std::atomic<std::size_t> next{0};
    auto run = [&]() {
        for (std::size_t i; (i = next.fetch_add(1)) < jobs.size();) jobs[i]();
    };
    std::vector<std::thread> workers;
    for (std::size_t k = 0; k < worker_cores.size() && k + 1 < jobs.size(); ++k)
        workers.emplace_back([&run, core = worker_cores[k]]() {
            ra2_switch_pin_thread(core);
            run();
        });
    run();
    for (std::thread& t : workers) t.join();
}

char* ra2_switch_pref_path() {
    ::mkdir(RA2_SWITCH_BASE_DIR, 0777);
    ::mkdir(kPrefDir, 0777);
    return SDL_strdup(kPrefDir);
}

// libnx calls these around main(). Sockets back LAN / online play (UDP
// broadcast + lockstep); nifm reports the local IPv4 + netmask for LAN
// discovery (engine/mp/mp_socket.cpp).
extern "C" void userAppInit(void) {
    ::mkdir(RA2_SWITCH_BASE_DIR, 0777);
    ::mkdir(kPrefDir, 0777);
    socketInitializeDefault();
    // No console on hardware: SDL_Log and the engine's stderr lines go back to
    // the PC when nxlink sent the .nro (`nxlink -s`), else to
    // save/ra2engine.log (truncated each launch) for bug reports.
    if (__nxlink_host.s_addr != 0 && nxlinkStdio() >= 0) {
        std::setvbuf(stdout, nullptr, _IOLBF, 0);
        std::setvbuf(stderr, nullptr, _IONBF, 0);
        setenv("RA2_PAD_DEBUG", "1", 1);   // rig runs (switch/rig): log controller input
    } else if (std::freopen(kLogFile, "w", stderr)) {
        std::setvbuf(stderr, nullptr, _IOLBF, 0);
        std::freopen(kLogFile, "a", stdout);
        std::setvbuf(stdout, nullptr, _IOLBF, 0);
    }
    nifmInitialize(NifmServiceType_User);
    romfsInit();   // romfs:/fonts (switch/switch_text.cpp)
    // HOME -> Close must not kill the process mid-frame: a Mesa 26 (NVK/zink)
    // app killed that way left the GPU wedged (black screen, home menu
    // included, until a reboot). With exit locked the close arrives as an
    // exit request -> appletMainLoop() false -> SDL_QUIT -> a normal shutdown.
    appletLockExit();
    // Mesa 26 (nvc0): with glthread on, a skirmish crashed after ~1-2 min in
    // _mesa_glthread_upload (null deref) right after "durable GPU submission
    // error latched: -22 ... bufctx". SDL's renderer gains nothing from it.
    setenv("mesa_glthread", "false", 1);
    // Applet mode (album launch) leaves the homebrew a few hundred MB; the
    // engine streams ~1 GB of .mix archives and wants title takeover.
    std::fprintf(stderr, "[switch] %s mode\n",
                 appletGetOperationMode() == AppletOperationMode_Console ? "docked" : "handheld");
    const AppletType at = appletGetAppletType();
    if (at != AppletType_Application && at != AppletType_SystemApplication)
        std::fprintf(stderr, "[switch] WARNING: running in applet mode (limited memory); "
                             "launch via title takeover (hold R on a game)\n");
}

extern "C" void userAppExit(void) {
    romfsExit();
    nifmExit();
    socketExit();
    appletUnlockExit();
}

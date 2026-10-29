// Nintendo Switch platform glue (libnx services, SD-card paths, log file).
#include "switch_platform.hpp"

#include <SDL.h>
#include <arpa/inet.h>   // struct in_addr for __nxlink_host
#include <switch.h>

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

namespace {
constexpr const char* kPrefDir = RA2_SWITCH_BASE_DIR "save/";
constexpr const char* kLogFile = RA2_SWITCH_BASE_DIR "save/ra2engine.log";
}  // namespace

char* ra2_switch_base_path() { return SDL_strdup(RA2_SWITCH_BASE_DIR); }

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
    const AppletType at = appletGetAppletType();
    if (at != AppletType_Application && at != AppletType_SystemApplication)
        std::fprintf(stderr, "[switch] WARNING: running in applet mode (limited memory); "
                             "launch via title takeover (hold R on a game)\n");
}

extern "C" void userAppExit(void) {
    nifmExit();
    socketExit();
    appletUnlockExit();
}

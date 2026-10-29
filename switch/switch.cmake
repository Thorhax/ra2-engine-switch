# Nintendo Switch (libnx / devkitA64) build of the engine, included from the
# top-level CMakeLists.txt when the devkitPro Switch toolchain is in use.
# Builds ra2engine.nro only: the desktop tests and tools are skipped.
#
#   cmake -B build-switch -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
#     -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-switch
#
# switch/build.sh wraps this in the ra2-switch-dkp Docker image.

# SDL2 comes from devkitPro's switch-sdl2 (2.28.x), not the pinned desktop
# tarball: the Switch video/audio/joystick backends live in that fork.
set(RA2_SWITCH_PORTLIBS "${DEVKITPRO}/portlibs/switch")

add_library(ra2engine_obj OBJECT ${RA2_ENGINE_SOURCES} engine/audio_sdl.cpp
    switch/switch_platform.cpp switch/switch_gamepad.cpp)
target_compile_definitions(ra2engine_obj PRIVATE RA2_VERSION="${RA2_VERSION}")
# Q3: no FMA contraction, so float paths match the desktop / Android builds.
target_compile_options(ra2engine_obj PRIVATE -ffp-contract=off -Wall -Wextra)
target_include_directories(ra2engine_obj PRIVATE include engine switch ${RA2_GENERATED_DIR})
target_include_directories(ra2engine_obj SYSTEM PRIVATE third_party
    "${RA2_SWITCH_PORTLIBS}/include/SDL2" "${RA2_SWITCH_PORTLIBS}/include")
target_compile_options(ra2assets PRIVATE -ffp-contract=off)
# newlib only declares POSIX extras (fseeko/ftello, ...) with _GNU_SOURCE;
# glibc's g++ defines it implicitly, devkitA64's does not.
foreach (_t ra2assets ra2engine_obj)
  target_compile_definitions(${_t} PRIVATE _GNU_SOURCE)
endforeach()

add_executable(ra2engine $<TARGET_OBJECTS:ra2engine_obj>)
target_link_directories(ra2engine PRIVATE "${RA2_SWITCH_PORTLIBS}/lib")

# Mesa: the unified Mesa 26 SDK (NVK + zink) ships static archives whose .pc
# files omit the NVK/NAK/NIL internals (and the Rust runtime bundled in
# libnak_rs/libnil), so list them in a group. The stock switch-mesa 20.1 only
# needs EGL/GLESv2/glapi/drm_nouveau.
if (EXISTS "${RA2_SWITCH_PORTLIBS}/lib/libnvk.a")
  message(STATUS "Switch: linking the unified Mesa SDK (NVK/zink)")
  set(RA2_SWITCH_GL_LIBS
      -Wl,--start-group
      EGL GLESv2 glapi vulkan nvk nak nak_rs nil libnil_format_table nir vtn
      compiler compiler_c_helpers nouveau_ws nouveau_horizon nouveau_mme
      nvidia_headers_c mesa_util mesa_util_c11 mesa_util_simd blake3 xmlconfig
      expat zstd z
      -Wl,--end-group)
else()
  message(STATUS "Switch: linking stock switch-mesa")
  set(RA2_SWITCH_GL_LIBS EGL GLESv2 glapi)
endif()
target_link_libraries(ra2engine PRIVATE ra2assets SDL2 ${RA2_SWITCH_GL_LIBS}
    drm_nouveau stdc++ nx m pthread)

if (NOT RA2_VERSION_BASE)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" RA2_VERSION_BASE)
  string(STRIP "${RA2_VERSION_BASE}" RA2_VERSION_BASE)
endif()
nx_generate_nacp(ra2engine.nacp
    NAME "RA2 Engine"
    AUTHOR "RA2 Engine contributors (Switch port)"
    VERSION "${RA2_VERSION_BASE}")
set(_ra2_nro_icon)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/switch/icon.jpg")
  set(_ra2_nro_icon ICON "${CMAKE_CURRENT_SOURCE_DIR}/switch/icon.jpg")
endif()
nx_create_nro(ra2engine NACP ra2engine.nacp ${_ra2_nro_icon})

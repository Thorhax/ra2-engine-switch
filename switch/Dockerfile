# Switch build environment: devkitA64 + the unified Mesa 26 (NVK/zink) overlay.
# Build context is the folder holding the Mesa SDK zip:
#   docker build -t ra2-switch-dkp -f switch/Dockerfile /path/to/mesa-drivers
FROM devkitpro/devkita64:latest
ARG MESA_ZIP=mesa-26.2.4-switch-unified-horizon-sdk.zip
COPY ${MESA_ZIP} /tmp/mesa.zip
RUN apt-get update -qq && apt-get install -y -qq unzip ninja-build >/dev/null \
 && unzip -oq /tmp/mesa.zip -d / && rm /tmp/mesa.zip
# vulkan.pc lists libelf in Requires.private, but no Mesa archive references an
# elf_* symbol and devkitPro ships no switch-libelf: satisfy pkg-config only.
RUN printf 'Name: libelf\nDescription: stub (unused by Mesa on Horizon)\nVersion: 0.8.13\nLibs:\nCflags:\n' \
      > /opt/devkitpro/portlibs/switch/lib/pkgconfig/libelf.pc
ENV PATH=/opt/devkitpro/devkitA64/bin:/opt/devkitpro/portlibs/switch/bin:/opt/devkitpro/tools/bin:$PATH

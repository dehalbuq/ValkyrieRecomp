# ARM64 native Debian 13 userspace aligned with dArkOSRE metadata.
# Only public bootstrap code; no game APK/ELFs/assets or proprietary Mali drivers.
FROM debian:trixie-slim
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq && apt-get install -y --no-install-recommends \
 bash ca-certificates cmake ninja-build gcc g++ binutils pkg-config \
 libsdl2-dev libegl-dev libgles-dev libgles1 libgl1-mesa-dri \
 libasound2-dev default-jdk-headless python3-minimal coreutils time file \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /work
COPY . /work/lenneth-r36s/
RUN cmake -S /work/lenneth-r36s -B /work/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build /work/build -j 4 \
 && readelf -h /work/build/lenneth-probe | grep -q 'Machine:.*AArch64'
ENV SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=offscreen \
 EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 \
 GALLIUM_DRIVER=llvmpipe XDG_CACHE_HOME=/tmp
ENTRYPOINT ["/bin/bash", "/work/lenneth-r36s/cloud/run-tests.sh"]

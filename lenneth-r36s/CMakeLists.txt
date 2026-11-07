cmake_minimum_required(VERSION 3.20)
project(LennethR36SBootstrap LANGUAGES C ASM)

# Native hardware / ABI probe only, not a working game port yet.
# Never put commercial APK assets or libraries into public CI artifacts.
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    message(WARNING "Lenneth probe intended for R36S AArch64; detected ${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
find_library(GLESV2_LIB NAMES GLESv2 REQUIRED)
find_package(Threads REQUIRED)

add_executable(lenneth-probe src/host.c src/elf_audit.c src/elf_map_preflight.c src/elf_link_preflight.c src/portable_provider.c src/asset_provider.c src/android_log_provider.c src/bionic_threads.c src/bionic_fortify.c src/cxx_runtime.c src/posix_fd_provider.c src/unicode_provider.c src/bionic_data_provider.c src/ndk_dloader.c src/saveops_provider.c src/bionic_stdio.c src/opensl_bridge.c src/pcm_sdl_probe.c src/compat_phase17.c src/bionic_locale18.c src/compat_phase19.c src/compat_phase20.c src/bionic_linker21.c src/compat_phase22.c src/compat_phase23.c src/compat_phase23_arm64.S)
target_link_libraries(lenneth-probe PRIVATE PkgConfig::SDL2 "${GLESV2_LIB}" "${CMAKE_DL_LIBS}" Threads::Threads)
target_compile_options(lenneth-probe PRIVATE
    -Wall -Wextra -Wpedantic -O2 -fno-strict-aliasing)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$" AND
   CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
    target_compile_options(lenneth-probe PRIVATE -mcpu=cortex-a35)
endif()

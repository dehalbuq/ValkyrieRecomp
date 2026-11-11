cmake_minimum_required(VERSION 3.20)
project(vplnx_linux_platform C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(GLES2 REQUIRED IMPORTED_TARGET glesv2)
pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl)

add_library(vplnx-linux-platform STATIC src/platform_linux.c)
target_include_directories(vplnx-linux-platform PUBLIC src)
target_compile_definitions(vplnx-linux-platform PRIVATE _POSIX_C_SOURCE=200809L)
target_compile_options(vplnx-linux-platform PRIVATE -Wall -Wextra -Werror)
target_link_libraries(vplnx-linux-platform PUBLIC
    PkgConfig::SDL2 PkgConfig::GLES2 PkgConfig::EGL)

add_executable(vplnx-linux-platform-check src/platform_smoke.c)
target_compile_options(vplnx-linux-platform-check PRIVATE -Wall -Wextra -Werror)
target_link_libraries(vplnx-linux-platform-check PRIVATE vplnx-linux-platform)


# Android AArch64 ELF interoperability layer, derived from vpl_nx requirements.
# No game binaries are downloaded or linked into these public build targets.
add_library(vplnx-linux-elf STATIC src/elf_linux.c)
target_include_directories(vplnx-linux-elf PUBLIC src)
target_compile_options(vplnx-linux-elf PRIVATE -Wall -Wextra -Werror)
add_executable(vplnx-linux-elf-check src/elf_check.c)
target_compile_options(vplnx-linux-elf-check PRIVATE -Wall -Wextra -Werror)
target_link_libraries(vplnx-linux-elf-check PRIVATE vplnx-linux-elf)


# NDK AAsset* API backed by sandboxed 64-bit Linux streaming.
add_library(vplnx-linux-assets STATIC src/assets_linux.c)
target_include_directories(vplnx-linux-assets PUBLIC src)
target_compile_options(vplnx-linux-assets PRIVATE -Wall -Wextra -Werror)
add_executable(vplnx-linux-assets-check src/assets_check.c)
target_compile_options(vplnx-linux-assets-check PRIVATE -Wall -Wextra -Werror)
target_link_libraries(vplnx-linux-assets-check PRIVATE vplnx-linux-assets)


# Real MIT vpl_nx OpenSL ES 1.0.1 / SDL2 implementation, with Linux config shim.
# Independent fixture sends silence only and does not touch game binaries.
add_library(vplnx-linux-opensles STATIC
    upstream/opensles.c upstream/linux_audio_compat.c)
target_include_directories(vplnx-linux-opensles PUBLIC upstream)
target_compile_options(vplnx-linux-opensles PRIVATE -Wall -Wextra)
target_link_libraries(vplnx-linux-opensles PUBLIC PkgConfig::SDL2 m)
add_executable(vplnx-linux-audio-check src/audio_check.c)
target_compile_options(vplnx-linux-audio-check PRIVATE -Wall -Wextra -Werror)
target_link_libraries(vplnx-linux-audio-check PRIVATE vplnx-linux-opensles)


# MIT vpl_nx fake JNI tables, adapted for Linux pthread and existing AAsset shim.
# Stage-1 JNI services only; no game or untrusted APK execution.
find_package(Threads REQUIRED)
add_library(vplnx-linux-jni STATIC upstream/jni_fake_linux.c)
target_include_directories(vplnx-linux-jni PUBLIC upstream src)
target_compile_definitions(vplnx-linux-jni PRIVATE _GNU_SOURCE=1)
target_compile_options(vplnx-linux-jni PRIVATE -Wall -Wextra)
target_link_libraries(vplnx-linux-jni PUBLIC
    vplnx-linux-assets vplnx-linux-opensles Threads::Threads)
add_executable(vplnx-linux-jni-check src/jni_check.c)
target_compile_options(vplnx-linux-jni-check PRIVATE -Wall -Wextra -Werror)
target_link_libraries(vplnx-linux-jni-check PRIVATE vplnx-linux-jni)


# Explicit allowlisted Bionic/NDK/GLES2 imports for the AArch64 guest ELF.
# Never forward Bionic FILE*, pthread object layouts or unsupported JNI calls.
# Android ARM64 pthread opaque-object bridge backed by Linux pthreads.
add_library(vplnx-bionic-pthread STATIC src/bionic_pthread_linux.c)
target_include_directories(vplnx-bionic-pthread PUBLIC src)
target_compile_options(vplnx-bionic-pthread PRIVATE -Wall -Wextra -Werror)
target_link_libraries(vplnx-bionic-pthread PUBLIC Threads::Threads)
add_executable(vplnx-bionic-pthread-check src/bionic_pthread_check.c)
target_compile_options(vplnx-bionic-pthread-check PRIVATE -Wall -Wextra -Werror)
target_link_libraries(vplnx-bionic-pthread-check PRIVATE vplnx-bionic-pthread)

# Android/Bionic FORTIFY wrappers validate buffer boundaries before use.
add_library(vplnx-bionic-fortify STATIC src/bionic_fortify_linux.c)
target_include_directories(vplnx-bionic-fortify PUBLIC src)
target_compile_options(vplnx-bionic-fortify PRIVATE -Wall -Wextra -Werror)
add_executable(vplnx-bionic-fortify-check src/bionic_fortify_check.c)
target_compile_options(vplnx-bionic-fortify-check PRIVATE -Wall -Wextra -Werror)
target_link_libraries(vplnx-bionic-fortify-check PRIVATE vplnx-bionic-fortify)

add_library(vplnx-linux-host STATIC src/host_imports_linux.c)
target_include_directories(vplnx-linux-host PUBLIC src)
target_compile_options(vplnx-linux-host PRIVATE -Wall -Wextra -Werror)
target_link_libraries(vplnx-linux-host PUBLIC
    vplnx-linux-assets vplnx-linux-opensles vplnx-bionic-pthread vplnx-bionic-fortify
    PkgConfig::GLES2 PkgConfig::EGL m)
add_executable(vplnx-linux-host-check src/host_check.c)
target_compile_options(vplnx-linux-host-check PRIVATE -Wall -Wextra -Werror)
target_link_libraries(vplnx-linux-host-check PRIVATE
    vplnx-linux-elf vplnx-linux-host)

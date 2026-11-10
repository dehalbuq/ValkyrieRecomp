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

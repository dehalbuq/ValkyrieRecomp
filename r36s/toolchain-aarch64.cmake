# Experimental R36S/DarkOSRE AArch64 toolchain.
# Point R36S_SYSROOT to a *development* sysroot matching the target OS/ABI.
# The sysroot must include target SDL2 headers/libs/pkg-config files.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(NOT DEFINED ENV{R36S_SYSROOT} OR "$ENV{R36S_SYSROOT}" STREQUAL "")
    message(FATAL_ERROR
        "Export R36S_SYSROOT=/absolute/path/to/your/R36S/aarch64/sysroot")
endif()
set(CMAKE_SYSROOT "$ENV{R36S_SYSROOT}")
if(NOT IS_ABSOLUTE "${CMAKE_SYSROOT}" OR NOT IS_DIRECTORY "${CMAKE_SYSROOT}")
    message(FATAL_ERROR "R36S_SYSROOT must be an existing absolute directory")
endif()

if(DEFINED ENV{R36S_CC} AND NOT "$ENV{R36S_CC}" STREQUAL "")
    set(CMAKE_C_COMPILER "$ENV{R36S_CC}")
else()
    set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
endif()
if(DEFINED ENV{R36S_CXX} AND NOT "$ENV{R36S_CXX}" STREQUAL "")
    set(CMAKE_CXX_COMPILER "$ENV{R36S_CXX}")
else()
    set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
endif()

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
if(NOT DEFINED ENV{PKG_CONFIG_LIBDIR} OR "$ENV{PKG_CONFIG_LIBDIR}" STREQUAL "")
    set(ENV{PKG_CONFIG_LIBDIR}
        "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
endif()

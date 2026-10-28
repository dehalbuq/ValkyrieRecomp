cmake_minimum_required(VERSION 3.20)
set(PSXRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp" CACHE PATH
    "Path to psxrecomp")
if(NOT EXISTS "${PSXRECOMP_ROOT}/runtime/runtime.cmake")
    message(FATAL_ERROR
        "PSXRecomp runtime is missing: ${PSXRECOMP_ROOT}/runtime/runtime.cmake\n"
        "This project expects the psxrecomp framework checkout at ${PSXRECOMP_ROOT}.\n"
        "Run: git submodule update --init --recursive\n"
        "If psxrecomp/ is not a submodule, set -DPSXRECOMP_ROOT=<path-to-psxrecomp>.")
endif()

project(ValkyrieRecomp LANGUAGES C CXX)

# --- Opt-in product surfaces (OFF by default in the framework) -------------
# Scaffold prompts / flags fill these. Without recomp-ui, PSX_RECOMP_UI is OFF.
# Setup-host CI requires WIZARD_CMAKE_BLOCK / ENABLE_SETUP_WIZARD (FORCE_SETUP_HOST
# alone does not open first-run / Generate & rebuild).
# recomp-ui submodule present (PSX_RECOMP_UI defaults ON).
# Full netplay UI -- enable after adding recomp-ui + testing:
# if(EXISTS "${PSXRECOMP_ROOT}/lib/recomp-net/CMakeLists.txt")
#     set(PSX_NETPLAY ON CACHE BOOL "..." FORCE)
# endif()
# R36S is an opt-in, lean ARM64 target. No desktop launcher is required:
# the handheld starts the game via an external PortMaster-style shell script.
# This profile does NOT claim an OpenGL ES game-renderer implementation.
option(VALKYRIE_R36S "Build the experimental R36S/AArch64 profile" OFF)
if(VALKYRIE_R36S)
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        message(FATAL_ERROR
            "VALKYRIE_R36S requires an AArch64 target toolchain/sysroot "
            "(CMAKE_SYSTEM_PROCESSOR must be aarch64/arm64).")
    endif()
    set(PSX_SDL_BACKEND SDL2 CACHE STRING "SDL backend" FORCE)
    set(PSX_RECOMP_UI OFF CACHE BOOL "Disable desktop launcher on R36S" FORCE)
    set(PSX_ENABLE_VULKAN OFF CACHE BOOL "Disable Vulkan on R36S" FORCE)
    set(PSX_REWIND OFF CACHE BOOL "Disable optional rewind snap-ring to save memory" FORCE)
    set(PSX_ZLIB_FETCH OFF CACHE BOOL "Require target zlib in sysroot (no fallback download)" FORCE)
    set(PSX_SETUP_WIZARD OFF CACHE BOOL "No first-run desktop wizard" FORCE)
    # Never emit a setup-only host and accidentally distribute it as the game.
    set(PSXRECOMP_REQUIRE_GAME_C ON CACHE BOOL
        "Require generated game sources for the handheld build" FORCE)
else()
    # Preserve the existing PC build/first-run setup behavior.
    set(PSX_SETUP_WIZARD ON CACHE BOOL
        "Advertise first-run setup wizard + Generate & rebuild in recomp-ui" FORCE)
endif()
include("${PSXRECOMP_ROOT}/runtime/runtime.cmake")

if(VALKYRIE_R36S)
    # The desktop GL source includes SDL_opengl.h and desktop OpenGL 3.3 APIs,
    # which the R36S GLES/Panfrost sysroot generally does not provide.
    # Replace only the GL backend with inert symbols. The existing CPU renderer
    # and SDL2 presentation path continue to operate unmodified.
    set(_VALKYRIE_DESKTOP_GL
        "${PSXRECOMP_ROOT}/runtime/src/gpu_gl_renderer.c")
    list(FIND PSXRECOMP_RUNTIME_SOURCES "${_VALKYRIE_DESKTOP_GL}"
        _VALKYRIE_GL_SOURCE_INDEX)
    if(_VALKYRIE_GL_SOURCE_INDEX EQUAL -1)
        message(FATAL_ERROR
            "PSXRecomp changed its renderer source list; inspect before "
            "using the R36S software-only adapter.")
    endif()
    list(REMOVE_ITEM PSXRECOMP_RUNTIME_SOURCES "${_VALKYRIE_DESKTOP_GL}")
    list(APPEND PSXRECOMP_RUNTIME_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/r36s/gpu_gl_stub.c")
    message(STATUS
        "R36S: desktop OpenGL source replaced with software-only adapter")
endif()

set(_VALKYRIE_SETUP_ARGS)
if(NOT VALKYRIE_R36S)
    list(APPEND _VALKYRIE_SETUP_ARGS ENABLE_SETUP_WIZARD)
endif()

psxrecomp_add_game_runtime(psx-runtime
    # ENABLE_NETPLAY_IF_PRESENT
    # NETPLAY_LOBBY_URL "ws://netplay.retcomm.net:8765"
    ${_VALKYRIE_SETUP_ARGS}
    WINDOW_TITLE "ValkyrieRecomp"
    DEFAULT_GAME_CONFIG_PATH "game.toml"
    MAX_PLAYERS 2
    LAUNCHER_BOXART "${CMAKE_CURRENT_SOURCE_DIR}/assets/valkyrie_profile_cover.png"
    APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/psxrecomp.ico"
    GEN_MARKER "generated/SLUS_011.56_dispatch.c"
    GEN_FULL_GLOB "generated/SLUS_011.56_full_*.c"
    CODEGEN_SETUP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/codegen_setup.c"
)

if(VALKYRIE_R36S)
    # The PortMaster launcher expects the name ValkyrieRecomp, not psx-runtime.
    # Leave the normal desktop build's output name unchanged.
    set_target_properties(psx-runtime PROPERTIES OUTPUT_NAME "ValkyrieRecomp")
endif()

# Cortex-A35 tuning without -ffast-math or unsafe emulator arithmetic.
if(VALKYRIE_R36S AND
   CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$" AND
   CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    target_compile_options(psx-runtime PRIVATE -mcpu=cortex-a35)
endif()

# Stage runtime configs next to the exe for direct build/ launches. Only copy
# files that exist — never fail POST_BUILD when game_options.toml is absent.
foreach(_PSX_RUNTIME_CONFIG game.toml game_options.toml)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_PSX_RUNTIME_CONFIG}")
        add_custom_command(TARGET psx-runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/${_PSX_RUNTIME_CONFIG}"
                "$<TARGET_FILE_DIR:psx-runtime>/${_PSX_RUNTIME_CONFIG}"
            COMMENT "Staging ${_PSX_RUNTIME_CONFIG}"
            VERBATIM)
    endif()
endforeach()

# Stage the custom cover used by the launcher runtime
# (psxrecomp/runtime/src/main.cpp -> assets/valkyrie_profile_cover.png).
set(_VALKYRIE_COVER "${CMAKE_CURRENT_SOURCE_DIR}/assets/valkyrie_profile_cover.png")
if(EXISTS "${_VALKYRIE_COVER}")
    add_custom_command(TARGET psx-runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:psx-runtime>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_VALKYRIE_COVER}"
            "$<TARGET_FILE_DIR:psx-runtime>/assets/valkyrie_profile_cover.png"
        COMMENT "Staging Valkyrie Profile cover"
        VERBATIM)
endif()

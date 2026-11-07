#!/usr/bin/env python3
"""Idempotent, pinned-runtime-only SDL2 handheld hooks.

Do not patch game data. Fail on any upstream source change instead of
silently inserting hooks at an unsafe position. Kept separate from PC builds.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
target = root / "psxrecomp/runtime/src/main.cpp"
source = target.read_text(encoding="utf-8-sig")

def replace_once(old: str, new: str, label: str) -> None:
    global source
    if new in source:
        print(f"[R36S] {label}: already applied")
        return
    count = source.count(old)
    if count != 1:
        raise SystemExit(f"[R36S] {label}: expected exactly one source anchor, found {count}")
    source = source.replace(old, new, 1)
    print(f"[R36S] {label}: applied")

replace_once(
    '#include "psx_window_icon.h"\n',
    '#include "psx_window_icon.h"\n#include "r36s_exit_menu.h"\n',
    "exit menu header",
)
replace_once(
    '    refresh_player_devices();  /* open SDL handles to match the player config */',
    '    r36s_install_fallback_gamepad_maps();\n'
    '    refresh_player_devices();  /* open SDL handles to match the player config */',
    "R36S SDL controller mapping",
)
replace_once(
    '        sdl_audio_device = psx_sdl_audio_open(&want, &have);\n'
    '        if (sdl_audio_device) {',
    '        sdl_audio_device = psx_sdl_audio_open(&want, &have);\n'
    '        std::fprintf(stdout, "[R36S audio] driver=%s desired=%d Hz legacy=%d device=%u %s\\n",\n'
    '                     SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "(none)",\n'
    '                     (int)want.freq, legacy ? 1 : 0, (unsigned)sdl_audio_device,\n'
    '                     sdl_audio_device ? "opened" : SDL_GetError());\n'
    '        if (sdl_audio_device) {',
    "R36S SDL audio diagnostics",
)
replace_once(
    '    }\n    /* Always register: SPU advance is guest-cycle budgeted and must not\n',
    '    } else {\n'
    '        std::fprintf(stderr, "[R36S audio] SDL_InitSubSystem(AUDIO) failed: %s\\n",\n'
    '                     SDL_GetError());\n'
    '    }\n'
    '    /* Always register: SPU advance is guest-cycle budgeted and must not\n',
    "R36S SDL audio init failure",
)
replace_once(
    '        savestate_menu_poll_toggle_buttons();\n'
    '        rewind_poll_toggle_buttons();',
    '        r36s_exit_menu_tick();\n'
    '        if (r36s_exit_menu_quit_requested()) {\n'
    '            psx_crash_trace_set_exit_origin("r36s_start_select_exit");\n'
    '            shutdown_runtime();\n'
    '            std::exit(0);\n'
    '        }\n'
    '        savestate_menu_poll_toggle_buttons();\n'
    '        rewind_poll_toggle_buttons();',
    "R36S exit menu input",
)
replace_once(
    '    SDL_RenderCopy(sdl_renderer, sdl_texture, &src, &dst);\n'
    '    host_osd_draw_sdl(sdl_renderer);',
    '    SDL_RenderCopy(sdl_renderer, sdl_texture, &src, &dst);\n'
    '    host_osd_draw_sdl(sdl_renderer);\n'
    '    r36s_exit_menu_draw(sdl_renderer, g_logical_w, 480 * tex_scale);',
    "R36S exit confirmation overlay",
)
replace_once(
    '    if (!sdl_renderer) {\n'
    '        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\\n", SDL_GetError());\n'
    '        return 1;\n'
    '    }\n\n'
    '    /* Present in a logical space',
    '    if (!sdl_renderer) {\n'
    '        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\\n", SDL_GetError());\n'
    '        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");\n'
    '        sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_SOFTWARE);\n'
    '        std::fprintf(stderr, "[R36S video] GLES2 failed; software SDL fallback %s\\n",\n'
    '                     sdl_renderer ? "available" : SDL_GetError());\n'
    '        if (!sdl_renderer) return 1;\n'
    '    }\n'
    '    SDL_RendererInfo rinfo;\n'
    '    SDL_zero(rinfo);\n'
    '    if (SDL_GetRendererInfo(sdl_renderer, &rinfo) == 0)\n'
    '        std::fprintf(stdout, "[R36S video] SDL driver=%s renderer=%s accelerated=%d flags=0x%x\\n",\n'
    '                     SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "(none)",\n'
    '                     rinfo.name ? rinfo.name : "(unnamed)",\n'
    '                     (rinfo.flags & SDL_RENDERER_ACCELERATED) != 0, (unsigned)rinfo.flags);\n\n'
    '    /* Present in a logical space',
    "R36S hardware SDL renderer diagnostics/fallback",
)

target.write_text(source, encoding="utf-8-sig")

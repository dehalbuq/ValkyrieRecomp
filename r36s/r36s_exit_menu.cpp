/* R36S / dArkOSRE SDL2 exit confirmation.
 * Hold Start+Select for 2.5 seconds, then A=exit or B=resume.
 * The fallback accepts common raw joystick button indices if SDL's
 * controller mapping isn't available. Optional override:
 * VALKYRIE_START_BUTTON / VALKYRIE_SELECT_BUTTON (zero-based raw indices).
 */
#include "r36s_exit_menu.h"
#include <chrono>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const auto s_boot_clock_start = std::chrono::steady_clock::now();
static int s_boot_first_frame = 0;
static int s_boot_first_nonblack = 0;

void r36s_boot_profile_mark(const char *stage) {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - s_boot_clock_start).count();
    fprintf(stdout, "[R36S boot] stage=%s elapsed_ms=%lld\n",
            stage ? stage : "(null)", (long long)elapsed);
    fflush(stdout);
}

void r36s_boot_present_probe(const uint32_t *pixels, int width, int height) {
    if (!s_boot_first_frame) {
        s_boot_first_frame = 1;
        r36s_boot_profile_mark("first-frame-buffer");
    }
    if (s_boot_first_nonblack || !pixels || width < 1 || height < 1 ||
        width > 4096 || height > 4096) return;
    const size_t count = (size_t)width * (size_t)height;
    /* Sample ~1K positions, then a full scan only if the samples are black.
     * This code runs until the first visible pixel and never touches VRAM. */
    for (size_t i = 0; i < count; i += (count / 1024u) + 1u) {
        if ((pixels[i] & 0x00FFFFFFu) != 0u) {
            s_boot_first_nonblack = 1;
            r36s_boot_profile_mark("first-nonblack-frame");
            return;
        }
    }
    /* Tiny image details could miss the sparse probe: sample every pixel
     * only after 500ms and just once a second in the all-black phase. */
    static Uint32 last_full_scan = 0;
    const Uint32 now = SDL_GetTicks();
    if (now - last_full_scan < 1000) return;
    last_full_scan = now;
    for (size_t i = 0; i < count; ++i) {
        if ((pixels[i] & 0x00FFFFFFu) != 0u) {
            s_boot_first_nonblack = 1;
            r36s_boot_profile_mark("first-nonblack-frame");
            return;
        }
    }
}

static SDL_Joystick *s_joy = NULL;
static SDL_GameController *s_pad = NULL;
static SDL_JoystickID s_instance = -1;
static Uint32 s_hold_from = 0;
static Uint32 s_now = 0;
static int s_menu = 0;
static int s_quit = 0;
static int s_rearm = 1;
static int s_wait_release = 0;
static int s_previous_accept = 0;
static int s_previous_cancel = 0;
static int s_previous_f10 = 0;
static int s_raw_start = -1;
static int s_raw_select = -1;
static Uint32 s_profile_start_ms = 0;
static unsigned s_profile_frames = 0;

static int read_index(const char *name) {
    const char *v = getenv(name);
    if (!v || !*v) return -1;
    char *end = NULL;
    const long n = strtol(v, &end, 10);
    return end != v && *end == '\0' && n >= 0 && n < 256 ? (int)n : -1;
}

/* The PSXRecomp release defaults P1 to keyboard and ignores raw joysticks
 * that SDL cannot classify as GameController. Honour PortMaster mappings first.
 * Only add a conservative fallback for an unmapped device with enough keys.
 * Device-specific overrides remain available via SDL_GAMECONTROLLERCONFIG. */
void r36s_install_fallback_gamepad_maps(void) {
    const char *disable = getenv("VALKYRIE_AUTOMAP");
    if (disable && disable[0] == '0') return;
    const int n = SDL_NumJoysticks();
    fprintf(stdout, "[R36S input] SDL_NumJoysticks=%d controllerconfig=%s\n",
            n, getenv("SDL_GAMECONTROLLERCONFIG") ? "provided" : "not provided");
    for (int i = 0; i < n; ++i) {
        const char *name = SDL_JoystickNameForIndex(i);
        if (SDL_IsGameController(i)) {
            fprintf(stdout, "[R36S input] mapped game controller %d: %s\n",
                    i, name ? name : "(unnamed)");
            continue;
        }
        SDL_Joystick *j = SDL_JoystickOpen(i);
        if (!j) {
            fprintf(stderr, "[R36S input] SDL_JoystickOpen(%d) failed: %s\n",
                    i, SDL_GetError());
            continue;
        }
        const int buttons = SDL_JoystickNumButtons(j);
        const int axes = SDL_JoystickNumAxes(j);
        const int hats = SDL_JoystickNumHats(j);
        fprintf(stdout, "[R36S input] unmapped raw joystick %d (%s): buttons=%d axes=%d hats=%d\n",
                i, name ? name : "(unnamed)", buttons, axes, hats);
        if (buttons >= 8 && axes >= 2) {
            const int back = buttons >= 10 ? 8 : 6;
            const int start = buttons >= 10 ? 9 : 7;
            char guid[40] = {0}, mapping[600] = {0}, dpad[140] = {0};
            SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof(guid));
            if (hats > 0) {
                snprintf(dpad, sizeof(dpad),
                         "dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,");
            } else if (buttons >= 14) {
                snprintf(dpad, sizeof(dpad),
                         "dpup:b10,dpdown:b11,dpleft:b12,dpright:b13,");
            }
            snprintf(mapping, sizeof(mapping),
                     "%s,R36S Generic Fallback,a:b0,b:b1,x:b2,y:b3,"
                     "back:b%d,start:b%d,leftshoulder:b4,rightshoulder:b5,"
                     "leftx:a0,lefty:a1,%splatform:Linux,",
                     guid, back, start, dpad);
            const int added = SDL_GameControllerAddMapping(mapping);
            fprintf(stdout, "[R36S input] fallback mapping for joystick %d => %d (%s)\n",
                    i, added, added < 0 ? SDL_GetError() : "ok");
        }
        SDL_JoystickClose(j);
    }
    fflush(stdout);
}

static void refresh_controller(void) {
    if (s_joy && SDL_JoystickGetAttached(s_joy)) return;
    if (s_pad) { SDL_GameControllerClose(s_pad); s_pad = NULL; }
    if (s_joy) { SDL_JoystickClose(s_joy); s_joy = NULL; }
    s_instance = -1;
    const int count = SDL_NumJoysticks();
    if (count <= 0) return;
    for (int i = 0; i < count; ++i) {
        SDL_Joystick *joy = SDL_JoystickOpen(i);
        if (!joy) continue;
        s_joy = joy;
        s_instance = SDL_JoystickInstanceID(s_joy);
        if (SDL_IsGameController(i))
            s_pad = SDL_GameControllerOpen(i);
        fprintf(stdout,
                "[R36S input] joystick='%s' instance=%d buttons=%d axes=%d hats=%d mapped=%d\n",
                SDL_JoystickName(s_joy) ? SDL_JoystickName(s_joy) : "(unknown)",
                (int)s_instance, SDL_JoystickNumButtons(s_joy),
                SDL_JoystickNumAxes(s_joy), SDL_JoystickNumHats(s_joy),
                s_pad != NULL);
        fflush(stdout);
        return;
    }
}

static int raw_pressed(int b) {
    return s_joy && b >= 0 && b < SDL_JoystickNumButtons(s_joy) &&
           SDL_JoystickGetButton(s_joy, b) != 0;
}

static int mapped_pressed(SDL_GameControllerButton b) {
    return s_pad && SDL_GameControllerGetButton(s_pad, b) != 0;
}

void r36s_exit_menu_tick(void) {
    static int initialised = 0;
    if (!initialised) {
        s_raw_start = read_index("VALKYRIE_START_BUTTON");
        s_raw_select = read_index("VALKYRIE_SELECT_BUTTON");
        initialised = 1;
        fprintf(stdout,
                "[R36S exit] hold START+SELECT 2500ms; A=EXIT, B=RESUME; raw overrides start=%d select=%d\n",
                s_raw_start, s_raw_select);
    }
    /* Called after SDL_PollEvent: keyboard/gamepad snapshots are up to date. */
    SDL_GameControllerUpdate();
    SDL_JoystickUpdate();
    refresh_controller();
    s_now = SDL_GetTicks();
    /* Count host-observed PSX VBlanks. This measures actual simulated frames
     * per wall second without relying on display's fixed 59Hz refresh cap. */
    if (!s_profile_start_ms) s_profile_start_ms = s_now;
    ++s_profile_frames;
    const Uint32 dt = s_now - s_profile_start_ms;
    if (dt >= 5000) {
        fprintf(stdout,
                "[R36S perf] simulated_vblanks=%u elapsed_ms=%u effective_fps=%.2f (refresh is NOT fps)\n",
                s_profile_frames, (unsigned)dt,
                (double)s_profile_frames * 1000.0 / (double)dt);
        fflush(stdout);
        s_profile_frames = 0;
        s_profile_start_ms = s_now;
    }

    const Uint8 *key = SDL_GetKeyboardState(NULL);
    const int both_keys =
        (key[SDL_SCANCODE_RETURN] && key[SDL_SCANCODE_BACKSPACE]) ||
        (key[SDL_SCANCODE_RETURN] && key[SDL_SCANCODE_ESCAPE]);
    const int mapped_both = mapped_pressed(SDL_CONTROLLER_BUTTON_START) &&
                             mapped_pressed(SDL_CONTROLLER_BUTTON_BACK);
    /* Common SDL/evdev layouts have select/start at b6/b7 or b8/b9.
     * A specific device can override them with the variables above. */
    const int raw_both = s_raw_start >= 0 && s_raw_select >= 0
        ? raw_pressed(s_raw_start) && raw_pressed(s_raw_select)
        : (raw_pressed(7) && raw_pressed(6)) ||
          (raw_pressed(9) && raw_pressed(8));
    const int combo = both_keys || mapped_both || raw_both;
    const int f10 = key[SDL_SCANCODE_F10] != 0;

    const int accept = mapped_pressed(SDL_CONTROLLER_BUTTON_A) ||
                       raw_pressed(0) || key[SDL_SCANCODE_RETURN];
    const int cancel = mapped_pressed(SDL_CONTROLLER_BUTTON_B) ||
                       raw_pressed(1) || key[SDL_SCANCODE_ESCAPE];

    if (!s_menu) {
        if (!combo) { s_hold_from = 0; s_rearm = 1; }
        else if (s_rearm) {
            if (!s_hold_from) s_hold_from = s_now ? s_now : 1;
            else if (s_now - s_hold_from >= 2500) {
                s_menu = 1;
                s_rearm = 0;
                s_wait_release = 1;
                fprintf(stdout, "[R36S exit] menu opened by START+SELECT\n");
            }
        }
        if (f10 && !s_previous_f10) {
            s_menu = 1;
            s_rearm = 0;
            s_wait_release = 1;
            fprintf(stdout, "[R36S exit] menu opened by F10\n");
        }
    } else {
        if (s_wait_release) {
            if (!combo && !f10 && !accept && !cancel)
                s_wait_release = 0;
        } else if (accept && !s_previous_accept) {
            s_quit = 1;
            fprintf(stdout, "[R36S exit] user confirmed exit\n");
        } else if (cancel && !s_previous_cancel) {
            s_menu = 0;
            s_rearm = 0;
            s_hold_from = 0;
            fprintf(stdout, "[R36S exit] return to game\n");
        }
    }
    s_previous_accept = accept != 0;
    s_previous_cancel = cancel != 0;
    s_previous_f10 = f10 != 0;
}

int r36s_exit_menu_quit_requested(void) {
    return s_quit;
}

/* Tiny own-drawn 5x7 pixel font: no SDL_ttf/font assets needed on dArkOSRE. */
typedef struct {
    char ch;
    unsigned char row[7];
} Glyph;
static const Glyph s_font[] = {
    {' ', {0,0,0,0,0,0,0}},
    {'A', {14,17,17,31,17,17,17}},
    {'B', {30,17,17,30,17,17,30}},
    {'C', {14,17,16,16,16,17,14}},
    {'D', {30,17,17,17,17,17,30}},
    {'E', {31,16,16,30,16,16,31}},
    {'F', {31,16,16,30,16,16,16}},
    {'G', {14,17,16,23,17,17,14}},
    {'I', {31,4,4,4,4,4,31}},
    {'J', {7,2,2,2,18,18,12}},
    {'L', {16,16,16,16,16,16,31}},
    {'M', {17,27,21,21,17,17,17}},
    {'N', {17,25,21,19,17,17,17}},
    {'O', {14,17,17,17,17,17,14}},
    {'R', {30,17,17,30,20,18,17}},
    {'S', {15,16,16,14,1,1,30}},
    {'T', {31,4,4,4,4,4,4}},
    {'V', {17,17,17,17,17,10,4}},
    {'?', {14,17,1,2,4,0,4}},
    {'=', {0,0,31,0,31,0,0}},
    {'+', {0,4,4,31,4,4,0}},
    {'-', {0,0,0,31,0,0,0}},
    {'2', {14,17,1,2,4,8,31}},
    {':', {0,4,4,0,4,4,0}}
};
static void text(SDL_Renderer *r, const char *msg, int x, int y, int scale) {
    for (const char *p = msg; *p; ++p) {
        const Glyph *g = &s_font[0];
        for (unsigned i = 0; i < sizeof(s_font)/sizeof(s_font[0]); ++i)
            if (s_font[i].ch == *p) { g = &s_font[i]; break; }
        for (int yy = 0; yy < 7; ++yy)
            for (int xx = 0; xx < 5; ++xx)
                if (g->row[yy] & (1u << (4-xx))) {
                    SDL_Rect rect = {x + xx*scale, y + yy*scale, scale, scale};
                    SDL_RenderFillRect(r, &rect);
                }
        x += 6*scale;
    }
}
void r36s_exit_menu_draw(SDL_Renderer *r, int logical_w, int logical_h) {
    if (!r || logical_w < 1 || logical_h < 1) return;
    if (!s_menu && !s_boot_first_nonblack) {
        /* Keep the player informed while BIOS/kernel/game run on the CPU.
         * This overlay does not speed emulation up and disappears automatically
         * once an actual non-black framebuffer is emitted. */
        const int scale = logical_w >= 480 ? 3 : 2;
        const int title_w = 14 * 6 * scale;
        const int box_w = title_w + 10 * scale;
        SDL_Rect b = {(logical_w-box_w)/2, (logical_h - 22*scale)/2,
                      box_w, 22*scale};
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 208);
        SDL_RenderFillRect(r, &b);
        SDL_SetRenderDrawColor(r, 220, 220, 220, 255);
        SDL_RenderDrawRect(r, &b);
        text(r, "INICIANDO JOGO", b.x + 5*scale, b.y + 7*scale, scale);
        return;
    }
    if (!s_menu) return;
    int scale = logical_w >= 500 ? 3 : 2;
    const int bw = 86*scale, bh = 51*scale;
    SDL_Rect box = {(logical_w-bw)/2, (logical_h-bh)/2, bw, bh};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 220);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 225, 225, 225, 255);
    SDL_RenderDrawRect(r, &box);
    const int x = box.x + 5*scale;
    text(r, "SAIR DO JOGO?", x, box.y+5*scale, scale);
    SDL_SetRenderDrawColor(r, 150, 230, 150, 255);
    text(r, "A = SAIR", x, box.y+19*scale, scale);
    SDL_SetRenderDrawColor(r, 245, 185, 140, 255);
    text(r, "B = VOLTAR", x, box.y+31*scale, scale);
}

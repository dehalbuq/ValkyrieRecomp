#pragma once
/* R36S: in-game Start+Select exit confirmation (SDL2 only).
 * Intentionally independent of PlayStation emulated-pad routing: the exit
 * shortcut must remain usable even if the gamepad is not mapped in PSXRecomp.
 */
#include <SDL.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void r36s_install_fallback_gamepad_maps(void);
/* Timestamps are monotonic and start at the runtime's main(), not SDL_Init. */
void r36s_boot_profile_mark(const char *stage);
/* Cheap, one-time black-frame scan to distinguish engine boot from first image. */
void r36s_boot_present_probe(const uint32_t *pixels, int width, int height);
void r36s_exit_menu_tick(void);
int r36s_exit_menu_quit_requested(void);
void r36s_exit_menu_draw(SDL_Renderer *renderer, int logical_w, int logical_h);
#ifdef __cplusplus
}
#endif

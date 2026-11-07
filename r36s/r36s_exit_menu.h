#pragma once
/* R36S: in-game Start+Select exit confirmation (SDL2 only).
 * Intentionally independent of PlayStation emulated-pad routing: the exit
 * shortcut must remain usable even if the gamepad is not mapped in PSXRecomp.
 */
#include <SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
void r36s_exit_menu_tick(void);
int r36s_exit_menu_quit_requested(void);
void r36s_exit_menu_draw(SDL_Renderer *renderer, int logical_w, int logical_h);
#ifdef __cplusplus
}
#endif

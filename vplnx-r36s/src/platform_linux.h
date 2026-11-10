/* Linux AArch64 platform layer for the planned MIT vpl_nx adaptation.
 * Original glue code; not a commercial engine launcher.
 */
#ifndef VPLNX_PLATFORM_LINUX_H
#define VPLNX_PLATFORM_LINUX_H
#include <stdint.h>
#include <SDL.h>

typedef struct {
    SDL_Window *window;
    SDL_GLContext context;
    SDL_GameController *controller;
    int width;
    int height;
    int running;
    unsigned context_major;
} VplLinuxPlatform;

int vpl_linux_init(VplLinuxPlatform *p, int width, int height, int gles_major);
void vpl_linux_poll(VplLinuxPlatform *p);
void vpl_linux_swap(VplLinuxPlatform *p);
void vpl_linux_shutdown(VplLinuxPlatform *p);
uint64_t vpl_linux_ticks_ns(void);
void vpl_linux_sleep_until_ns(uint64_t deadline_ns);
#endif

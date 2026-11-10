/* SPDX-License-Identifier: MIT
 * Native Linux SDL2/KMSDRM/EGL/GLES replacement for vpl_nx Switch window/time
 * services. No libnx headers, SVC syscalls, game files or guest code execution.
 */
#define _POSIX_C_SOURCE 200809L
#include "platform_linux.h"
#include <GLES2/gl2.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void open_controller(VplLinuxPlatform *p, int index) {
    if (p->controller || !SDL_IsGameController(index))
        return;
    p->controller = SDL_GameControllerOpen(index);
    if (p->controller)
        fprintf(stderr, "[vpl-linux] gamepad connected: %s\n",
                SDL_GameControllerName(p->controller));
}

int vpl_linux_init(VplLinuxPlatform *p, int width, int height, int gles_major) {
    if (!p || width < 1 || height < 1 || width > 1920 || height > 1080 ||
        (gles_major != 2 && gles_major != 3))
        return -1;
    memset(p, 0, sizeof(*p));
    p->width = width;
    p->height = height;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[vpl-linux] SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, gles_major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    p->window = SDL_CreateWindow("Lenneth vpl_nx Linux platform",
                                 SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!p->window) {
        fprintf(stderr, "[vpl-linux] SDL window failed: %s\n", SDL_GetError());
        goto fail;
    }
    p->context = SDL_GL_CreateContext(p->window);
    if (!p->context) {
        fprintf(stderr, "[vpl-linux] ES%d context failed: %s\n",
                gles_major, SDL_GetError());
        goto fail;
    }
    if (SDL_GL_MakeCurrent(p->window, p->context) != 0) {
        fprintf(stderr, "[vpl-linux] MakeCurrent failed: %s\n", SDL_GetError());
        goto fail;
    }
    int actual_major = 0, actual_minor = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &actual_major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &actual_minor);
    if (actual_major < gles_major) {
        fprintf(stderr, "[vpl-linux] Required GLES %d unavailable (got %d.%d)\n",
                gles_major, actual_major, actual_minor);
        goto fail;
    }
    p->context_major = (unsigned)actual_major;
    SDL_GL_SetSwapInterval(1);
    const GLubyte *version = glGetString(GL_VERSION);
    const GLubyte *renderer = glGetString(GL_RENDERER);
    const GLubyte *vendor = glGetString(GL_VENDOR);
    fprintf(stderr, "[vpl-linux] SDL driver: %s\n",
            SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown");
    fprintf(stderr, "[vpl-linux] GLES: %s; GPU: %s; vendor: %s\n",
            version ? (const char *)version : "unknown",
            renderer ? (const char *)renderer : "unknown",
            vendor ? (const char *)vendor : "unknown");
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
        open_controller(p, i);
    p->running = 1;
    return 0;
fail:
    vpl_linux_shutdown(p);
    return -1;
}

void vpl_linux_poll(VplLinuxPlatform *p) {
    SDL_Event ev;
    if (!p) return;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT)
            p->running = 0;
        else if (ev.type == SDL_CONTROLLERDEVICEADDED)
            open_controller(p, ev.cdevice.which);
        else if (ev.type == SDL_CONTROLLERDEVICEREMOVED && p->controller &&
                 SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p->controller))
                    == ev.cdevice.which) {
            SDL_GameControllerClose(p->controller);
            p->controller = NULL;
        }
    }
}

void vpl_linux_swap(VplLinuxPlatform *p) {
    if (p && p->context && p->window)
        SDL_GL_SwapWindow(p->window);
}

void vpl_linux_shutdown(VplLinuxPlatform *p) {
    if (!p) return;
    if (p->controller) {
        SDL_GameControllerClose(p->controller);
        p->controller = NULL;
    }
    if (p->context) {
        SDL_GL_DeleteContext(p->context);
        p->context = NULL;
    }
    if (p->window) {
        SDL_DestroyWindow(p->window);
        p->window = NULL;
    }
    SDL_Quit();
    p->running = 0;
}

uint64_t vpl_linux_ticks_ns(void) {
    struct timespec ts = {0, 0};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

void vpl_linux_sleep_until_ns(uint64_t deadline_ns) {
    struct timespec deadline = {
        (time_t)(deadline_ns / UINT64_C(1000000000)),
        (long)(deadline_ns % UINT64_C(1000000000))
    };
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL) == EINTR) {
        /* Preserve a stable 30 Hz timebase without accelerating game logic. */
    }
}

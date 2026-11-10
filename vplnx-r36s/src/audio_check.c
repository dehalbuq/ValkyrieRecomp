/* SPDX-License-Identifier: MIT
 * Standalone SDL/OpenSL no-music / no-game PCM fixture for Linux AArch64.
 */
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include "../upstream/opensles.h"

static int fail(const char *reason) {
    fprintf(stderr, "VPLNX_AUDIO=FAIL reason=%s\n", reason);
    opensles_shutdown();
    SDL_Quit();
    return 1;
}
int main(void) {
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "VPLNX_AUDIO=FAIL SDL_Init=%s\n", SDL_GetError());
        return 1;
    }
    void *engine = NULL;
    if (slCreateEngine(&engine, 0, NULL, 0, NULL, NULL) != 0 ||
        engine == NULL || SL_IID_ENGINE == NULL)
        return fail("OpenSL engine interface not constructed");

    int rate = opensles_movie_begin(44100);
    if (rate <= 0)
        return fail("SDL audio device did not open");
    int16_t silence[256] = {0}; /* 128 stereo PCM frames, not music */
    if (opensles_movie_queue(silence, 128) != 128)
        return fail("movie queue rejected PCM");
    if (opensles_movie_samples_queued() != 128)
        return fail("movie queue sample counter invalid");
    if (opensles_movie_buffered_frames() != 128)
        return fail("paused ring-buffer value invalid");

    opensles_movie_set_paused(0);
    for (int i = 0; i < 12 &&
         opensles_movie_samples_played() < 128; ++i) {
        SDL_Delay(40);
    }
    if (opensles_movie_samples_played() != 128)
        return fail("audio callback never drained the ring buffer");
    opensles_movie_end();
    opensles_shutdown();
    SDL_Quit();
    printf("VPLNX_OPENSL=PASS device_rate=%d pcm_frames=128 callback_drained=1 commercial_engine=NO\n",
           rate);
    return 0;
}

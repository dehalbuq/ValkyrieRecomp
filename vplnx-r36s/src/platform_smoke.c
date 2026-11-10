/* Native Linux platform integration sample. NOT a game port or an emulator.
 * Can be removed once the real JNI/render entry point is connected.
 */
#include "platform_linux.h"
#include <GLES2/gl2.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int es = 2;
    if (argc == 2 && strcmp(argv[1], "--es3") == 0) {
        es = 3;
    } else if (argc != 1) {
        fprintf(stderr, "usage: vplnx-linux-platform [--es3]\n");
        return 2;
    }
    VplLinuxPlatform p;
    if (vpl_linux_init(&p, 640, 480, es) != 0)
        return 1;
    uint64_t next = vpl_linux_ticks_ns();
    for (int frame = 0; frame < 60 && p.running; frame++) {
        vpl_linux_poll(&p);
        glViewport(0, 0, p.width, p.height);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        vpl_linux_swap(&p);
        next += UINT64_C(33333333); /* 30 Hz engine nominal cadence */
        uint64_t now = vpl_linux_ticks_ns();
        if (next < now) next = now;
        vpl_linux_sleep_until_ns(next);
    }
    vpl_linux_shutdown(&p);
    puts("VPLNX_LINUX_PLATFORM=PASS commercial_engine=NOT_EXECUTED");
    return 0;
}

/* SPDX-License-Identifier: MIT */
#include "linux_audio_compat.h"
#include <stdio.h>
#include <stdarg.h>
VplAudioConfig config = { .audio_rate = 0 };
int debugPrintf(const char *format, ...) {
    int ret;
    va_list args;
    va_start(args, format);
    ret = vfprintf(stderr, format, args);
    va_end(args);
    return ret;
}

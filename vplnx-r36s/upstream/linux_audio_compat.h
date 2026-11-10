/* SPDX-License-Identifier: MIT
 * Small Linux-only compatibility shim for upstream vpl_nx's OpenSL ES bridge.
 * The original audio mixer code remains in opensles.c with attribution.
 */
#ifndef VPLNX_LINUX_AUDIO_COMPAT_H
#define VPLNX_LINUX_AUDIO_COMPAT_H
typedef struct VplAudioConfig {
    int audio_rate;
} VplAudioConfig;
extern VplAudioConfig config;
int debugPrintf(const char *format, ...);
#endif

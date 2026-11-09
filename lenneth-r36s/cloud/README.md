# Lenneth ARM64 Cloud Lab

Run `sh coletar-darkosre.sh` in a writable directory on the R36S. It creates a ZIP with filtered system, CPU, memory, GLES/SDL, ALSA and input metadata; no APK, game files, GPU firmware, credentials or network identifiers are copied. Review before sharing. No automatic upload.

In GitHub Actions, run **Lenneth ARM64 Cloud Lab (dArkOSRE profile)**, or push code to branch `port/lenneth-cloud-lab`. The existing Phase1–33 full regression workflow also runs automatically on that branch. The additional Cloud Lab job builds native AArch64, runs 29 independent synthetic ABI/JNI/Bionic checks and a mock ELF lifecycle, recording maximum RSS per check.

**Limitations:** Ubuntu native ARM64 is not dArkOSRE kernel 4.4.189, libc 2.41 or Mali-G31. Mesa llvmpipe offscreen rendering is software-only, not a GPU clone; KMSDRM, ALSA hardware, gamepad timing and FPS must still be checked on the handheld. The commercial game code and assets are never uploaded or executed in cloud CI.

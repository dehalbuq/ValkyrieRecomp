/*
 * Synthetic AArch64 bootstrap fixture only. MIT.
 * Not derived from or linked to any commercial game library.
 * Compiled with -shared -nostdlib -fPIC and no DT_NEEDED.
 */
__attribute__((visibility("default")))
const char lenneth_phase30_mock_marker[] =
    "LENNETH_PHASE30_SYNTHETIC_LIFECYCLE_ONLY_2026";

__attribute__((visibility("default")))
volatile int phase30_shared_progress = 0;

__attribute__((constructor(101)))
static void phase30_partner_ctor(void) {
    phase30_shared_progress = 10;
}

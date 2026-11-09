/* Synthetic unresolved + weak symbols for offline dependency audits only. */
extern int phase39_shared_api(void);
extern int phase39_missing_api(void);
extern int phase39_weak_api(void) __attribute__((weak));
__attribute__((visibility("default"))) int phase39_good(void){
    return phase39_shared_api() + (phase39_weak_api ? phase39_weak_api():0);
}
__attribute__((visibility("default"))) int phase39_unresolved(void){
    return phase39_missing_api();
}

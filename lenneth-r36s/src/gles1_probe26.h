#ifndef LENNETH_GLES1_PROBE26_H
#define LENNETH_GLES1_PROBE26_H
/* Read-only host GLES1 library/symbol discovery plus known ELF import plan.
 * Never calls GL functions, JNI, or commercial code. */
int lenneth_gles1_probe26(void);
int lenneth_gles1_probe26_selftest(void);
#endif

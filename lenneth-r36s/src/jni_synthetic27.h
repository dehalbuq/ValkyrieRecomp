#ifndef LENNETH_JNI_SYNTHETIC27_H
#define LENNETH_JNI_SYNTHETIC27_H
/* Synthetic JNI Invocation ABI tests only: no game constructors or JNI_OnLoad. */
int lenneth_jni_synthetic27_selftest(void);

/* Used only by verified, independently built Phase30 mock ELF fixtures. */
typedef int (*Lenneth30MockOnload)(void *,void *);
typedef void (*Lenneth30MockInit)(void *,void *,int,int);
typedef void (*Lenneth30MockRender)(void *,void *);
int lenneth_jni27_replay_mock(Lenneth30MockOnload onload,
                               Lenneth30MockInit init,
                               Lenneth30MockRender render);
#endif

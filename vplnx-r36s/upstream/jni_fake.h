/* jni_fake.h -- fake JNI environment for the Cocos2d-x 3.x engine (libMyGame.so)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef __JNI_FAKE_H__
#define __JNI_FAKE_H__

extern void *fake_vm;  // JavaVM *
extern void *fake_env; // JNIEnv *

// set when the engine asks the activity to finish (terminateProcessJNI/finish)
extern volatile int jni_quit_requested;

void jni_init(void);

// the fake Context/AppActivity instance handed to the native methods
void *jni_make_thiz(void);
void *jni_make_string(const char *utf);

// persistent primitive arrays for the input pump (nativeTouchesMove batches)
void *jni_make_input_array(int max_len, int elem_size);
void jni_input_array_set(void *arr, const void *src, int n);

#endif

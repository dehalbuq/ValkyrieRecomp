/* jni_fake.c -- fake JNI environment for the Cocos2d-x 3.x engine (libMyGame.so)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 *
 * Two directions cross the JNI boundary:
 *   - Java -> native: the renderer/helper native methods (nativeInit,
 *     nativeRender, nativeSetContext, nativeTouchesBegin, ...). main.c resolves
 *     those exported Java_org_cocos2dx_* symbols and calls them directly.
 *   - native -> Java: cocos2d::JniHelper::callStatic*Method(...) on
 *     org/cocos2dx/lib/Cocos2dxHelper (writable path, package name, language,
 *     UserDefault preferences, obb fd) and friends. We provide a functional
 *     JNIEnv so FindClass/GetStaticMethodID/CallStatic*Method resolve to the
 *     native handlers below.
 *
 * The object model, local-ref registry and env-table wiring are inherited from
 * the FF Dimensions wrapper; only the method dispatch is Cocos2d-x specific.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "jni_linux_compat.h"
#include <strings.h>

#include "jni_fake.h"
#include "assets_linux.h"

#define JNI_OK 0
#define JNI_VERSION_1_6 0x00010006

#define VPL_PACKAGE "com.crunchyroll.gv.valkyrieprofilelenneth.game"

typedef uint32_t juint; /* JNI jint is 32-bit on AArch64 */

// ---------------------------------------------------------------------------
// fake object model
// ---------------------------------------------------------------------------
enum {
  TAG_OBJECT = 0x4f424a31, // 'OBJ1'
  TAG_STRING = 0x53545231, // 'STR1'
  TAG_OBJARR = 0x4f415231, // 'OAR1'
  TAG_PRIARR = 0x50415231, // 'PAR1'
  TAG_ID     = 0x4d494431, // 'MID1'  pooled
  TAG_CLASS  = 0x434c5331, // 'CLS1'  singleton per class name
};

typedef struct { uint32_t tag; char label[96]; } FakeObject;   // also FakeClass
typedef struct { uint32_t tag; char *utf; } FakeString;
typedef struct { uint32_t tag; int len; void **items; } FakeObjArray;
typedef struct { uint32_t tag; int len; int elem_size; void *data; } FakePriArray;
typedef struct { uint32_t tag; char cls[96]; char name[96]; char sig[128]; } FakeID;

volatile int jni_quit_requested = 0;

// ---------------------------------------------------------------------------
// local reference registry
// ---------------------------------------------------------------------------
#define MAX_LOCALS 16384
#define MAX_FRAMES 64
static void *locals[MAX_LOCALS];
static int locals_top = 0;
static int frames[MAX_FRAMES];
static int frame_top = 0;
static Mutex locals_lock;

static void *reg_local(void *ref) {
  if (ref) {
    mutexLock(&locals_lock);
    if (locals_top < MAX_LOCALS) locals[locals_top++] = ref;
    else debugPrintf("JNI: local ref table full, leaking %p\n", ref);
    mutexUnlock(&locals_lock);
  }
  return ref;
}

static void free_ref(void *ref) {
  if (!ref) return;
  switch (*(uint32_t *)ref) {
    case TAG_STRING: { FakeString *s = ref; free(s->utf); free(s); break; }
    case TAG_PRIARR: { FakePriArray *a = ref; free(a->data); free(a); break; }
    case TAG_OBJARR: { FakeObjArray *a = ref; free(a->items); free(a); break; }
    case TAG_OBJECT: free(ref); break;
    default: break; // TAG_ID / TAG_CLASS never freed
  }
}

static void delete_local(void *ref) {
  if (!ref) return;
  mutexLock(&locals_lock);
  for (int i = locals_top - 1; i >= 0; i--)
    if (locals[i] == ref) { locals[i] = locals[--locals_top]; free_ref(ref); break; }
  mutexUnlock(&locals_lock);
}

// ---------------------------------------------------------------------------
// constructors
// ---------------------------------------------------------------------------
void *jni_make_string(const char *utf) {
  FakeString *s = calloc(1, sizeof(*s));
  if (!s) return NULL;
  s->tag = TAG_STRING;
  s->utf = strdup(utf ? utf : "");
  if (!s->utf) { free(s); return NULL; }
  return reg_local(s);
}

static void *make_pri_array_adopt(void *data, int len, int elem_size) {
  FakePriArray *a = calloc(1, sizeof(*a));
  if (!a) { free(data); return NULL; }
  a->tag = TAG_PRIARR; a->len = len; a->elem_size = elem_size; a->data = data;
  return reg_local(a);
}
// persistent primitive array for the input pump (NOT a JNI local ref, so it is
// never freed on frame pop). Reused every frame for nativeTouchesMove batches.
void *jni_make_input_array(int max_len, int elem_size) {
  FakePriArray *a = calloc(1, sizeof(*a));
  if (!a) return NULL;
  if (max_len <= 0 || max_len > 65536 || elem_size <= 0 || elem_size > 8) { free(a); return NULL; }
  a->tag = TAG_PRIARR; a->len = max_len; a->elem_size = elem_size;
  a->data = calloc(max_len ? max_len : 1, elem_size);
  return a;
}
// set the effective length (GetArrayLength returns this) + copy n elements.
void jni_input_array_set(void *arr, const void *src, int n) {
  FakePriArray *a = arr;
  if (!a || a->tag != TAG_PRIARR) return;
  if (n < 0 || n > 65536 || (src == NULL && n)) return;
  a->len = n;
  if (src && n > 0) memcpy(a->data, src, (size_t)n * a->elem_size);
}
static void *new_pri_array(int len, int elem_size) {
  if (len < 0 || len > 65536 || elem_size <= 0 || elem_size > 8) return NULL;
  void *data = calloc(len ? len : 1, elem_size);
  if (!data) return NULL;
  return make_pri_array_adopt(data, len, elem_size);
}
static const char *obj_str(void *jstr) {
  FakeString *s = jstr;
  return (s && s->tag == TAG_STRING) ? s->utf : "";
}

// singletons keyed by class name
#define MAX_CLASSES 64
static FakeObject class_pool[MAX_CLASSES];
static int class_count = 0;
static void *get_class(const char *name) {
  for (int i = 0; i < class_count; i++)
    if (!strcmp(class_pool[i].label, name)) return &class_pool[i];
  if (class_count >= MAX_CLASSES) return &class_pool[0];
  FakeObject *c = &class_pool[class_count++];
  c->tag = TAG_CLASS;
  snprintf(c->label, sizeof(c->label), "%s", name ? name : "?");
  return c;
}

static FakeObject *g_thiz = NULL; // the AppActivity/Context instance
void *jni_make_thiz(void) {
  if (!g_thiz) {
    g_thiz = calloc(1, sizeof(*g_thiz));
    g_thiz->tag = TAG_CLASS;
    strcpy(g_thiz->label, "AppActivity");
  }
  return g_thiz;
}

// method/field ID pool
#define MAX_IDS 512
static FakeID id_pool[MAX_IDS];
static int id_count = 0;
static FakeID *get_id(const char *cls, const char *name, const char *sig) {
  for (int i = 0; i < id_count; i++)
    if (!strcmp(id_pool[i].cls, cls ? cls : "") &&
        !strcmp(id_pool[i].name, name) && !strcmp(id_pool[i].sig, sig ? sig : "")) return &id_pool[i];
  if (id_count >= MAX_IDS) { debugPrintf("JNI: id pool exhausted (%s)\n", name); return &id_pool[0]; }
  FakeID *id = &id_pool[id_count++];
  id->tag = TAG_ID;
  snprintf(id->cls, sizeof(id->cls), "%s", cls ? cls : "");
  snprintf(id->name, sizeof(id->name), "%s", name);
  snprintf(id->sig, sizeof(id->sig), "%s", sig ? sig : "");
  return id;
}

// ---------------------------------------------------------------------------
// UserDefault (Cocos2dxHelper preferences): tiny in-memory key/value store
// ---------------------------------------------------------------------------
#define MAX_PREFS 512
typedef struct { char key[128]; char val[256]; int used; } Pref;
static Pref prefs[MAX_PREFS];
static const char *pref_get(const char *key, const char *dflt) {
  for (int i = 0; i < MAX_PREFS; i++)
    if (prefs[i].used && !strcmp(prefs[i].key, key)) return prefs[i].val;
  return dflt;
}
static void pref_set(const char *key, const char *val) {
  int free_slot = -1;
  for (int i = 0; i < MAX_PREFS; i++) {
    if (prefs[i].used && !strcmp(prefs[i].key, key)) {
      snprintf(prefs[i].val, sizeof(prefs[i].val), "%s", val); return;
    }
    if (!prefs[i].used && free_slot < 0) free_slot = i;
  }
  if (free_slot >= 0) {
    prefs[free_slot].used = 1;
    snprintf(prefs[free_slot].key, sizeof(prefs[free_slot].key), "%s", key);
    snprintf(prefs[free_slot].val, sizeof(prefs[free_slot].val), "%s", val);
  }
}

// the writable/data dir = the game folder the NRO launched from (the CWD)
static const char *data_dir(void) {
  static char dir[256];
  if (!dir[0]) {
    if (!getcwd(dir, sizeof(dir)) || !dir[0]) strcpy(dir, ".");
    size_t n = strlen(dir);
    if (n > 1 && dir[n - 1] == '/') dir[n - 1] = 0;
  }
  return dir;
}

// ---------------------------------------------------------------------------
// native -> Java method dispatch (by method name; the signature disambiguates
// the JNI slot, so name collisions across classes are rare and logged)
// ---------------------------------------------------------------------------

static void *call_object(FakeID *id, va_list va) {
  const char *n = id->name;
  // JniHelper's default class loader: getClassLoader() must return non-null and
  // loadClass(name) must return a usable jclass, or class lookup NULL-derefs.
  if (!strcmp(n, "getClassLoader"))
    return get_class("java/lang/ClassLoader");
  if (!strcmp(n, "loadClass"))
    return get_class(obj_str(va_arg(va, void *)));
  // the Android Context / AssetManager the engine pulls off the activity;
  // these must be non-null (SpriteStudio's asset load derefs them)
  if (!strcmp(n, "getContext") || !strcmp(n, "getApplicationContext") ||
      !strcmp(n, "getActivity") || !strcmp(n, "getBaseContext"))
    return jni_make_thiz();
  if (!strcmp(n, "getAssets"))
    return get_class("android/content/res/AssetManager");
  if (!strcmp(n, "getCocos2dxWritablePath") || !strcmp(n, "getWritablePath"))
    return jni_make_string(data_dir());
  if (!strcmp(n, "getCocos2dxPackageName") || !strcmp(n, "getPackageName"))
    return jni_make_string(VPL_PACKAGE);
  if (!strcmp(n, "getCurrentLanguage") || !strcmp(n, "getCurrentLanguageCode"))
    return jni_make_string("en");
  if (!strcmp(n, "getDeviceModel"))
    return jni_make_string("Linux ARM64");
  if (!strcmp(n, "getAssetsPath"))
    return jni_make_string(ASSETS_DIR);
  if (!strcmp(n, "getVersion") || !strcmp(n, "getSystemVersion"))
    return jni_make_string("1.0");
  if (!strcmp(n, "getStringForKey")) {
    const char *key = obj_str(va_arg(va, void *));
    const char *dflt = obj_str(va_arg(va, void *));
    return jni_make_string(pref_get(key, dflt));
  }
  // FileUtilsAndroid.getObbAssetFileDescriptor(path) -> long[3]{fd,start,len}
  if (!strcmp(n, "getObbAssetFileDescriptor")) {
    const char *path = obj_str(va_arg(va, void *));
    AAsset *a = AAssetManager_open_fake(asset_manager(), path, 0);
    if (!a) return NULL;
    int64_t start = 0, len = 0;
    int fd = AAsset_openFileDescriptor64_fake(a, &start, &len);
    AAsset_close_fake(a);
    if (fd < 0) return NULL;
    int64_t *arr = malloc(3 * sizeof(int64_t));
    arr[0] = fd; arr[1] = start; arr[2] = len;
    return make_pri_array_adopt(arr, 3, 8);
  }
  if (DEBUG_LOG) debugPrintf("JNI callObject: unhandled %s.%s %s\n", id->cls, n, id->sig);
  (void)va;
  return NULL;
}

static juint call_int(FakeID *id, va_list va) {
  const char *n = id->name;
  if (!strcmp(n, "getBoolForKey")) {
    const char *key = obj_str(va_arg(va, void *));
    int dflt = va_arg(va, int);
    const char *v = pref_get(key, NULL);
    return v ? (juint)(!strcmp(v, "1") || !strcasecmp(v, "true")) : (juint)dflt;
  }
  if (!strcmp(n, "getIntegerForKey")) {
    const char *key = obj_str(va_arg(va, void *));
    int dflt = va_arg(va, int);
    const char *v = pref_get(key, NULL);
    return v ? (juint)atoi(v) : (juint)dflt;
  }
  /* Do not simulate entitlement, network or permission success. Requests not
     implemented by this Linux JNI layer fail closed. */
  if (!strcmp(n, "getDPI")) return 320;
  if (!strcmp(n, "getDeviceRotation")) return 0;
  if (!strcmp(n, "openURL")) return 0;
  if (DEBUG_LOG) debugPrintf("JNI callInt: unhandled %s.%s %s\n", id->cls, n, id->sig);
  (void)va;
  return 0;
}

static juint call_long(FakeID *id, va_list va) {
  (void)va;
  if (DEBUG_LOG) debugPrintf("JNI callLong: unhandled %s.%s\n", id->cls, id->name);
  return 0;
}

static double call_double(FakeID *id, va_list va) {
  const char *n = id->name;
  if (!strcmp(n, "getFloatForKey") || !strcmp(n, "getDoubleForKey")) {
    (void)va_arg(va, void *);           // key
    double dflt = va_arg(va, double);   // float/double default (promoted)
    return dflt;
  }
  (void)va;
  return 0.0;
}

static void call_void(FakeID *id, va_list va) {
  const char *n = id->name;
  // UserDefault setters
  if (!strcmp(n, "setBoolForKey")) {
    const char *key = obj_str(va_arg(va, void *));
    int v = va_arg(va, int);
    pref_set(key, v ? "1" : "0"); return;
  }
  if (!strcmp(n, "setIntegerForKey")) {
    const char *key = obj_str(va_arg(va, void *));
    int v = va_arg(va, int);
    char b[32]; snprintf(b, sizeof(b), "%d", v); pref_set(key, b); return;
  }
  if (!strcmp(n, "setFloatForKey") || !strcmp(n, "setDoubleForKey")) {
    const char *key = obj_str(va_arg(va, void *));
    double v = va_arg(va, double);
    char b[64]; snprintf(b, sizeof(b), "%g", v); pref_set(key, b); return;
  }
  if (!strcmp(n, "setStringForKey")) {
    const char *key = obj_str(va_arg(va, void *));
    const char *v = obj_str(va_arg(va, void *));
    pref_set(key, v); return;
  }
  // lifecycle / no-op device hooks
  if (!strcmp(n, "terminateProcessJNI") || !strcmp(n, "finish") ||
      !strcmp(n, "appTerminate") || !strcmp(n, "appEnd")) {
    debugPrintf("JNI: %s.%s requested application finish\n", id->cls, n);
    jni_quit_requested = 1;
    return;
  }
  if (DEBUG_LOG) debugPrintf("JNI callVoid: %s.%s %s\n", id->cls, n, id->sig);
  (void)va;
}

// ---------------------------------------------------------------------------
// JNIEnv implementations
// ---------------------------------------------------------------------------
static juint j_GetVersion(void *env) { (void)env; return JNI_VERSION_1_6; }
static void *j_FindClass(void *env, const char *name) { (void)env; return get_class(name); }
static void *j_GetObjectClass(void *env, void *obj) { (void)env; (void)obj; return get_class("?"); }
static void *j_GetMethodID(void *env, void *cls, const char *name, const char *sig) {
  (void)env; FakeObject *c = cls; return get_id(c && c->tag == TAG_CLASS ? c->label : "", name, sig);
}
static void *j_NewGlobalRef(void *env, void *obj) {
  (void)env;
  mutexLock(&locals_lock);
  for (int i = locals_top - 1; i >= 0; i--)
    if (locals[i] == obj) { locals[i] = locals[--locals_top]; break; }
  mutexUnlock(&locals_lock);
  return obj;
}
static void j_DeleteGlobalRef(void *env, void *obj) { (void)env; free_ref(obj); }
static void j_DeleteLocalRef(void *env, void *obj) { (void)env; delete_local(obj); }
static void *j_NewLocalRef(void *env, void *obj) { (void)env; return obj; }
static juint j_IsSameObject(void *env, void *a, void *b) { (void)env; return a == b; }
static juint j_EnsureLocalCapacity(void *env, int cap) { (void)env; (void)cap; return 0; }

static juint j_PushLocalFrame(void *env, int cap) {
  (void)env; (void)cap;
  mutexLock(&locals_lock);
  if (frame_top < MAX_FRAMES) frames[frame_top++] = locals_top;
  mutexUnlock(&locals_lock);
  return 0;
}
static void *j_PopLocalFrame(void *env, void *result) {
  (void)env;
  mutexLock(&locals_lock);
  const int mark = frame_top > 0 ? frames[--frame_top] : 0;
  for (int i = mark; i < locals_top; i++) if (locals[i] != result) free_ref(locals[i]);
  locals_top = mark;
  if (result && locals_top < MAX_LOCALS) locals[locals_top++] = result;
  mutexUnlock(&locals_lock);
  return result;
}

// Call<type>Method (instance + static share name dispatch)
#define CALL_VARIADIC(fn, ret_t, dispatch) \
  static ret_t fn(void *env, void *recv, FakeID *id, ...) { \
    (void)env; (void)recv; va_list va; va_start(va, id); \
    ret_t r = dispatch(id, va); va_end(va); return r; } \
  static ret_t fn##V(void *env, void *recv, FakeID *id, va_list va) { \
    (void)env; (void)recv; return dispatch(id, va); }

CALL_VARIADIC(j_CallObjectMethod, void *, call_object)
CALL_VARIADIC(j_CallIntMethod, juint, call_int)
CALL_VARIADIC(j_CallBooleanMethod, juint, call_int)
CALL_VARIADIC(j_CallLongMethod, juint, call_long)
CALL_VARIADIC(j_CallDoubleMethod, double, call_double)
CALL_VARIADIC(j_CallFloatMethod, double, call_double)

static void j_CallVoidMethod(void *env, void *recv, FakeID *id, ...) {
  (void)env; (void)recv; va_list va; va_start(va, id); call_void(id, va); va_end(va);
}
static void j_CallVoidMethodV(void *env, void *recv, FakeID *id, va_list va) {
  (void)env; (void)recv; call_void(id, va);
}

#define j_CallStaticObjectMethod   j_CallObjectMethod
#define j_CallStaticObjectMethodV  j_CallObjectMethodV
#define j_CallStaticIntMethod      j_CallIntMethod
#define j_CallStaticIntMethodV     j_CallIntMethodV
#define j_CallStaticBooleanMethod  j_CallBooleanMethod
#define j_CallStaticBooleanMethodV j_CallBooleanMethodV
#define j_CallStaticLongMethod     j_CallLongMethod
#define j_CallStaticLongMethodV    j_CallLongMethodV
#define j_CallStaticFloatMethod    j_CallFloatMethod
#define j_CallStaticFloatMethodV   j_CallFloatMethodV
#define j_CallStaticDoubleMethod   j_CallDoubleMethod
#define j_CallStaticDoubleMethodV  j_CallDoubleMethodV
#define j_CallStaticVoidMethod     j_CallVoidMethod
#define j_CallStaticVoidMethodV    j_CallVoidMethodV

// strings
static void *j_NewStringUTF(void *env, const char *utf) { (void)env; return jni_make_string(utf); }
static const char *j_GetStringUTFChars(void *env, void *jstr, uint8_t *is_copy) {
  (void)env; if (is_copy) *is_copy = 0; return obj_str(jstr);
}
static void j_ReleaseStringUTFChars(void *env, void *jstr, const char *utf) { (void)env; (void)jstr; (void)utf; }
static juint j_GetStringUTFLength(void *env, void *jstr) { (void)env; return strlen(obj_str(jstr)); }
static juint j_GetStringLength(void *env, void *jstr) { (void)env; return strlen(obj_str(jstr)); }

// UTF-16 string access: cocos2d::StringUtils::getStringUTFCharsJNI() (used by
// JniHelper::jstring2string for every Java->std::string conversion) reads via
// GetStringChars + GetStringLength, not GetStringUTFChars. Our fake strings are
// ASCII, so widen byte-per-byte to jchar.
static const uint16_t *j_GetStringChars(void *env, void *jstr, uint8_t *is_copy) {
  (void)env;
  if (is_copy) *is_copy = 1;
  const char *s = obj_str(jstr);
  size_t n = strlen(s);
  uint16_t *buf = malloc((n + 1) * sizeof(uint16_t));
  if (!buf) return NULL;
  for (size_t i = 0; i < n; i++) buf[i] = (uint8_t)s[i];
  buf[n] = 0;
  return buf;
}
static void j_ReleaseStringChars(void *env, void *jstr, const uint16_t *chars) {
  (void)env; (void)jstr; free((void *)chars);
}

// arrays
static juint j_GetArrayLength(void *env, void *arr) {
  (void)env; FakeObjArray *a = arr;
  if (a && (a->tag == TAG_PRIARR || a->tag == TAG_OBJARR)) return a->len;
  return 0;
}
static void *j_NewByteArray(void *env, int len) { (void)env; return new_pri_array(len, 1); }
static void *j_NewIntArray(void *env, int len) { (void)env; return new_pri_array(len, 4); }
static void *j_NewFloatArray(void *env, int len) { (void)env; return new_pri_array(len, 4); }
static void *j_NewLongArray(void *env, int len) { (void)env; return new_pri_array(len, 8); }

static void *j_GetPriArrayElements(void *env, void *arr, uint8_t *is_copy) {
  (void)env; if (is_copy) *is_copy = 0;
  FakePriArray *a = arr; return (a && a->tag == TAG_PRIARR) ? a->data : NULL;
}
static void j_ReleasePriArrayElements(void *env, void *arr, void *elems, int mode) {
  (void)env; (void)arr; (void)elems; (void)mode;
}
static void j_GetPriArrayRegion(void *env, void *arr, int start, int len, void *buf) {
  (void)env; FakePriArray *a = arr;
  if (a && a->tag == TAG_PRIARR && buf && start >= 0 && len >= 0 &&
      start <= a->len && len <= a->len - start)
    memcpy(buf, (char *)a->data + (size_t)start * a->elem_size, (size_t)len * a->elem_size);
}
static void j_SetPriArrayRegion(void *env, void *arr, int start, int len, const void *buf) {
  (void)env; FakePriArray *a = arr;
  if (a && a->tag == TAG_PRIARR && buf && start >= 0 && len >= 0 &&
      start <= a->len && len <= a->len - start)
    memcpy((char *)a->data + (size_t)start * a->elem_size, buf, (size_t)len * a->elem_size);
}

// misc
static juint j_RegisterNatives(void *env, void *cls, void *methods, int n) {
  (void)env; (void)cls; (void)methods; (void)n;
  /* ART-style native method registration is not implemented yet. */
  return (juint)-1;
}
static juint j_GetJavaVM(void *env, void **vm) { (void)env; *vm = fake_vm; return JNI_OK; }
static juint j_ExceptionCheck(void *env) { (void)env; return 0; }
static void *j_ExceptionOccurred(void *env) { (void)env; return NULL; }
static void j_void1(void *env) { (void)env; }
static juint j_unimplemented(void) { return 0; }

// ---------------------------------------------------------------------------
// table assembly (JNI spec indices)
// ---------------------------------------------------------------------------
static void *env_table[233];
static void **env_table_ptr = env_table;
void *fake_env = &env_table_ptr;

static juint vm_DestroyJavaVM(void *vm) { (void)vm; return JNI_OK; }
static juint vm_AttachCurrentThread(void *vm, void **env, void *args) {
  (void)vm; (void)args; if (env) *env = fake_env; return JNI_OK;
}
static juint vm_DetachCurrentThread(void *vm) { (void)vm; return JNI_OK; }
static juint vm_GetEnv(void *vm, void **env, int version) {
  (void)vm; (void)version; if (env) *env = fake_env; return JNI_OK;
}
static void *vm_table[8];
static void **vm_table_ptr = vm_table;
void *fake_vm = &vm_table_ptr;

void jni_init(void) {
  mutexInit(&locals_lock);

  for (int i = 0; i < 233; i++) env_table[i] = (void *)j_unimplemented;

  env_table[4]   = (void *)j_GetVersion;
  env_table[6]   = (void *)j_FindClass;
  env_table[15]  = (void *)j_ExceptionOccurred;
  env_table[16]  = (void *)j_void1;  // ExceptionDescribe
  env_table[17]  = (void *)j_void1;  // ExceptionClear
  env_table[19]  = (void *)j_PushLocalFrame;
  env_table[20]  = (void *)j_PopLocalFrame;
  env_table[21]  = (void *)j_NewGlobalRef;
  env_table[22]  = (void *)j_DeleteGlobalRef;
  env_table[23]  = (void *)j_DeleteLocalRef;
  env_table[24]  = (void *)j_IsSameObject;
  env_table[25]  = (void *)j_NewLocalRef;
  env_table[26]  = (void *)j_EnsureLocalCapacity;
  env_table[31]  = (void *)j_GetObjectClass;
  env_table[33]  = (void *)j_GetMethodID;
  env_table[34]  = (void *)j_CallObjectMethod;
  env_table[35]  = (void *)j_CallObjectMethodV;
  env_table[37]  = (void *)j_CallBooleanMethod;
  env_table[38]  = (void *)j_CallBooleanMethodV;
  env_table[49]  = (void *)j_CallIntMethod;
  env_table[50]  = (void *)j_CallIntMethodV;
  env_table[53]  = (void *)j_CallLongMethod;
  env_table[54]  = (void *)j_CallLongMethodV;
  env_table[57]  = (void *)j_CallFloatMethod;
  env_table[58]  = (void *)j_CallFloatMethodV;
  env_table[59]  = (void *)j_CallDoubleMethod;
  env_table[60]  = (void *)j_CallDoubleMethodV;
  env_table[61]  = (void *)j_CallVoidMethod;
  env_table[62]  = (void *)j_CallVoidMethodV;
  env_table[113] = (void *)j_GetMethodID;             // GetStaticMethodID
  env_table[114] = (void *)j_CallStaticObjectMethod;
  env_table[115] = (void *)j_CallStaticObjectMethodV;
  env_table[117] = (void *)j_CallStaticBooleanMethod;
  env_table[118] = (void *)j_CallStaticBooleanMethodV;
  env_table[129] = (void *)j_CallStaticIntMethod;
  env_table[130] = (void *)j_CallStaticIntMethodV;
  env_table[132] = (void *)j_CallStaticLongMethod;
  env_table[133] = (void *)j_CallStaticLongMethodV;
  env_table[135] = (void *)j_CallStaticFloatMethod;
  env_table[136] = (void *)j_CallStaticFloatMethodV;
  env_table[138] = (void *)j_CallStaticDoubleMethod;
  env_table[139] = (void *)j_CallStaticDoubleMethodV;
  env_table[141] = (void *)j_CallStaticVoidMethod;
  env_table[142] = (void *)j_CallStaticVoidMethodV;
  env_table[144] = (void *)j_GetMethodID;             // GetStaticFieldID
  env_table[164] = (void *)j_GetStringLength;
  env_table[165] = (void *)j_GetStringChars;
  env_table[166] = (void *)j_ReleaseStringChars;
  env_table[167] = (void *)j_NewStringUTF;
  env_table[168] = (void *)j_GetStringUTFLength;
  env_table[169] = (void *)j_GetStringUTFChars;
  env_table[170] = (void *)j_ReleaseStringUTFChars;
  env_table[171] = (void *)j_GetArrayLength;
  env_table[176] = (void *)j_NewByteArray;
  env_table[179] = (void *)j_NewIntArray;
  env_table[180] = (void *)j_NewLongArray;
  env_table[181] = (void *)j_NewFloatArray;
  for (int i = 183; i <= 190; i++) env_table[i] = (void *)j_GetPriArrayElements;
  for (int i = 191; i <= 198; i++) env_table[i] = (void *)j_ReleasePriArrayElements;
  for (int i = 199; i <= 206; i++) env_table[i] = (void *)j_GetPriArrayRegion;
  for (int i = 207; i <= 214; i++) env_table[i] = (void *)j_SetPriArrayRegion;
  env_table[215] = (void *)j_RegisterNatives;
  env_table[219] = (void *)j_GetJavaVM;
  env_table[222] = (void *)j_GetPriArrayElements;      // GetPrimitiveArrayCritical
  env_table[223] = (void *)j_ReleasePriArrayElements;  // ReleasePrimitiveArrayCritical
  env_table[226] = (void *)j_NewGlobalRef;             // NewWeakGlobalRef
  env_table[227] = (void *)j_DeleteGlobalRef;          // DeleteWeakGlobalRef
  env_table[228] = (void *)j_ExceptionCheck;

  vm_table[3] = (void *)vm_DestroyJavaVM;
  vm_table[4] = (void *)vm_AttachCurrentThread;
  vm_table[5] = (void *)vm_DetachCurrentThread;
  vm_table[6] = (void *)vm_GetEnv;
  vm_table[7] = (void *)vm_AttachCurrentThread;
}

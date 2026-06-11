/*
 * rahook_jni.c — JNI bridge for RaHook
 *
 * Copyright (C) 2026 Dere3046
 */

#include <jni.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "rahook.h"

#define RAHOOK_JNI_VERSION JNI_VERSION_1_6

static int native_test_add(int a, int b) { return a + b; }
static int native_test_mul(int a, int b) { return a * b; }
static int native_test_double(int x) { return x * 2; }
static int native_test_triple(int x) { return x * 3; }

static void *g_hook_add = NULL;
static void *g_hook_mul = NULL;
static void *g_hook_double = NULL;
static void *g_hook_triple = NULL;

// hook replacement functions
static int hook_add(int a, int b) { return a + b + 100; }
static int hook_mul(int a, int b) { return a * b * 10; }
static int hook_double(int x) { return x * 20; }
static int hook_triple(int x) { return x * 30; }

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeAdd(JNIEnv *env, jclass clazz, jint a, jint b) {
    (void)env; (void)clazz;
    return native_test_add(a, b);
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeMul(JNIEnv *env, jclass clazz, jint a, jint b) {
    (void)env; (void)clazz;
    return native_test_mul(a, b);
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeDouble(JNIEnv *env, jclass clazz, jint x) {
    (void)env; (void)clazz;
    return native_test_double(x);
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeTriple(JNIEnv *env, jclass clazz, jint x) {
    (void)env; (void)clazz;
    return native_test_triple(x);
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeHookAdd(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    if (g_hook_add) return 0;
    g_hook_add = rahook_quick((void*)native_test_add, (void*)hook_add, NULL);
    return g_hook_add ? 1 : 0;
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeUnhookAdd(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    if (!g_hook_add) return 0;
    rahook_remove(g_hook_add);
    g_hook_add = NULL;
    return 1;
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeHookMul(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    if (g_hook_mul) return 0;
    g_hook_mul = rahook_quick((void*)native_test_mul, (void*)hook_mul, NULL);
    return g_hook_mul ? 1 : 0;
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeUnhookMul(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    if (!g_hook_mul) return 0;
    rahook_remove(g_hook_mul);
    g_hook_mul = NULL;
    return 1;
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeHookDouble(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    if (g_hook_double) return 0;
    g_hook_double = rahook_quick((void*)native_test_double, (void*)hook_double, NULL);
    return g_hook_double ? 1 : 0;
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeUnhookDouble(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    if (!g_hook_double) return 0;
    rahook_remove(g_hook_double);
    g_hook_double = NULL;
    return 1;
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeUnhookAll(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    if (g_hook_add) { rahook_remove(g_hook_add); g_hook_add = NULL; }
    if (g_hook_mul) { rahook_remove(g_hook_mul); g_hook_mul = NULL; }
    if (g_hook_double) { rahook_remove(g_hook_double); g_hook_double = NULL; }
    if (g_hook_triple) { rahook_remove(g_hook_triple); g_hook_triple = NULL; }
    return 1;
}

JNIEXPORT jstring JNICALL
Java_rahook_RaHook_nativeVersion(JNIEnv *env, jclass clazz) {
    (void)clazz;
    return (*env)->NewStringUTF(env, RAHOOK_VERSION);
}

JNIEXPORT jstring JNICALL
Java_rahook_RaHook_nativeRunTests(JNIEnv *env, jclass clazz) {
    (void)clazz;
    char buf[4096];
    int off = 0;
    off += snprintf(buf + off, sizeof(buf) - off, "RaHook %s\n\n", RAHOOK_VERSION);

    off += snprintf(buf + off, sizeof(buf) - off, "init: %s\n", rahook_init() == 0 ? "OK" : "FAIL");
    off += snprintf(buf + off, sizeof(buf) - off, "errno: %d\n", rahook_get_errno());
    off += snprintf(buf + off, sizeof(buf) - off, "errmsg: %s\n", rahook_to_errmsg(0));

    off += snprintf(buf + off, sizeof(buf) - off, "\nsymbol:\n");
    void *h = rahook_dlopen("libc.so");
    if (h) {
        void *sym = rahook_dlsym(h, "strlen");
        off += snprintf(buf + off, sizeof(buf) - off, "  dlopen libc OK, strlen=%p\n", sym);
        rahook_dlclose(h);
    } else {
        off += snprintf(buf + off, sizeof(buf) - off, "  dlopen FAIL (no libc, expected on stub)\n");
    }

    off += snprintf(buf + off, sizeof(buf) - off, "\nnative functions:\n");
    off += snprintf(buf + off, sizeof(buf) - off, "  add(3,4)=%d\n", native_test_add(3, 4));
    off += snprintf(buf + off, sizeof(buf) - off, "  mul(5,6)=%d\n", native_test_mul(5, 6));
    off += snprintf(buf + off, sizeof(buf) - off, "  double(10)=%d\n", native_test_double(10));
    off += snprintf(buf + off, sizeof(buf) - off, "  triple(10)=%d\n", native_test_triple(10));

    off += snprintf(buf + off, sizeof(buf) - off, "\nhook libc strlen:\n");
    typedef size_t (*strlen_t)(const char*);
    strlen_t real_strlen = (strlen_t)raknob_dlsym(NULL, "strlen");
    if (real_strlen) {
        off += snprintf(buf + off, sizeof(buf) - off, "  strlen(\"hello\")=%zu\n", real_strlen("hello"));
        static size_t stub_strlen(const char *s) { (void)s; return 999; }
        void *sh = rahook_quick((void*)real_strlen, (void*)stub_strlen, NULL);
        if (sh) {
            size_t hooked = ((strlen_t)real_strlen)("hello");
            off += snprintf(buf + off, sizeof(buf) - off, "  hooked strlen(\"hello\")=%zu (expect 999)\n", hooked);
            rahook_remove(sh);
            off += snprintf(buf + off, sizeof(buf) - off, "  unhooked strlen(\"hello\")=%zu (expect 5)\n", real_strlen("hello"));
        } else {
            off += snprintf(buf + off, sizeof(buf) - off, "  hook FAILED (target may be in .plt)\n");
        }
    } else {
        off += snprintf(buf + off, sizeof(buf) - off, "  dlsym strlen FAILED (expected on stub device)\n");
    }

    off += snprintf(buf + off, sizeof(buf) - off, "\nerror handling:\n");
    off += snprintf(buf + off, sizeof(buf) - off, "  hook(NULL)=%p\n", rahook(NULL, NULL, NULL, 0));

    off += snprintf(buf + off, sizeof(buf) - off, "\ntransaction:\n");
    off += snprintf(buf + off, sizeof(buf) - off, "  begin=%d\n", rahook_begin_transaction());
    off += snprintf(buf + off, sizeof(buf) - off, "  abort=%d\n", rahook_abort_transaction());

    off += snprintf(buf + off, sizeof(buf) - off, "\nrecording:\n");
    rahook_record_start();
    off += snprintf(buf + off, sizeof(buf) - off, "  active=%d\n", rahook_record_is_active());
    rahook_record_stop();
    char *json = rahook_record_export();
    off += snprintf(buf + off, sizeof(buf) - off, "  json=%s\n", json);
    rahook_record_free(json);

    off += snprintf(buf + off, sizeof(buf) - off, "\ndisable:\n");
    rahook_set_disable(true);
    off += snprintf(buf + off, sizeof(buf) - off, "  disabled=%d\n", rahook_get_disable());
    rahook_set_disable(false);

    off += snprintf(buf + off, sizeof(buf) - off, "\nthread ignore:\n");
    rahook_ignore_current_thread();
    off += snprintf(buf + off, sizeof(buf) - off, "  ignored=%d\n", rahook_is_thread_ignored());
    rahook_unignore_current_thread();

    off += snprintf(buf + off, sizeof(buf) - off, "\nlinker callbacks:\n");
    off += snprintf(buf + off, sizeof(buf) - off, "  register=%d\n", rahook_register_dl_init_cb(NULL));
    off += snprintf(buf + off, sizeof(buf) - off, "  unregister=%d\n", rahook_unregister_dl_init_cb(NULL));

    off += snprintf(buf + off, sizeof(buf) - off, "\nplugin: chdir /data/local/tmp\n");
    int cd = chdir("/data/local/tmp");
    off += snprintf(buf + off, sizeof(buf) - off, "  chdir=%d\n", cd);

    off += snprintf(buf + off, sizeof(buf) - off, "\nplugin: test nativeRunTests done\n");

    rahook_deinit();
    return (*env)->NewStringUTF(env, buf);
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)vm; (void)reserved;
    rahook_init();
    return RAHOOK_JNI_VERSION;
}

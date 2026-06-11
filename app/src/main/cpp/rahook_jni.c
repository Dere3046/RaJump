/*
 * rahook_jni.c — JNI bridge for RaHook
 *
 * Copyright (C) 2026 Dere3046
 */

#include <jni.h>
#include <string.h>
#include <stdlib.h>

#include "rahook.h"

#define RAHOOK_JNI_VERSION JNI_VERSION_1_6

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeInit(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    return (jint)rahook_init();
}

JNIEXPORT void JNICALL
Java_rahook_RaHook_nativeDeinit(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    rahook_deinit();
}

JNIEXPORT jstring JNICALL
Java_rahook_RaHook_nativeVersion(JNIEnv *env, jclass clazz) {
    (void)clazz;
    return (*env)->NewStringUTF(env, RAHOOK_VERSION);
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeGetErrno(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    return (jint)rahook_get_errno();
}

JNIEXPORT jstring JNICALL
Java_rahook_RaHook_nativeToErrmsg(JNIEnv *env, jclass clazz, jint err) {
    (void)clazz;
    return (*env)->NewStringUTF(env, rahook_to_errmsg((int)err));
}

JNIEXPORT void JNICALL
Java_rahook_RaHook_nativeSetDisable(JNIEnv *env, jclass clazz, jboolean disable) {
    (void)env; (void)clazz;
    rahook_set_disable((bool)disable);
}

JNIEXPORT jboolean JNICALL
Java_rahook_RaHook_nativeGetDisable(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    return (jboolean)rahook_get_disable();
}

JNIEXPORT void JNICALL
Java_rahook_RaHook_nativeSetDebug(JNIEnv *env, jclass clazz, jboolean debug) {
    (void)env; (void)clazz;
    rahook_set_debug((bool)debug);
}

JNIEXPORT jlong JNICALL
Java_rahook_RaHook_nativeDlopen(JNIEnv *env, jclass clazz, jstring lib) {
    (void)clazz;
    const char *l = (*env)->GetStringUTFChars(env, lib, NULL);
    void *h = rahook_dlopen(l);
    (*env)->ReleaseStringUTFChars(env, lib, l);
    return (jlong)(uintptr_t)h;
}

JNIEXPORT void JNICALL
Java_rahook_RaHook_nativeDlclose(JNIEnv *env, jclass clazz, jlong handle) {
    (void)env; (void)clazz;
    rahook_dlclose((void *)(uintptr_t)handle);
}

JNIEXPORT jlong JNICALL
Java_rahook_RaHook_nativeDlsym(JNIEnv *env, jclass clazz, jlong handle, jstring sym) {
    (void)clazz;
    const char *s = (*env)->GetStringUTFChars(env, sym, NULL);
    void *addr = rahook_dlsym((void *)(uintptr_t)handle, s);
    (*env)->ReleaseStringUTFChars(env, sym, s);
    return (jlong)(uintptr_t)addr;
}

JNIEXPORT jlong JNICALL
Java_rahook_RaHook_nativeHook(JNIEnv *env, jclass clazz, jlong target, jlong replace, jint flags) {
    (void)env; (void)clazz;
    return (jlong)(uintptr_t)rahook((void *)(uintptr_t)target, (void *)(uintptr_t)replace, NULL, (uint32_t)flags);
}

JNIEXPORT jlong JNICALL
Java_rahook_RaHook_nativeHookSymbol(JNIEnv *env, jclass clazz, jstring lib, jstring sym,
                                      jlong replace, jint flags) {
    (void)clazz;
    const char *l = (*env)->GetStringUTFChars(env, lib, NULL);
    const char *s = (*env)->GetStringUTFChars(env, sym, NULL);
    void *st = rahook_symbol(l, s, (void *)(uintptr_t)replace, NULL, (uint32_t)flags);
    (*env)->ReleaseStringUTFChars(env, lib, l);
    (*env)->ReleaseStringUTFChars(env, sym, s);
    return (jlong)(uintptr_t)st;
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeRemove(JNIEnv *env, jclass clazz, jlong stub) {
    (void)env; (void)clazz;
    return (jint)rahook_remove((void *)(uintptr_t)stub);
}

JNIEXPORT jlong JNICALL
Java_rahook_RaHook_nativePrePost(JNIEnv *env, jclass clazz, jlong target, jint flags) {
    (void)env; (void)clazz;
    return (jlong)(uintptr_t)rahook_pre_post((void *)(uintptr_t)target, NULL, NULL, NULL, NULL, (uint32_t)flags);
}

JNIEXPORT jlong JNICALL
Java_rahook_RaHook_nativeIntercept(JNIEnv *env, jclass clazz, jlong addr) {
    (void)env; (void)clazz;
    return (jlong)(uintptr_t)rahook_intercept((void *)(uintptr_t)addr, NULL, NULL);
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeUnintercept(JNIEnv *env, jclass clazz, jlong stub) {
    (void)env; (void)clazz;
    return (jint)rahook_unintercept((void *)(uintptr_t)stub);
}

JNIEXPORT jlong JNICALL
Java_rahook_RaHook_nativePlt(JNIEnv *env, jclass clazz, jlong target, jlong replace) {
    (void)env; (void)clazz;
    return (jlong)(uintptr_t)rahook_plt((void *)(uintptr_t)target, (void *)(uintptr_t)replace, NULL);
}

JNIEXPORT jlong JNICALL
Java_rahook_RaHook_nativePltSymbol(JNIEnv *env, jclass clazz, jstring lib, jstring sym, jlong replace) {
    (void)clazz;
    const char *l = (*env)->GetStringUTFChars(env, lib, NULL);
    const char *s = (*env)->GetStringUTFChars(env, sym, NULL);
    void *st = rahook_plt_symbol(l, s, (void *)(uintptr_t)replace, NULL);
    (*env)->ReleaseStringUTFChars(env, lib, l);
    (*env)->ReleaseStringUTFChars(env, sym, s);
    return (jlong)(uintptr_t)st;
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeBeginTransaction(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    return (jint)rahook_begin_transaction();
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeEndTransaction(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    return (jint)rahook_end_transaction();
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativeAbortTransaction(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    return (jint)rahook_abort_transaction();
}

JNIEXPORT void JNICALL
Java_rahook_RaHook_nativeRecordStart(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    rahook_record_start();
}

JNIEXPORT void JNICALL
Java_rahook_RaHook_nativeRecordStop(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    rahook_record_stop();
}

JNIEXPORT jboolean JNICALL
Java_rahook_RaHook_nativeRecordIsActive(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    return (jboolean)rahook_record_is_active();
}

JNIEXPORT jstring JNICALL
Java_rahook_RaHook_nativeRecordExport(JNIEnv *env, jclass clazz) {
    (void)clazz;
    char *json = rahook_record_export();
    jstring result = (*env)->NewStringUTF(env, json);
    rahook_record_free(json);
    return result;
}

JNIEXPORT void JNICALL
Java_rahook_RaHook_nativeIgnoreThread(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    rahook_ignore_current_thread();
}

JNIEXPORT void JNICALL
Java_rahook_RaHook_nativeUnignoreThread(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    rahook_unignore_current_thread();
}

JNIEXPORT jint JNICALL
Java_rahook_RaHook_nativePatch(JNIEnv *env, jclass clazz, jlong addr, jbyteArray code) {
    (void)clazz;
    jsize len = (*env)->GetArrayLength(env, code);
    jbyte *buf = (*env)->GetByteArrayElements(env, code, NULL);
    int r = rahook_patch((void *)(uintptr_t)addr, buf, (size_t)len);
    (*env)->ReleaseByteArrayElements(env, code, buf, JNI_ABORT);
    return (jint)r;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)vm; (void)reserved;
    return RAHOOK_JNI_VERSION;
}

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "rh_safe.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#define RH_SAFE_IDX_MALLOC    0
#define RH_SAFE_IDX_FREE      1
#define RH_SAFE_IDX_MTX_LOCK  2
#define RH_SAFE_IDX_MTX_UNLK  3
#define RH_SAFE_IDX_ABORT     4
#define RH_SAFE_IDX_COUNT     5

static void *rh_safe_funcs[RH_SAFE_IDX_COUNT];

int rh_safe_init(void) {
    void *handle = dlopen("libc.so", RTLD_NOW | RTLD_LOCAL);
    if (!handle) return -1;

    rh_safe_funcs[RH_SAFE_IDX_MALLOC]   = dlsym(handle, "malloc");
    rh_safe_funcs[RH_SAFE_IDX_FREE]     = dlsym(handle, "free");
    rh_safe_funcs[RH_SAFE_IDX_MTX_LOCK] = dlsym(handle, "pthread_mutex_lock");
    rh_safe_funcs[RH_SAFE_IDX_MTX_UNLK] = dlsym(handle, "pthread_mutex_unlock");
    rh_safe_funcs[RH_SAFE_IDX_ABORT]    = dlsym(handle, "abort");

    dlclose(handle);

    for (int i = 0; i < RH_SAFE_IDX_COUNT; i++) {
        if (!rh_safe_funcs[i]) return -1;
    }
    return 0;
}

void *rh_safe_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
#if defined(__arm__) || defined(__i386__)
    unsigned long pgoff = (unsigned long)offset >> 12;
    return (void *)(uintptr_t)syscall(SYS_mmap2, addr, length, prot, flags, fd, pgoff);
#else
    return (void *)(uintptr_t)syscall(SYS_mmap, addr, length, prot, flags, fd, offset);
#endif
}

int rh_safe_munmap(void *addr, size_t size) {
    return (int)syscall(SYS_munmap, addr, size);
}

int rh_safe_mprotect(void *addr, size_t len, int prot) {
    return (int)syscall(SYS_mprotect, addr, len, prot);
}

void *rh_safe_malloc(size_t size) {
    return ((void *(*)(size_t))rh_safe_funcs[RH_SAFE_IDX_MALLOC])(size);
}

void rh_safe_free(void *ptr) {
    ((void (*)(void *))rh_safe_funcs[RH_SAFE_IDX_FREE])(ptr);
}

int rh_safe_pthread_mutex_lock(pthread_mutex_t *mutex) {
    return ((int (*)(pthread_mutex_t *))rh_safe_funcs[RH_SAFE_IDX_MTX_LOCK])(mutex);
}

int rh_safe_pthread_mutex_unlock(pthread_mutex_t *mutex) {
    return ((int (*)(pthread_mutex_t *))rh_safe_funcs[RH_SAFE_IDX_MTX_UNLK])(mutex);
}

void rh_safe_abort(void) {
    ((void (*)(void))rh_safe_funcs[RH_SAFE_IDX_ABORT])();
}

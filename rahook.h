/*
 * rahook.h — 4 Architecture Android Inline Hook Library
 *
 * Copyright (C) 2026 dere3046
 *
 * CPU context types derived from ShadowHook:
 * Copyright (c) 2021-2025 ByteDance Inc. (MIT)
 */

#ifndef RAHOOK_H
#define RAHOOK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RAHOOK_VERSION "3.0.0"

#ifdef __cplusplus
extern "C" {
#endif

int  rahook_init(void);
void rahook_deinit(void);

// error handling
#define RAHOOK_ERR_OK             0
#define RAHOOK_ERR_PENDING        1
#define RAHOOK_ERR_UNINIT         2
#define RAHOOK_ERR_INVALID_ARG    3
#define RAHOOK_ERR_OOM            4
#define RAHOOK_ERR_MPROT          5
#define RAHOOK_ERR_WRITE_CRASH    6
#define RAHOOK_ERR_DUP            7
#define RAHOOK_ERR_NOT_FOUND      8
#define RAHOOK_ERR_DEFERRED       9
#define RAHOOK_ERR_DISABLED      10
#define RAHOOK_ERR_MODE_CONFLICT 43
#define RAHOOK_ERR_HOOK_FAILED   99
#define RAHOOK_ERR_CFG_UNSAFE   100

int  rahook_get_errno(void);
const char *rahook_to_errmsg(int err);

// global disable/enable
bool rahook_get_disable(void);
void rahook_set_disable(bool disable);

// debug toggle
bool rahook_get_debug(void);
void rahook_set_debug(bool debug);

// symbol resolution (xDL)
void *rahook_dlopen(const char *lib_name);
void  rahook_dlclose(void *handle);
void *rahook_dlsym(void *handle, const char *sym_name);
void *rahook_dlsym_dynsym(void *handle, const char *sym_name);
void *rahook_dlsym_symtab(void *handle, const char *sym_name);

// hook flags
#define RAHOOK_FLAG_SHARED 1
#define RAHOOK_FLAG_MULTI  2
#define RAHOOK_FLAG_UNIQUE 4

// hook
void *rahook(void *target, void *replace, void **origin, uint32_t flags);
void *rahook_symbol(const char *lib, const char *sym, void *replace,
                    void **origin, uint32_t flags);
typedef void (*rahook_hooked_cb_t)(int err, const char *lib, const char *sym,
                                    void *sym_addr, void *new_addr,
                                    void *orig_addr, void *arg);
void *rahook_symbol_callback(const char *lib, const char *sym, void *replace,
                             void **origin, uint32_t flags,
                             rahook_hooked_cb_t cb, void *cb_arg);
int  rahook_remove(void *stub);

// zero-config quick hook (auto-init + default unique mode)
static inline void *rahook_quick(void *target, void *replace, void **origin) {
    return rahook(target, replace, origin, RAHOOK_FLAG_UNIQUE);
}

// pre/post callback hook (like HookZz ZzHook)
// pre: called BEFORE original function, can modify ctx
// post: called AFTER original function, ctx contains return value
typedef union {
    uint64_t d[2];
    uint32_t s[4];
    uint16_t h[8];
    uint8_t  b[16];
} rahook_vreg_t;

#if defined(__aarch64__)
typedef struct {
    uint64_t regs[31];
    uint64_t sp, pc, pstate;
    rahook_vreg_t vregs[32];
    uint64_t fpsr, fpcr;
} rahook_ctx_t;
#elif defined(__arm__)
typedef struct {
    uint32_t regs[16];
    uint32_t cpsr, fpscr;
    rahook_vreg_t vregs[16];
} rahook_ctx_t;
#elif defined(__i386__)
typedef struct {
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi, eip, eflags;
} rahook_ctx_t;
#elif defined(__x86_64__)
typedef struct {
    uint64_t rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, rflags;
} rahook_ctx_t;
#endif

typedef void (*rahook_interceptor_t)(rahook_ctx_t *ctx, void *data);

typedef void (*rahook_pre_t)(rahook_ctx_t *ctx, void *user_data);
typedef void (*rahook_post_t)(rahook_ctx_t *ctx, void *user_data);

void *rahook_pre_post(void *target, rahook_pre_t pre, rahook_post_t post,
                      void *user_data, void **origin, uint32_t flags);
void *rahook_pre_post_symbol(const char *lib, const char *sym,
                             rahook_pre_t pre, rahook_post_t post,
                             void *user_data, void **origin, uint32_t flags);

// thread ignore portal (like Frida-gum)
void rahook_ignore_current_thread(void);
void rahook_unignore_current_thread(void);
bool rahook_is_thread_ignored(void);

// recording
void  rahook_record_start(void);
void  rahook_record_stop(void);
bool  rahook_record_is_active(void);
char *rahook_record_export(void);
void  rahook_record_free(char *json);

// linker callbacks
typedef void (*rahook_dl_cb_t)(const char *lib_name);
int rahook_register_dl_init_cb(rahook_dl_cb_t cb);
int rahook_register_dl_fini_cb(rahook_dl_cb_t cb);
int rahook_unregister_dl_init_cb(rahook_dl_cb_t cb);
int rahook_unregister_dl_fini_cb(rahook_dl_cb_t cb);

// low-level
int  rahook_patch(void *addr, const void *code, size_t size);
void rahook_cache_flush(void *addr, size_t size);

// transaction batching
int  rahook_begin_transaction(void);
int  rahook_end_transaction(void);
int  rahook_abort_transaction(void);
bool rahook_is_in_transaction(void);

// per-instruction intercept API
void *rahook_intercept(void *instr_addr, rahook_interceptor_t cb, void *data);
void *rahook_intercept_symbol(const char *lib, const char *sym, rahook_interceptor_t cb, void *data);
int   rahook_unintercept(void *stub);

// PLT fast-path (GOT replacement, like Dobby ImportTableReplace)
void *rahook_plt(void *target, void *replace, void **origin);
void *rahook_plt_symbol(const char *lib, const char *sym, void *replace, void **origin);

#ifdef __cplusplus
}
#endif

#include "core/rh_mode.h"

#endif

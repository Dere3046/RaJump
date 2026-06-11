/*
 * rh_bytesig.c — per-thread per-signal crash protection
 *
 * Copyright (C) 2026 dere3046
 *
 * Based on ShadowHook bytesig.c (MIT):
 * Copyright (c) 2021-2025 ByteDance Inc.
 */

#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef __ANDROID__
#define android_get_device_api_level() 21
#endif

#ifndef __predict_false
#define __predict_false(x)  __builtin_expect(!!(x), 0)
#endif
#ifndef __predict_true
#define __predict_true(x)   __builtin_expect(!!(x), 1)
#endif

#define RH_BYTESIG_SIGNAL_MAX NSIG
#define RH_BYTESIG_TID_MAX 256

typedef struct {
    pid_t tid;
    sigjmp_buf jbuf;
} rh_bytesig_slot_t;

typedef struct {
    rh_bytesig_slot_t slots[RH_BYTESIG_TID_MAX];
    struct sigaction old_action;
    int signum;
} rh_bytesig_signal_t;

static rh_bytesig_signal_t rh_bytesig_signals[RH_BYTESIG_SIGNAL_MAX];

static void rh_bytesig_handler(int signum, siginfo_t *info, void *ctx)
{
    (void)ctx;
    pid_t tid = (pid_t)syscall(SYS_gettid);
    rh_bytesig_signal_t *sig = &rh_bytesig_signals[signum];

    for (int i = 0; i < RH_BYTESIG_TID_MAX; i++) {
        if (sig->slots[i].tid == tid) {
            int ret = (signum << 16) | (info->si_code & 0xFF);
            siglongjmp(sig->slots[i].jbuf, ret);
        }
    }

    // no matching slot — chain to previous handler
    if (sig->old_action.sa_handler == SIG_DFL) {
        signal(signum, SIG_DFL);
    } else if (sig->old_action.sa_handler != SIG_IGN &&
               sig->old_action.sa_handler != NULL) {
        if (sig->old_action.sa_flags & SA_SIGINFO)
            sig->old_action.sa_sigaction(signum, info, ctx);
        else
            sig->old_action.sa_handler(signum);
    }
}

int rh_bytesig_init(int signum)
{
    if (signum < 0 || signum >= RH_BYTESIG_SIGNAL_MAX) return -1;
    rh_bytesig_signal_t *sig = &rh_bytesig_signals[signum];
    memset(sig->slots, 0, sizeof(sig->slots));
    sig->signum = signum;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = rh_bytesig_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    return sigaction(signum, &sa, &sig->old_action);
}

void rh_bytesig_protect(pid_t tid, sigjmp_buf *jbuf, const int signums[], size_t cnt)
{
    for (size_t n = 0; n < cnt; n++) {
        int s = signums[n];
        if (s < 0 || s >= RH_BYTESIG_SIGNAL_MAX) continue;
        rh_bytesig_signal_t *sig = &rh_bytesig_signals[s];
        for (int i = 0; i < RH_BYTESIG_TID_MAX; i++) {
            if (sig->slots[i].tid == 0) {
                sig->slots[i].tid = tid;
                memcpy(&sig->slots[i].jbuf, jbuf, sizeof(sigjmp_buf));
                break;
            }
        }
    }
}

void rh_bytesig_unprotect(pid_t tid, const int signums[], size_t cnt)
{
    for (size_t n = 0; n < cnt; n++) {
        int s = signums[n];
        if (s < 0 || s >= RH_BYTESIG_SIGNAL_MAX) continue;
        rh_bytesig_signal_t *sig = &rh_bytesig_signals[s];
        for (int i = 0; i < RH_BYTESIG_TID_MAX; i++) {
            if (sig->slots[i].tid == tid) {
                sig->slots[i].tid = 0;
                memset(&sig->slots[i].jbuf, 0, sizeof(sigjmp_buf));
                break;
            }
        }
    }
}

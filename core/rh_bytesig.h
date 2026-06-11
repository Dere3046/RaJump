#pragma once
#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

int rh_bytesig_init(int signum);
void rh_bytesig_protect(pid_t tid, sigjmp_buf *jbuf, const int signums[], size_t cnt);
void rh_bytesig_unprotect(pid_t tid, const int signums[], size_t cnt);

// simplified guard for safe memory probing (global pair, not per-thread)
static volatile int rh_guard_flag = 0;
static sigjmp_buf rh_guard_env;

static inline int rh_guard_begin(void) {
    rh_guard_flag = 1;
    if (sigsetjmp(rh_guard_env, 1) == 0) return 0;
    rh_guard_flag = 0;
    return 1;
}

static inline void rh_guard_end(void) {
    rh_guard_flag = 0;
}

static void rh_guard_handler(int sig, siginfo_t *info, void *ctx) {
    (void)sig; (void)info; (void)ctx;
    if (rh_guard_flag) siglongjmp(rh_guard_env, 1);
}

static inline int rh_guard_init(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = rh_guard_handler;
    return sigaction(SIGSEGV, &sa, NULL);
}

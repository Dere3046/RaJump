#pragma once
#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    jmp_buf jbuf;
    struct sigaction old_sa;
    int signum;
} rh_sig_jmp_t;

static _Thread_local rh_sig_jmp_t *rh_sig_current_jmp = NULL;

static void rh_sig_trampoline(int signum, siginfo_t *info, void *ctx) {
    (void)info;
    (void)ctx;
    if (rh_sig_current_jmp) longjmp(rh_sig_current_jmp->jbuf, signum);
}

static inline int rh_sig_init(int signum) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = rh_sig_trampoline;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    return sigaction(signum, &sa, NULL);
}

static inline int rh_sig_setjmp(rh_sig_jmp_t *jmp, int signum) {
    jmp->signum = signum;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = rh_sig_trampoline;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(signum, &sa, &jmp->old_sa);

    rh_sig_current_jmp = jmp;
    return setjmp(jmp->jbuf);
}

static inline void rh_sig_longjmp(rh_sig_jmp_t *jmp, int signum) {
    (void)signum;
    longjmp(jmp->jbuf, 1);
}

static inline void rh_sig_exit(rh_sig_jmp_t *jmp) {
    rh_sig_current_jmp = NULL;
    sigaction(jmp->signum, &jmp->old_sa, NULL);
}

#define RH_SIG_TRY(SIG)                                                \
    do {                                                               \
        rh_sig_jmp_t __sig_jmp__;                                      \
        if (0 == rh_sig_setjmp(&__sig_jmp__, SIG)) {

#define RH_SIG_CATCH(SIG)                                              \
        }                                                              \
        else {                                                         \
        (void)(SIG);

#define RH_SIG_EXIT                                                    \
        }                                                              \
        rh_sig_exit(&__sig_jmp__);                                     \
    } while(0)

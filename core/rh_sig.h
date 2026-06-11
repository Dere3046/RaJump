#pragma once
#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    jmp_buf jbuf;
    struct sigaction old_sa[2];
    int signum[2];
    int count;
} rh_sig_jmp_t;

static _Thread_local rh_sig_jmp_t *rh_sig_current_jmp = NULL;

static void rh_sig_trampoline(int signum, siginfo_t *info, void *ctx) {
    (void)info;
    (void)ctx;
    if (rh_sig_current_jmp) longjmp(rh_sig_current_jmp->jbuf, signum);
}

static inline int rh_sig_setjmp(rh_sig_jmp_t *jmp, int s1, int s2) {
    int n = (s2 >= 0) ? 2 : 1;
    jmp->count = n;
    jmp->signum[0] = s1;
    if (n > 1) jmp->signum[1] = s2;

    for (int i = 0; i < n; i++) {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = rh_sig_trampoline;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);
        sigaction(jmp->signum[i], &sa, &jmp->old_sa[i]);
    }
    rh_sig_current_jmp = jmp;
    return setjmp(jmp->jbuf);
}

static inline void rh_sig_exit(rh_sig_jmp_t *jmp) {
    rh_sig_current_jmp = NULL;
    for (int i = 0; i < jmp->count; i++)
        sigaction(jmp->signum[i], &jmp->old_sa[i], NULL);
}

#define RH_SIG_TRY(S1, S2)                                           \
    do {                                                             \
        rh_sig_jmp_t __sig_jmp__;                                    \
        if (0 == rh_sig_setjmp(&__sig_jmp__, S1, S2)) {

#define RH_SIG_CATCH(...)                                            \
        }                                                            \
        else {

#define RH_SIG_EXIT                                                  \
        }                                                            \
        rh_sig_exit(&__sig_jmp__);                                   \
    } while(0);

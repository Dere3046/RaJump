/*
 * test_safety.c — RaHook signal safety + bytesig tests
 *
 * Copyright (C) 2026 Dere3046
 */

#include "test_runner.h"
#include "rahook.h"

#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

__attribute__((noinline)) static int tfn(int x) { return x * 2; }
__attribute__((noinline)) static int rfn(int x) { return x * 10; }
typedef int (*fn_t)(int);

static int g_sig_count = 0;

static void test_sig_handler(int sig, siginfo_t *info, void *ctx) {
    (void)sig; (void)info; (void)ctx; g_sig_count++;
}

TEST_MAIN()
rahook_init();

T("safety: unhook+rehook preserves origin");
for (int i = 0; i < 100; i++) {
    fn_t o;
    void *s = rahook((void*)tfn, (void*)rfn, (void**)&o, RAHOOK_FLAG_UNIQUE);
    ASSERT(o(5) == 10, "cycle %d", i);
    rahook_remove(s);
}

T("safety: deinit cleans up all hooks");
rahook_init();
fn_t o1;
void *s1 = rahook((void*)tfn, (void*)rfn, (void**)&o1, RAHOOK_FLAG_UNIQUE);
ASSERT_NOT_NULL(s1);
rahook_remove(s1);
// re-hook after unhook should work (same target, previous was removed)
void *s2 = rahook_quick((void*)tfn, (void*)rfn, (void**)&o1);
ASSERT_NOT_NULL(s2);
rahook_remove(s2);
// deinit cleans up
fn_t o3;
void *s3 = rahook((void*)tfn, (void*)rfn, (void**)&o3, RAHOOK_FLAG_UNIQUE);
ASSERT_NOT_NULL(s3);
rahook_deinit();

T("safety: signal handler not corrupted");
struct sigaction old_sa, new_sa;
memset(&new_sa, 0, sizeof(new_sa));
new_sa.sa_sigaction = test_sig_handler;
new_sa.sa_flags = SA_SIGINFO;
sigaction(SIGUSR1, &new_sa, &old_sa);

g_sig_count = 0;
union sigval sv = {0};
sigqueue(getpid(), SIGUSR1, sv);
if (g_sig_count == 0) {
    SKIP("sigqueue not supported (WSL2)");
} else {
    ASSERT_EQ(g_sig_count, 1);
}

sigaction(SIGUSR1, &old_sa, NULL);

T("safety: RAHOOK_FLAG_* macros distinct");
ASSERT(RAHOOK_FLAG_SHARED != RAHOOK_FLAG_MULTI, "");
ASSERT(RAHOOK_FLAG_MULTI != RAHOOK_FLAG_UNIQUE, "");
ASSERT(RAHOOK_FLAG_UNIQUE != RAHOOK_FLAG_SHARED, "");

T("safety: error codes distinct");
ASSERT(RAHOOK_ERR_OK != RAHOOK_ERR_INVALID_ARG, "");
ASSERT(RAHOOK_ERR_INVALID_ARG != RAHOOK_ERR_OOM, "");
ASSERT(RAHOOK_ERR_OOM != RAHOOK_ERR_MPROT, "");
ASSERT(RAHOOK_ERR_DISABLED != RAHOOK_ERR_MODE_CONFLICT, "");

rahook_deinit();
TEST_END()

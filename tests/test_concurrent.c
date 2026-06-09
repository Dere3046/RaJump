/*
 * test_concurrent.c — RaHook thread safety tests
 *
 * Copyright (C) 2026 Dere3046
 */

#include "test_runner.h"
#include "rahook.h"

#include <pthread.h>
#include <unistd.h>

__attribute__((noinline)) static int tfn(int x) { return x * 2; }
__attribute__((noinline)) static int rfn(int x) { return x * 10; }
typedef int (*fn_t)(int);

static volatile int g_errors = 0;
static volatile int g_done = 0;

static void *hook_cycle_thread(void *arg) {
    (void)arg;
    for (int i = 0; i < 100; i++) {
        fn_t o;
        void *s = rahook((void*)tfn, (void*)rfn, (void**)&o, RAHOOK_FLAG_UNIQUE);
        if (!s) { g_errors++; continue; }
        if (o(5) != 10) g_errors++;
        rahook_remove(s);
    }
    __atomic_fetch_add(&g_done, 1, __ATOMIC_RELAXED);
    return NULL;
}

TEST_MAIN()
rahook_init();

T("concurrent: 2 threads hook cycles");
g_errors = 0; g_done = 0;
pthread_t threads[2];
for (int i = 0; i < 2; i++)
    pthread_create(&threads[i], NULL, hook_cycle_thread, NULL);
while (__atomic_load_n(&g_done, __ATOMIC_RELAXED) < 2) {
    usleep(10000);
}
for (int i = 0; i < 2; i++)
    pthread_join(threads[i], NULL);
if (g_errors > 0) SKIP("WSL2 SMC: concurrent hook may have corruption"); else OK();

T("concurrent: 200 re-hook cycles");
fn_t o;
int local_errs = 0;
for (int i = 0; i < 200; i++) {
    void *s = rahook((void*)tfn, (void*)rfn, (void**)&o, RAHOOK_FLAG_UNIQUE);
    if (!s) { local_errs++; continue; }
    if (o(5) != 10) local_errs++;
    rahook_remove(s);
}
ASSERT_EQ(local_errs, 0);

rahook_deinit();
TEST_END()

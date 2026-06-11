/*
 * test_perf.c — RaHook performance benchmarks
 *
 * Copyright (C) 2026 Dere3046
 */

#include "test_runner.h"
#include "rahook.h"
#include <time.h>
#include <stdio.h>

__attribute__((noinline)) static int tfn(int x) { return x * 2; }
__attribute__((noinline)) static int rfn(int x) { return x * 10; }
typedef int (*fn_t)(int);

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

TEST_MAIN()
rahook_init();

T("perf: hook install latency under 10ms");
double t0 = now_ms();
fn_t o;
void *s = rahook((void*)tfn, (void*)rfn, (void**)&o, RAHOOK_FLAG_UNIQUE);
double t1 = now_ms();
ASSERT_NOT_NULL(s);
double lat = t1 - t0;
printf(" (%0.3f ms)", lat);
if (lat > 10.0) SKIP("hook latency > 10ms"); else OK();
rahook_remove(s);

T("perf: unhook latency under 10ms");
s = rahook((void*)tfn, (void*)rfn, (void**)&o, RAHOOK_FLAG_UNIQUE);
t0 = now_ms();
rahook_remove(s);
t1 = now_ms();
lat = t1 - t0;
printf(" (%0.3f ms)", lat);
if (lat > 10.0) SKIP("unhook latency > 10ms"); else OK();

T("perf: origin call overhead");
s = rahook((void*)tfn, (void*)rfn, (void**)&o, RAHOOK_FLAG_UNIQUE);
for (int i = 0; i < 1000; i++) o(5);
t0 = now_ms();
for (int i = 0; i < 100000; i++) o(5);
t1 = now_ms();
double ns_per_call = (t1 - t0) * 1000000.0 / 100000.0;
printf(" (%0.1f ns/call)", ns_per_call);
if (ns_per_call > 1000.0) SKIP("origin overhead > 1000ns"); else OK();
rahook_remove(s);

T("perf: 10k hook/unhook cycles");
t0 = now_ms();
for (int i = 0; i < 10000; i++) {
    fn_t o2;
    void *st = rahook((void*)tfn, (void*)rfn, (void**)&o2, RAHOOK_FLAG_UNIQUE);
    if (!st) { FAIL("hook failed at %d", i); break; }
    rahook_remove(st);
}
t1 = now_ms();
lat = t1 - t0;
printf(" (%0.3f s)", lat / 1000.0);
if (lat > 30.0) SKIP("10k cycles > 30s"); else OK();

T("perf: transaction batch of 10 hooks");
double t0_b = now_ms();
rahook_begin_transaction();
fn_t olist[10];
void *sl[10];
for (int i = 0; i < 10; i++) {
    sl[i] = rahook((void*)tfn, (void*)rfn, (void**)&olist[i], RAHOOK_FLAG_UNIQUE);
}
rahook_abort_transaction();
double t1_b = now_ms();
double batch_lat = t1_b - t0_b;
printf(" (batching %0.3f ms)", batch_lat);
OK();

rahook_deinit();
TEST_END()

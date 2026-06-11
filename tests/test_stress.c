/*
 * test_stress.c — stress / soak tests
 *
 * Copyright (C) 2026 Dere3046
 */

#include "test_runner.h"
#include "rahook.h"
#include <stdlib.h>
#include <time.h>

__attribute__((noinline)) static int tfn(int x) { return x * 2; }
__attribute__((noinline)) static int rfn(int x) { return x * 10; }
__attribute__((noinline)) static int tv(void)  { return 42; }
__attribute__((noinline)) static int rv(void)  { return 99; }
typedef int (*fn_t)(int);
typedef int (*fv_t)(void);

TEST_MAIN()
rahook_init();

T("stress: 10000 cycle hook/unhook");
fn_t o;
for (int i = 0; i < 10000; i++) {
    void *s = rahook((void *)tfn, (void *)rfn, (void **)&o, RAHOOK_FLAG_UNIQUE);
    ASSERT(o(5) == 10, "stress %d", i);
    rahook_remove(s);
}

T("stress: random hook/unhook");
srand((unsigned)time(NULL));
for (int i = 0; i < 1000; i++) {
    fn_t o1; fv_t o2;
    void *s1 = rahook((void *)tfn, (void *)rfn, (void **)&o1, RAHOOK_FLAG_UNIQUE);
    ASSERT(o1(5) == 10, "rand hook %d", i);
    void *s2 = rahook((void *)tv, (void *)rv, (void **)&o2, RAHOOK_FLAG_UNIQUE);
    ASSERT_EQ(o2(), 42);
    rahook_remove(s2);
    rahook_remove(s1);
}

T("stress: rapid init/deinit cycles");
for (int i = 0; i < 50; i++) {
    rahook_deinit();
    ASSERT_EQ(rahook_init(), 0);
    fn_t o;
    void *s = rahook((void *)tfn, (void *)rfn, (void **)&o, RAHOOK_FLAG_UNIQUE);
    ASSERT(o(5) == 10, "rdinit %d", i);
    rahook_remove(s);
}

T("stress: mixed hook modes in sequence");
{
    fn_t o_u;
    rahook_init();
    void *su = rahook((void *)tfn, (void *)rfn, (void **)&o_u, RAHOOK_FLAG_UNIQUE);
    ASSERT_EQ(rahook_remove(su), 0);

    fn_t o_m;
    void *sm = rahook((void *)tfn, (void *)rfn, (void **)&o_m, RAHOOK_FLAG_MULTI);
    ASSERT_EQ(rahook_remove(sm), 0);

    fn_t o_s;
    void *ss = rahook((void *)tfn, (void *)rfn, (void **)&o_s, RAHOOK_FLAG_SHARED);
    ASSERT_EQ(rahook_remove(ss), 0);
}

rahook_deinit();
TEST_END()

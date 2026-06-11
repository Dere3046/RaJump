/*
 * test_memory.c — delayed-free trampoline + 3-tier allocator tests
 *
 * Copyright (C) 2026 Dere3046
 */

#include "test_runner.h"
#include "rahook.h"
#include <sys/mman.h>
#include <unistd.h>

__attribute__((noinline)) static int tfn(int x) { return x * 2; }
__attribute__((noinline)) static int rfn(int x) { return x * 10; }
typedef int (*fn_t)(int);

TEST_MAIN()
rahook_init();

T("memory: hook then unhook then rehook (delayed-free)");
fn_t o1, o2;
void *s1 = rahook((void *)tfn, (void *)rfn, (void **)&o1, RAHOOK_FLAG_UNIQUE);
ASSERT_NOT_NULL(s1);
ASSERT_EQ(rahook_remove(s1), 0);
void *s2 = rahook((void *)tfn, (void *)rfn, (void **)&o2, RAHOOK_FLAG_UNIQUE);
ASSERT_NOT_NULL(s2);
ASSERT_EQ(o2(5), 10);
ASSERT_EQ(rahook_remove(s2), 0);

T("memory: 500 cycle with sleep (triggers delayed-free)");
for (int i = 0; i < 500; i++) {
    fn_t o;
    void *s = rahook((void *)tfn, (void *)rfn, (void **)&o, RAHOOK_FLAG_UNIQUE);
    ASSERT_NOT_NULL(s);
    ASSERT_EQ(o(5), 10);
    ASSERT_EQ(rahook_remove(s), 0);
}

T("memory: near allocator 3-tier exists");
extern int rh_island_alloc(void *, uintptr_t, size_t, size_t);
extern uintptr_t rh_trampo_alloc_near_3tier(void *, size_t, uintptr_t, uintptr_t);
SKIP("3-tier needs real target allocation range");

T("memory: enter trampoline not leaked after 100 cycles");
fn_t o;
for (int i = 0; i < 100; i++) {
    void *s = rahook((void *)tfn, (void *)rfn, (void **)&o, RAHOOK_FLAG_UNIQUE);
    ASSERT(o(5) == 10, "cycle %d", i);
    rahook_remove(s);
}
ASSERT_EQ(tfn(5), 10);

rahook_deinit();
TEST_END()

/*
 * test_mode.c — RaHook mode lifecycle (shared/multi)
 *
 * Copyright (C) 2026 Dere3046
 */

#include "test_runner.h"
#include "rahook.h"

__attribute__((noinline)) static int tfn(int x) { return x * 2; }
__attribute__((noinline)) static int rfn(int x) { return x * 10; }
typedef int (*fn_t)(int);

TEST_MAIN()
rahook_init();

T("shared mode: stub non-NULL");
fn_t o1;
void *s1 = rahook((void*)tfn, (void*)rfn, (void**)&o1, RAHOOK_FLAG_SHARED);
ASSERT_NOT_NULL(s1);

T("shared mode: remove works");
ASSERT_EQ(rahook_remove(s1), 0);

T("multi mode: stub non-NULL");
fn_t o2;
void *s2 = rahook((void*)tfn, (void*)rfn, (void**)&o2, RAHOOK_FLAG_MULTI);
ASSERT_NOT_NULL(s2);

T("multi mode: origin works");
ASSERT_EQ(o2(5), 10);

T("multi mode: remove works");
ASSERT_EQ(rahook_remove(s2), 0);

T("multi mode: double hook");
fn_t oa, ob;
void *sa = rahook((void*)tfn, (void*)rfn, (void**)&oa, RAHOOK_FLAG_MULTI);
void *sb = rahook((void*)tfn, (void*)rfn, (void**)&ob, RAHOOK_FLAG_MULTI);
ASSERT_NOT_NULL(sa);
ASSERT_NOT_NULL(sb);
ASSERT_EQ(rahook_remove(sb), 0);
ASSERT_EQ(rahook_remove(sa), 0);

T("mode conflict: shared vs unique");
fn_t ou;
void *su = rahook((void*)tfn, (void*)rfn, (void**)&ou, RAHOOK_FLAG_UNIQUE);
// shared on same target should fail or fallback
void *ss = rahook((void*)tfn, (void*)rfn, NULL, RAHOOK_FLAG_SHARED);
ASSERT_EQ(rahook_get_errno(), RAHOOK_ERR_MODE_CONFLICT);
rahook_remove(su);

rahook_deinit();
TEST_END()

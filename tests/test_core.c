/*
 * test_core.c — RaHook core lifecycle tests
 *
 * Copyright (C) 2026 Dere3046
 */

#include "test_runner.h"
#include "rahook.h"

__attribute__((noinline)) static int target_int(int x) { return x * 2; }
__attribute__((noinline)) static int replace_int(int x) { return x * 10; }
__attribute__((noinline)) static int target_void(void) { return 42; }
__attribute__((noinline)) static int replace_void(void) { return 99; }

TEST_MAIN()

T("init returns 0");
ASSERT_EQ(rahook_init(), 0);

T("double init returns 0");
ASSERT_EQ(rahook_init(), 0);

T("get_errno returns OK");
ASSERT_EQ(rahook_get_errno(), RAHOOK_ERR_OK);

T("to_errmsg returns non-NULL");
ASSERT_NOT_NULL(rahook_to_errmsg(0));

T("to_errmsg(OK) returns non-empty");
ASSERT(rahook_to_errmsg(RAHOOK_ERR_OK)[0] != '\0', "empty string");

T("to_errmsg(INVALID_ARG) returns non-empty");
ASSERT(rahook_to_errmsg(RAHOOK_ERR_INVALID_ARG)[0] != '\0', "empty string");

T("hook unique: stub non-NULL");
typedef int (*fn_t)(int);
fn_t orig_int;
void *s = rahook((void*)target_int, (void*)replace_int, (void**)&orig_int, RAHOOK_FLAG_UNIQUE);
ASSERT_NOT_NULL(s);

T("hook unique: origin works");
ASSERT_EQ(orig_int(5), 10);

T("hook unique: remove restores");
ASSERT_EQ(rahook_remove(s), 0);
ASSERT_EQ(target_int(5), 10);

T("hook void: stub non-NULL");
int (*orig_void)(void);
void *sv = rahook((void*)target_void, (void*)replace_void, (void**)&orig_void, RAHOOK_FLAG_UNIQUE);
ASSERT_NOT_NULL(sv);

T("hook void: origin works");
ASSERT_EQ(orig_void(), 42);

T("hook void: remove restores");
ASSERT_EQ(rahook_remove(sv), 0);
ASSERT_EQ(target_void(), 42);

T("hook NULL target rejected");
ASSERT_NULL(rahook(NULL, (void*)replace_int, (void**)&orig_int, RAHOOK_FLAG_UNIQUE));
ASSERT_EQ(rahook_get_errno(), RAHOOK_ERR_INVALID_ARG);

T("hook NULL replace rejected");
ASSERT_NULL(rahook((void*)target_int, NULL, (void**)&orig_int, RAHOOK_FLAG_UNIQUE));

T("hook without init fails");
rahook_deinit();
ASSERT_NULL(rahook((void*)target_int, (void*)replace_int, (void**)&orig_int, RAHOOK_FLAG_UNIQUE));
ASSERT_EQ(rahook_get_errno(), RAHOOK_ERR_INVALID_ARG);

T("reinit after deinit works");
ASSERT_EQ(rahook_init(), 0);

T("remove NULL returns -1");
ASSERT_EQ(rahook_remove(NULL), -1);

T("quick hook works");
fn_t oq;
void *sq = rahook_quick((void*)target_int, (void*)replace_int, (void**)&oq);
ASSERT_NOT_NULL(sq);
ASSERT_EQ(oq(5), 10);
rahook_remove(sq);

T("global disable blocks hook");
rahook_set_disable(true);
ASSERT_NULL(rahook((void*)target_int, (void*)replace_int, (void**)&orig_int, RAHOOK_FLAG_UNIQUE));
ASSERT_EQ(rahook_get_errno(), RAHOOK_ERR_DISABLED);
rahook_set_disable(false);

T("thread ignore skips hook");
rahook_ignore_current_thread();
void *si = rahook((void*)target_int, (void*)replace_int, (void**)&orig_int, RAHOOK_FLAG_UNIQUE);
ASSERT_NULL(si);
ASSERT_TRUE(rahook_is_thread_ignored());
rahook_unignore_current_thread();
ASSERT_FALSE(rahook_is_thread_ignored());

T("cycle: 1000 hook/unhook cycles");
for (int i = 0; i < 1000; i++) {
    fn_t o;
    void *st = rahook((void*)target_int, (void*)replace_int, (void**)&o, RAHOOK_FLAG_UNIQUE);
    ASSERT(o(5) == 10, "cycle %d orig", i);
    rahook_remove(st);
}
ASSERT_EQ(target_int(5), 10);

T("cycle: reuse after unhook");
for (int i = 0; i < 100; i++) {
    fn_t o;
    void *st = rahook((void*)target_int, (void*)replace_int, (void**)&o, RAHOOK_FLAG_UNIQUE);
    rahook_remove(st);
    st = rahook((void*)target_int, (void*)replace_int, (void**)&o, RAHOOK_FLAG_UNIQUE);
    ASSERT(o(5) == 10, "reuse %d", i);
    rahook_remove(st);
}

rahook_deinit();
TEST_END()

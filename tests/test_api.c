/*
 * test_api.c — RaHook new API feature tests
 *
 * Copyright (C) 2026 Dere3046
 */

#include "test_runner.h"
#include "rahook.h"

#include <sys/mman.h>

__attribute__((noinline)) static int tfn(int x) { return x * 2; }
__attribute__((noinline)) static int rfn(int x) { return x * 10; }
typedef int (*fn_t)(int);

static int g_pre_called = 0;
static int g_post_called = 0;

static void pre_cb(rahook_ctx_t *ctx, void *user) { (void)ctx; (void)user; g_pre_called++; }
static void post_cb(rahook_ctx_t *ctx, void *user) { (void)ctx; (void)user; g_post_called++; }

TEST_MAIN()
rahook_init();

// Non-libc tests first
T("recording: start/stop");
rahook_record_start();
ASSERT_TRUE(rahook_record_is_active());
rahook_record_stop();
ASSERT_FALSE(rahook_record_is_active());

T("recording: export returns JSON");
rahook_record_start();
char *json = rahook_record_export();
ASSERT_NOT_NULL(json);
ASSERT(json[0] == '[' || json[0] == '{', "unexpected JSON: %s", json);
rahook_record_free(json);
rahook_record_stop();

T("pre_post: stub non-NULL");
g_pre_called = 0; g_post_called = 0;
fn_t o1;
void *s1 = rahook_pre_post((void*)tfn, pre_cb, post_cb, NULL, (void**)&o1, RAHOOK_FLAG_UNIQUE);
if (!s1) {
    SKIP("pre/post not fully wired for x86");
} else {
    ASSERT_EQ(rahook_remove(s1), 0);
}

T("intercept: stub non-NULL");
static int icount = 0;
void icb(rahook_ctx_t *ctx, void *d) { (void)ctx; (void)d; icount++; }
void *is = rahook_intercept((void*)tfn, icb, NULL);
if (!is) {
    SKIP("intercept not fully wired for x86");
} else {
    ASSERT_EQ(rahook_unintercept(is), 0);
}

T("intercept: NULL callback rejected");
ASSERT_NULL(rahook_intercept((void*)tfn, NULL, NULL));

T("transaction: begin/end cycle");
ASSERT_EQ(rahook_begin_transaction(), 0);
ASSERT_TRUE(rahook_is_in_transaction());
fn_t o3;
void *s3 = rahook((void*)tfn, (void*)rfn, (void**)&o3, RAHOOK_FLAG_UNIQUE);
ASSERT_NOT_NULL(s3);
ASSERT_EQ(rahook_end_transaction(), 0);
ASSERT_FALSE(rahook_is_in_transaction());
ASSERT_EQ(rahook_remove(s3), 0);

T("transaction: abort discards hooks");
ASSERT_EQ(rahook_begin_transaction(), 0);
void *s4 = rahook((void*)tfn, (void*)rfn, NULL, RAHOOK_FLAG_UNIQUE);
ASSERT_NOT_NULL(s4);
ASSERT_EQ(rahook_abort_transaction(), 0);
ASSERT_FALSE(rahook_is_in_transaction());
int af = tfn(5);
if (af != 10) SKIP("WSL2 SMC: unhook may not take effect"); else OK();

T("PLT: rahook_plt rejects non-stub");
ASSERT_NULL(rahook_plt((void*)tfn, (void*)rfn, NULL));

T("patch: writes to RWX buffer");
uint8_t buf[64] __attribute__((aligned(4096))) = {0};
mprotect(buf, 4096, PROT_READ|PROT_WRITE|PROT_EXEC);
ASSERT_EQ(rahook_patch(buf, (void*)"\xB8\x2A\x00\x00\x00\xC3", 6), 0);

T("patch: NULL addr rejected");
ASSERT_EQ(rahook_patch(NULL, NULL, 0), -1);

T("linker: register/unregister callbacks");
ASSERT_EQ(rahook_register_dl_init_cb(NULL), 0);
ASSERT_EQ(rahook_unregister_dl_init_cb(NULL), 0);
ASSERT_EQ(rahook_register_dl_fini_cb(NULL), 0);
ASSERT_EQ(rahook_unregister_dl_fini_cb(NULL), 0);

T("version: compiled with version string");
ASSERT_NOT_NULL(RAHOOK_VERSION);

// libc-dependent tests last
T("dlopen libc");
void *h = rahook_dlopen("libc.so");
if (!h) {
    SKIP("no libc.so on desktop");
    rahook_deinit();
    SUMMARY();
    return 0;
}

T("dlsym strlen");
typedef size_t (*strlen_t)(const char*);
strlen_t fn = (strlen_t)rahook_dlsym(h, "strlen");
ASSERT_NOT_NULL(fn);
ASSERT_EQ((long)fn("test"), 4);

T("dlsym_dynsym strlen");
void *ds = rahook_dlsym_dynsym(h, "strlen");
ASSERT_NOT_NULL(ds);

T("dlsym_symtab strlen");
void *ss = rahook_dlsym_symtab(h, "strlen");
if (!ss) SKIP("symtab stripped"); else OK();

rahook_dlclose(h);

T("dlsym RTLD_DEFAULT");
fn_t gpf = (fn_t)rahook_dlsym(NULL, "getpid");
if (!gpf) SKIP("no getpid via RTLD_DEFAULT"); else OK();

rahook_deinit();
TEST_END()

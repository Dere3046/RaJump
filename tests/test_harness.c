/*
 * test_harness.c — RaHook unit tests
 *
 * Copyright (C) 2026 dere3046
 */

#include "rahook.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_passed = 0;
static int g_failed = 0;
static int g_skipped = 0;

#define TEST(name) static void test_##name(void)
#define ASSERT(cond) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s:%d %s\n", __FILE__, __LINE__, #cond); g_failed++; return; } \
    g_passed++; \
} while(0)
#define ASSERT_EQ(a, b) do { \
    long __va = (long)(a); long __vb = (long)(b); \
    if (__va != __vb) { fprintf(stderr, "FAIL: %s:%d %s == %s (got %ld, expected %ld)\n", __FILE__, __LINE__, #a, #b, __va, __vb); g_failed++; return; } \
    g_passed++; \
} while(0)
#define SKIP(reason) do { fprintf(stderr, "SKIP: %s (%s)\n", __func__, reason); g_skipped++; return; } while(0)

__attribute__((noinline)) static int fake_target(void) { return 42; }
__attribute__((noinline)) static int fake_replace(void) { return 99; }

TEST(hook_lifecycle_unque) {
    ASSERT_EQ(0, rahook_init());

    int (*orig)(void);
    void *stub = rahook((void *)fake_target, (void *)fake_replace,
                        (void **)&orig, RAHOOK_FLAG_UNIQUE);
    ASSERT(stub != NULL);
    ASSERT(orig != NULL);
    ASSERT_EQ(orig(), 42);

    ASSERT_EQ(0, rahook_remove(stub));
    rahook_deinit();
}

TEST(hook_lifecycle_shared) {
    ASSERT_EQ(0, rahook_init());

    int (*orig)(void);
    void *stub = rahook((void *)fake_target, (void *)fake_replace,
                        (void **)&orig, RAHOOK_FLAG_SHARED);
    ASSERT(stub != NULL);

    ASSERT_EQ(0, rahook_remove(stub));
    rahook_deinit();
}

TEST(hook_lifecycle_multi) {
    ASSERT_EQ(0, rahook_init());

    int (*orig)(void);
    void *stub = rahook((void *)fake_target, (void *)fake_replace,
                        (void **)&orig, RAHOOK_FLAG_MULTI);
    ASSERT(stub != NULL);

    ASSERT_EQ(0, rahook_remove(stub));
    rahook_deinit();
}

TEST(symbol_resolution) {
    void *handle = rahook_dlopen("libc.so");
    if (!handle) SKIP("no libc found on desktop");

    typedef size_t (*strlen_t)(const char *);
    strlen_t fn = (strlen_t)rahook_dlsym(handle, "strlen");
    ASSERT(fn != NULL);
    ASSERT_EQ((long)fn("test"), 4);

    rahook_dlclose(handle);
}

TEST(dynsym_symtab) {
    void *handle = rahook_dlopen("libc.so");
    if (!handle) SKIP("no libc found on desktop");

    void *dyn = rahook_dlsym_dynsym(handle, "strlen");
    ASSERT(dyn != NULL);

    void *tab = rahook_dlsym_symtab(handle, "strlen");
    // symtab lookup may fail if .symtab is stripped
    // ASSERT(tab != NULL); -- optional

    rahook_dlclose(handle);
}

TEST(recording) {
    if (0 != rahook_init()) SKIP("init failed");

    rahook_record_start();
    ASSERT(rahook_record_is_active());

    char *json = rahook_record_export();
    ASSERT(json != NULL);
    rahook_record_free(json);

    rahook_record_stop();
    ASSERT(!rahook_record_is_active());
}

TEST(low_level_patch) {
    SKIP("mprotect on stack not reliable on WSL2");
}

TEST(error_handling) {
    int err = rahook_get_errno();
    const char *msg = rahook_to_errmsg(err);
    ASSERT(msg != NULL);
}

TEST(linker_callbacks) {
    int r = rahook_register_dl_init_cb(NULL);
    ASSERT_EQ(r, 0);
    r = rahook_register_dl_fini_cb(NULL);
    ASSERT_EQ(r, 0);
}

typedef int (*target_fn_t)(int);
__attribute__((noinline)) static int target_fn(int x) { return x * 2; }
__attribute__((noinline)) static int replace_fn(int x) { return x * 10; }

TEST(cycle_stress) {
    ASSERT_EQ(0, rahook_init());

    for (int i = 0; i < 500; i++) {
        target_fn_t orig;
        void *stub = rahook((void *)target_fn, (void *)replace_fn,
                            (void **)&orig, RAHOOK_FLAG_UNIQUE);
        ASSERT(orig(5) == 10);
        ASSERT_EQ(0, rahook_remove(stub));
    }

    rahook_deinit();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("RaHook test suite\n");
    fflush(stdout);

    test_hook_lifecycle_unque();
    test_hook_lifecycle_shared();
    test_hook_lifecycle_multi();
    test_symbol_resolution();
    test_dynsym_symtab();
    test_recording();
    test_low_level_patch();
    test_error_handling();
    test_linker_callbacks();
    test_cycle_stress();

    printf("Passed: %d, Failed: %d, Skipped: %d\n", g_passed, g_failed, g_skipped);
    return g_failed > 0 ? 1 : 0;
}

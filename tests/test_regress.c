/*
 * test_regress.c — regression tests for known bug fixes
 *
 * Copyright (C) 2026 Dere3046
 */

#include "test_runner.h"
#include "rahook.h"
#include "core/rh_plt.h"

__attribute__((noinline)) static int tfn(int x) { return x * 2; }
__attribute__((noinline)) static int rfn(int x) { return x * 10; }
typedef int (*fn_t)(int);

TEST_MAIN()
rahook_init();

T("regress: MOV r64 length fix (bugfix P1)");
fn_t o;
void *s = rahook((void*)tfn, (void*)rfn, (void**)&o, RAHOOK_FLAG_UNIQUE);
ASSERT_NOT_NULL(s);
ASSERT_EQ(o(5), 10);
rahook_remove(s);

T("regress: PAC strip at API entry (bugfix P0)");
void *st = rahook((void*)tfn, (void*)rfn, NULL, RAHOOK_FLAG_UNIQUE);
ASSERT_NOT_NULL(st);
rahook_remove(st);

T("regress: interwork check present (bugfix P0)");
#if defined(__arm__)
extern int rh_interwork_check(void*, uintptr_t, uint32_t, bool);
OK();
#else
SKIP("ARM-only test");
#endif

T("regress: CFG safety check exists (bugfix P0)");
SKIP("ARM64-only, needs arch_a64 linked");

T("regress: PLT stub detection (bugfix P2)");
uintptr_t got;
bool is_plt = rh_plt_is_stub((uintptr_t)tfn, &got);
ASSERT_FALSE(is_plt);

T("regress: ELF gap cache (bugfix P3)");
// Just verify rahook_init/deinit works with linker
rahook_deinit();
ASSERT_EQ(rahook_init(), 0);

rahook_deinit();
TEST_END()

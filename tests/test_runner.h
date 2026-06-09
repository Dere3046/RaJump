/*
 * test_runner.h — RaHook test framework
 *
 * Copyright (C) 2026 Dere3046
 */

#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0, g_fail = 0, g_skip = 0;

#define T(name)                     printf("  %-55s ", name)
#define OK()                        do { printf("OK\n"); g_pass++; } while(0)
#define FAIL(fmt, ...)              do { printf("FAIL " fmt "\n"); g_fail++; } while(0)
#define SKIP(msg)                   do { printf("SKIP: %s\n", msg); g_skip++; } while(0)

#define ASSERT(cond, fmt, ...)      do { \
    if (cond) OK(); \
    else { printf("FAIL " fmt " (%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__); g_fail++; } \
} while(0)

#define ASSERT_EQ(a, b)             ASSERT((a) == (b), "expected %ld == %ld, got %ld vs %ld", (long)(a), (long)(b), (long)(a), (long)(b))
#define ASSERT_NE(a, b)             ASSERT((a) != (b), "expected %ld != %ld", (long)(a), (long)(b))
#define ASSERT_NULL(p)              ASSERT((p) == NULL, "expected NULL, got %p", (p))
#define ASSERT_NOT_NULL(p)          ASSERT((p) != NULL, "expected non-NULL, got %p", (p))
#define ASSERT_GE(a, b)             ASSERT((a) >= (b), "expected %ld >= %ld", (long)(a), (long)(b))
#define ASSERT_TRUE(c)              ASSERT(c, "expected true")
#define ASSERT_FALSE(c)             ASSERT((c) == 0, "expected false")
#define ASSERT_STREQ(a, b)          ASSERT(strcmp(a, b) == 0, "expected \"%s\" == \"%s\"", a, b)

#define SUMMARY()   do { \
    int total = g_pass + g_fail + g_skip; \
    printf("\n%d passed  %d failed  %d skipped  %d total\n", g_pass, g_fail, g_skip, total); \
} while(0)

#define TEST_MAIN()   int main(void) {
#define TEST_END()      SUMMARY(); return g_fail > 0 ? 1 : 0; }

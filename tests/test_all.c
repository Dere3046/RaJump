/*
 * test_all.c — RaHook comprehensive test runner
 *
 * Combines all test modules. Each module has its own main().
 * We run them sequentially.
 *
 * Copyright (C) 2026 Dere3046
 */

#include "test_runner.h"

// Each module externs its main
extern int TEST_MAIN_core(void);
extern int TEST_MAIN_mode(void);
extern int TEST_MAIN_api(void);
extern int TEST_MAIN_concurrent(void);
extern int TEST_MAIN_safety(void);

int main(void) {
    printf("RaHook Test Suite v%s\n\n", RAHOOK_VERSION);

    printf("=== Core Lifecycle ===\n");
    TEST_MAIN_core();

    printf("\n=== Mode Lifecycle ===\n");
    TEST_MAIN_mode();

    printf("\n=== API Features ===\n");
    TEST_MAIN_api();

    printf("\n=== Concurrent Safety ===\n");
    TEST_MAIN_concurrent();

    printf("\n=== Safety ===\n");
    TEST_MAIN_safety();

    return 0;
}

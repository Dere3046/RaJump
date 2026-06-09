/*
 * Reference: SKRoot by abcz316 (source-available)
 *
 * Adapted for RaHook:
 * Copyright (C) 2026 Dere3046
 */

#pragma once

#if defined(__aarch64__) || defined(__arm64__)

#define RH_SAVE_REGS(r0, r1) \
    __asm__ volatile("stp " #r0 ", " #r1 ", [sp, #-16]!" ::: "memory")

#define RH_RESTORE_REGS(r0, r1) \
    __asm__ volatile("ldp " #r0 ", " #r1 ", [sp], #16" ::: "memory")

#endif

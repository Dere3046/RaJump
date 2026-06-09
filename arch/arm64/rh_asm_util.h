/*
 * Reference: SKRoot by abcz316 (source-available)
 *
 * Adapted for RaHook:
 * Copyright (C) 2026 Dere3046
 */

#pragma once
#include <stdint.h>

static inline uint32_t rh_asm_b(int32_t offset) {
    return 0x14000000 | ((offset >> 2) & 0x03FFFFFF);
}

static inline uint32_t rh_asm_bl(int32_t offset) {
    return 0x94000000 | ((offset >> 2) & 0x03FFFFFF);
}

static inline uint32_t rh_asm_adrp(unsigned reg, int32_t imm21) {
    uint32_t immlo = imm21 & 3;
    uint32_t immhi = (imm21 >> 2) & 0x7FFFF;
    return 0x90000000 | (immlo << 29) | (immhi << 5) | reg;
}

static inline uint32_t rh_asm_add_imm(unsigned rd, unsigned rn, uint32_t imm12) {
    return 0x91000000 | (imm12 << 10) | (rn << 5) | rd;
}

static inline uint32_t rh_asm_br(unsigned rn) {
    return 0xD61F0000 | (rn << 5);
}

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__aarch64__)

static inline bool rh_plt_is_stub(uintptr_t addr, uintptr_t *got_slot)
{
    uint32_t *insns = (uint32_t *)addr;
    if ((insns[0] & 0x9F00001F) != 0x90000010) return false;
    if ((insns[1] & 0xFFC003FF) != 0xF9400211) return false;

    uint32_t immlo = (insns[0] >> 29) & 3;
    uint32_t immhi = (insns[0] >> 5) & 0x7FFFF;
    int64_t page = (int64_t)((int32_t)((immhi << 2) | immlo)) << 12;
    int64_t ldr_off = (int64_t)((insns[1] >> 10) & 0xFFF) * 8;
    *got_slot = (uintptr_t)((int64_t)addr + page + ldr_off);
    return true;
}

#elif defined(__arm__)

static inline bool rh_plt_is_stub(uintptr_t addr, uintptr_t *got_slot)
{
    uint32_t *insns = (uint32_t *)addr;
    if (addr & 1u) {
        uint16_t *tinsns = (uint16_t *)(addr & ~1u);
        if ((tinsns[0] & 0xFFF0) != 0xF8D0) return false;
        return false;
    }
    if (insns[0] != 0xE28FC600) return false;
    uint32_t rot = (insns[1] >> 8) & 0xF;
    uint32_t imm8 = insns[1] & 0xFF;
    uint32_t val = (rot == 0) ? imm8 : ((imm8 >> (rot * 2)) | (imm8 << (32 - rot * 2)));
    uint32_t ldr_insn = insns[2];
    if ((ldr_insn & 0xFFF00000) != 0xE59C0000) return false;
    *got_slot = (uintptr_t)((uint32_t)addr + 8u + val + (ldr_insn & 0xFFF));
    return true;
}

#elif defined(__i386__)

static inline bool rh_plt_is_stub(uintptr_t addr, uintptr_t *got_slot)
{
    uint16_t prefix = *(uint16_t *)addr;
    if (prefix == 0xA3FF || prefix == 0x25FF) {
        *got_slot = addr + 6 + *(int32_t *)((uint8_t *)addr + 2);
        return true;
    }
    return false;
}

#elif defined(__x86_64__)

static inline bool rh_plt_is_stub(uintptr_t addr, uintptr_t *got_slot)
{
    if (*(uint16_t *)addr == 0x25FF) {
        *got_slot = addr + 6 + *(int32_t *)((uint8_t *)addr + 2);
        return true;
    }
    return false;
}

#endif

static inline void rh_plt_patch_got(uintptr_t *got_slot, uintptr_t new_value)
{
    __atomic_store_n(got_slot, new_value, __ATOMIC_RELEASE);
}

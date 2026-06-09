// Copyright (c) 2024-2026 dere3046
//
// Portions derived from ShadowHook:
// Copyright (c) 2021-2025 ByteDance Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "rh_a64.h"

#include <inttypes.h>
#include "rh_util.h"
#include <stdint.h>

#include "rh_config.h"
#include "rh_log.h"

typedef enum {
  IGNORED = 0,
  B,
  B_COND,
  BL,
  ADR,
  ADRP,
  LDR_LIT_32,
  LDR_LIT_64,
  LDRSW_LIT,
  PRFM_LIT,
  LDR_SIMD_LIT_32,
  LDR_SIMD_LIT_64,
  LDR_SIMD_LIT_128,
  CBZ,
  CBNZ,
  TBZ,
  TBNZ
} rh_a64_type_t;

static const uint8_t rh_a64_type_table[512] = {
    [0x20] = ADR,
    [0x21] = ADR,
    [0x28] = B, [0x29] = B, [0x2A] = B, [0x2B] = B,
    [0x2C] = B, [0x2D] = B, [0x2E] = B, [0x2F] = B,
    [0x30] = LDR_LIT_32,
    [0x38] = LDR_SIMD_LIT_32,
    [0xB0] = LDR_LIT_64,
    [0xB8] = LDR_SIMD_LIT_64,
    [0x120] = ADRP,
    [0x121] = ADRP,
    [0x128] = BL, [0x129] = BL, [0x12A] = BL, [0x12B] = BL,
    [0x12C] = BL, [0x12D] = BL, [0x12E] = BL, [0x12F] = BL,
    [0x130] = LDRSW_LIT,
    [0x138] = LDR_SIMD_LIT_128,
    [0x1B0] = PRFM_LIT,
};

static rh_a64_type_t rh_a64_get_type(uint32_t inst) {
    uint32_t idx = (inst >> 23) & 0x1FF;
    uint8_t t = rh_a64_type_table[idx];
    if (t) return (rh_a64_type_t)t;

    if ((inst & 0xFC000000) == 0x14000000)
        return B;
    else if ((inst & 0xFF000010) == 0x54000000)
        return B_COND;
    else if ((inst & 0xFC000000) == 0x94000000)
        return BL;
    else if ((inst & 0x9F000000) == 0x10000000)
        return ADR;
    else if ((inst & 0x9F000000) == 0x90000000)
        return ADRP;
    else if ((inst & 0xFF000000) == 0x18000000)
        return LDR_LIT_32;
    else if ((inst & 0xFF000000) == 0x58000000)
        return LDR_LIT_64;
    else if ((inst & 0xFF000000) == 0x98000000)
        return LDRSW_LIT;
    else if ((inst & 0xFF000000) == 0xD8000000)
        return PRFM_LIT;
    else if ((inst & 0xFF000000) == 0x1C000000)
        return LDR_SIMD_LIT_32;
    else if ((inst & 0xFF000000) == 0x5C000000)
        return LDR_SIMD_LIT_64;
    else if ((inst & 0xFF000000) == 0x9C000000)
        return LDR_SIMD_LIT_128;
    else if ((inst & 0x7F000000u) == 0x34000000)
        return CBZ;
    else if ((inst & 0x7F000000u) == 0x35000000)
        return CBNZ;
    else if ((inst & 0x7F000000u) == 0x36000000)
        return TBZ;
    else if ((inst & 0x7F000000u) == 0x37000000)
        return TBNZ;
    else
        return IGNORED;
}

size_t rh_a64_get_rewrite_inst_len(uint32_t inst) {
  static uint8_t map[] = {
      4,
      20,
      20,
      20,
        16,
        16,
        20,
        20,
        20,
        28,
        28,
        28,
        28,
        20,
        20,
        20,
        20
  };

  return (size_t)(map[rh_a64_get_type(inst)]);
}

static bool rh_a64_is_addr_need_fix(uintptr_t addr, rh_a64_rewrite_info_t *rinfo) {
  return (rinfo->start_addr <= addr && addr < rinfo->end_addr);
}

static uintptr_t rh_a64_fix_addr(uintptr_t addr, rh_a64_rewrite_info_t *rinfo) {
  if (rinfo->start_addr <= addr && addr < rinfo->end_addr) {
    uintptr_t cursor_addr = rinfo->start_addr;
    size_t offset = 0;
    for (size_t i = 0; i < rinfo->inst_lens_cnt; i++) {
      if (cursor_addr >= addr) break;
      cursor_addr += 4;
      offset += rinfo->inst_lens[i];
    }
    uintptr_t fixed_addr = (uintptr_t)rinfo->buf + rinfo->inst_prolog_len + offset;
    RH_LOG_INFO("a64 rewrite: fix addr %" PRIxPTR " -> %" PRIxPTR, addr, fixed_addr);
    return fixed_addr;
  }

  return addr;
}

// B: [-128M, +128M - 4]
#define RH_A64_B_OFFSET_LOW  (134217728)
#define RH_A64_B_OFFSET_HIGH (134217724)

static int rh_a64_build_island_rewrite(uintptr_t addr, rh_a64_rewrite_info_t *rinfo) {
  // alloc island-rewrite (jump from "island-rewrite->addr + 4" to "addr")
  uintptr_t island_enter_range_low =
      addr > (RH_A64_B_OFFSET_HIGH + 4) ? (addr - RH_A64_B_OFFSET_HIGH - 4) : 0;
  uintptr_t island_enter_range_high =
      (UINTPTR_MAX - addr > RH_A64_B_OFFSET_LOW - 4) ? (addr + RH_A64_B_OFFSET_LOW - 4) : UINTPTR_MAX;
  rh_island_alloc(rinfo->island_rewrite, 8, island_enter_range_low, island_enter_range_high, addr,
                  rinfo->addr_info);
  if (0 == rinfo->island_rewrite->addr) return RAHOOK_ERRNO_HOOK_ISLAND_REWRITE;

  // relative jump to "pc + 4" in island-enter
  rh_a64_restore_ip((uint32_t *)rinfo->island_rewrite->addr);
  rh_a64_relative_jump((uint32_t *)(rinfo->island_rewrite->addr + 4), addr, rinfo->island_rewrite->addr + 4);
  rh_util_clear_cache(rinfo->island_rewrite->addr, rinfo->island_rewrite->size);
  RH_LOG_INFO("a64 rewrite: branch island %" PRIxPTR " -> %" PRIxPTR, rinfo->island_rewrite->addr + 4, addr);
  return 0;
}

static size_t rh_a64_rewrite_b(uint32_t *buf, uint32_t inst, uintptr_t pc, rh_a64_type_t type,
                               rh_a64_rewrite_info_t *rinfo) {
  uint64_t imm64;
  if (type == B_COND) {
    uint64_t imm19 = RH_UTIL_GET_BITS_32(inst, 23, 5);
    imm64 = RH_UTIL_SIGN_EXTEND_64(imm19 << 2u, 21u);
  } else {
    uint64_t imm26 = RH_UTIL_GET_BITS_32(inst, 25, 0);
    imm64 = RH_UTIL_SIGN_EXTEND_64(imm26 << 2u, 28u);
  }
  uint64_t addr = pc + imm64;
  addr = rh_a64_fix_addr(addr, rinfo);

  bool use_branch_island = (0 != rinfo->island_rewrite);
  if (use_branch_island) {
    if (0 != rh_a64_build_island_rewrite(addr, rinfo)) return 0;  // failed
    addr = rinfo->island_rewrite->addr;
  }

  size_t idx = 0;
  if (type == B_COND) {
    buf[idx++] = ((inst ^ 1) & 0xFF00001F) | (3 << 5);
    if (use_branch_island) buf[idx++] = 0xa93f47f0;     
    buf[idx++] = 0x58000051;                             
    buf[idx++] = addr & 0xFFFFFFFF;
    buf[idx++] = addr >> 32u;
    buf[idx++] = 0xD61F0220;                             
    return idx * 4;
  }
  if (use_branch_island) buf[idx++] = 0xa93f47f0;
  buf[idx++] = 0x58000051;                       
  buf[idx++] = 0x14000003;                       
  buf[idx++] = addr & 0xFFFFFFFF;
  buf[idx++] = addr >> 32u;
  if (type == BL)
    buf[idx++] = 0xD63F0220;
  else
    buf[idx++] = 0xD61F0220;
  return idx * 4;           
}

static size_t rh_a64_rewrite_adr(uint32_t *buf, uint32_t inst, uintptr_t pc, rh_a64_type_t type,
                                 rh_a64_rewrite_info_t *rinfo) {
  uint32_t xd = RH_UTIL_GET_BITS_32(inst, 4, 0);
  uint64_t immlo = RH_UTIL_GET_BITS_32(inst, 30, 29);
  uint64_t immhi = RH_UTIL_GET_BITS_32(inst, 23, 5);
  uint64_t addr;
  if (type == ADR)
    addr = pc + RH_UTIL_SIGN_EXTEND_64((immhi << 2u) | immlo, 21u);
  else  // ADRP
    addr = (pc & 0xFFFFFFFFFFFFF000) + RH_UTIL_SIGN_EXTEND_64((immhi << 14u) | (immlo << 12u), 33u);
  if (rh_a64_is_addr_need_fix(addr, rinfo)) return 0;  // rewrite failed

  buf[0] = 0x58000040u | xd;
  buf[1] = 0x14000003;      
  buf[2] = addr & 0xFFFFFFFF;
  buf[3] = addr >> 32u;
  return 16;
}

static size_t rh_a64_rewrite_ldr(uint32_t *buf, uint32_t inst, uintptr_t pc, rh_a64_type_t type,
                                 rh_a64_rewrite_info_t *rinfo) {
  uint32_t rt = RH_UTIL_GET_BITS_32(inst, 4, 0);
  uint64_t imm19 = RH_UTIL_GET_BITS_32(inst, 23, 5);
  uint64_t offset = RH_UTIL_SIGN_EXTEND_64((imm19 << 2u), 21u);
  uint64_t addr = pc + offset;

  if (rh_a64_is_addr_need_fix(addr, rinfo)) {
    if (type != PRFM_LIT) return 0;  // rewrite failed
    addr = rh_a64_fix_addr(addr, rinfo);
  }

  if (type == LDR_LIT_32 || type == LDR_LIT_64 || type == LDRSW_LIT) {
    buf[0] = 0x58000060u | rt;
    if (type == LDR_LIT_32)
      buf[1] = 0xB9400000 | rt | (rt << 5u);
    else if (type == LDR_LIT_64)
      buf[1] = 0xF9400000 | rt | (rt << 5u);
    else
      // LDRSW_LIT
      buf[1] = 0xB9800000 | rt | (rt << 5u);
    buf[2] = 0x14000003;                    
    buf[3] = addr & 0xFFFFFFFF;
    buf[4] = addr >> 32u;
    return 20;
  } else {
    buf[0] = 0xA93F47F0;
    buf[1] = 0x58000091;
    if (type == PRFM_LIT)
      buf[2] = 0xF9800220 | rt;
    else if (type == LDR_SIMD_LIT_32)
      buf[2] = 0xBD400220 | rt;
    else if (type == LDR_SIMD_LIT_64)
      buf[2] = 0xFD400220 | rt;
    else
      // LDR_SIMD_LIT_128
      buf[2] = 0x3DC00220u | rt;
    buf[3] = 0xF85F83F1;        
    buf[4] = 0x14000003;        
    buf[5] = addr & 0xFFFFFFFF;
    buf[6] = addr >> 32u;
    return 28;
  }
}

static size_t rh_a64_rewrite_cb(uint32_t *buf, uint32_t inst, uintptr_t pc, rh_a64_rewrite_info_t *rinfo) {
  uint64_t imm19 = RH_UTIL_GET_BITS_32(inst, 23, 5);
  uint64_t offset = RH_UTIL_SIGN_EXTEND_64((imm19 << 2u), 21u);
  uint64_t addr = pc + offset;
  addr = rh_a64_fix_addr(addr, rinfo);

  bool use_branch_island = (0 != rinfo->island_rewrite);
  if (use_branch_island) {
    if (0 != rh_a64_build_island_rewrite(addr, rinfo)) return 0;  // failed
    addr = rinfo->island_rewrite->addr;
  }

  size_t idx = 0;
  buf[idx++] = ((inst ^ 0x01000000) & 0xFF00001F) | (3 << 5);
  buf[idx++] = 0x58000051;
  buf[idx++] = 0xd61f0220;
  buf[idx++] = addr & 0xFFFFFFFF;
  buf[idx++] = addr >> 32u;
  return 20;
}

static size_t rh_a64_rewrite_tb(uint32_t *buf, uint32_t inst, uintptr_t pc, rh_a64_rewrite_info_t *rinfo) {
  uint64_t imm14 = RH_UTIL_GET_BITS_32(inst, 18, 5);
  uint64_t offset = RH_UTIL_SIGN_EXTEND_64((imm14 << 2u), 16u);
  uint64_t addr = pc + offset;
  addr = rh_a64_fix_addr(addr, rinfo);

  bool use_branch_island = (0 != rinfo->island_rewrite);
  if (use_branch_island) {
    if (0 != rh_a64_build_island_rewrite(addr, rinfo)) return 0;  // failed
    addr = rinfo->island_rewrite->addr;
  }

  size_t idx = 0;
  buf[idx++] = ((inst ^ 0x01000000) & 0xFFF8001F) | (3 << 5);
  buf[idx++] = 0x58000051;
  buf[idx++] = 0xd61f0220;
  buf[idx++] = addr & 0xFFFFFFFF;
  buf[idx++] = addr >> 32u;
  return 20;
}

size_t rh_a64_rewrite(uint32_t *buf, uint32_t inst, uintptr_t pc, rh_a64_rewrite_info_t *rinfo) {
  rh_a64_type_t type = rh_a64_get_type(inst);
  RH_LOG_INFO("a64 rewrite: type %d, inst %" PRIx32, type, inst);

  if (type == B || type == B_COND || type == BL)
    return rh_a64_rewrite_b(buf, inst, pc, type, rinfo);
  else if (type == ADR || type == ADRP)
    return rh_a64_rewrite_adr(buf, inst, pc, type, rinfo);
  else if (type == LDR_LIT_32 || type == LDR_LIT_64 || type == LDRSW_LIT || type == PRFM_LIT ||
           type == LDR_SIMD_LIT_32 || type == LDR_SIMD_LIT_64 || type == LDR_SIMD_LIT_128)
    return rh_a64_rewrite_ldr(buf, inst, pc, type, rinfo);
  else if (type == CBZ || type == CBNZ)
    return rh_a64_rewrite_cb(buf, inst, pc, rinfo);
  else if (type == TBZ || type == TBNZ)
    return rh_a64_rewrite_tb(buf, inst, pc, rinfo);
  else {
    // IGNORED
    buf[0] = inst;
    return 4;
  }
}

size_t rh_a64_nop(uint32_t *buf) {
  buf[0] = 0xd503201f;
  return 4;
}

size_t rh_a64_absolute_jump_with_br_ip(uint32_t *buf, uintptr_t addr) {
  buf[0] = 0x58000051;
  buf[1] = 0xd61f0220;
  buf[2] = addr & 0xFFFFFFFF;
  buf[3] = addr >> 32u;
  return 16;
}

/* ARM DDI0487 */
size_t rh_a64_absolute_jump_with_ret_ip(uint32_t *buf, uintptr_t addr) {
  buf[0] = 0x58000051;
  buf[1] = 0xd65f0220;
  buf[2] = addr & 0xFFFFFFFF;
  buf[3] = addr >> 32u;
  return 16;
}

size_t rh_a64_restore_ip(uint32_t *buf) {
  buf[0] = 0xa97f47f0;
  return 4;
}

size_t rh_a64_absolute_jump_with_br_rx(uint32_t *buf, uintptr_t addr) {
#ifdef RH_CONFIG_CORRUPT_IP_REGS
  buf[0] = 0xa93f47f0;
  buf[1] = 0x58000050;
  buf[2] = 0xd61f0200;
#else
  buf[0] = 0xa93f07e0;
  buf[1] = 0x58000040;
  buf[2] = 0xd61f0000;
#endif
  buf[3] = addr & 0xFFFFFFFF;
  buf[4] = addr >> 32u;
  return 20;
}

size_t rh_a64_absolute_jump_with_ret_rx(uint32_t *buf, uintptr_t addr) {
#ifdef RH_CONFIG_CORRUPT_IP_REGS
  buf[0] = 0xa93f47f0;
  buf[1] = 0x58000050;
  buf[2] = 0xd65f0200;
#else
  buf[0] = 0xa93f07e0;
  buf[1] = 0x58000040;
  buf[2] = 0xd65f0000;
#endif
  buf[3] = addr & 0xFFFFFFFF;
  buf[4] = addr >> 32u;
  return 20;
}

size_t rh_a64_restore_rx(uint32_t *buf) {
#ifdef RH_CONFIG_CORRUPT_IP_REGS
  buf[0] = 0xa97f47f0;
#else
  buf[0] = 0xa97f07e0;
#endif
  return 4;
}

size_t rh_a64_relative_jump(uint32_t *buf, uintptr_t addr, uintptr_t pc) {
  buf[0] = 0x14000000u | (((addr - pc) & 0x0FFFFFFFu) >> 2u);
  return 4;
}

size_t rh_a64_cfg_safe_size(uintptr_t addr, size_t min_bytes, size_t max_bytes) {
  size_t n = max_bytes;
  for (size_t offset = 0; offset + 4 <= n; offset += 4) {
    uint32_t inst = *(uint32_t *)(addr + offset);
    uintptr_t pc = addr + offset;
    rh_a64_type_t type = rh_a64_get_type(inst);

    uintptr_t target = 0;
    bool has_target = false;

    if (type == B || type == BL) {
      uint64_t imm26 = RH_UTIL_GET_BITS_32(inst, 25, 0);
      int64_t imm64 = RH_UTIL_SIGN_EXTEND_64(imm26 << 2u, 28u);
      target = pc + imm64;
      has_target = true;
    } else if (type == B_COND) {
      uint64_t imm19 = RH_UTIL_GET_BITS_32(inst, 23, 5);
      int64_t imm64 = RH_UTIL_SIGN_EXTEND_64(imm19 << 2u, 21u);
      target = pc + imm64;
      has_target = true;
    } else if (type == ADR) {
      uint64_t immlo = RH_UTIL_GET_BITS_32(inst, 30, 29);
      uint64_t immhi = RH_UTIL_GET_BITS_32(inst, 23, 5);
      int64_t imm64 = RH_UTIL_SIGN_EXTEND_64((immhi << 2u) | immlo, 21u);
      target = pc + imm64;
      has_target = true;
    } else if (type == ADRP) {
      uint64_t immlo = RH_UTIL_GET_BITS_32(inst, 30, 29);
      uint64_t immhi = RH_UTIL_GET_BITS_32(inst, 23, 5);
      int64_t imm64 = RH_UTIL_SIGN_EXTEND_64((immhi << 14u) | (immlo << 12u), 33u);
      target = (pc & 0xFFFFFFFFFFFFF000) + imm64;
      has_target = true;
    } else if (type == CBZ || type == CBNZ) {
      uint64_t imm19 = RH_UTIL_GET_BITS_32(inst, 23, 5);
      int64_t imm64 = RH_UTIL_SIGN_EXTEND_64(imm19 << 2u, 21u);
      target = pc + imm64;
      has_target = true;
    } else if (type == TBZ || type == TBNZ) {
      uint64_t imm14 = RH_UTIL_GET_BITS_32(inst, 18, 5);
      int64_t imm64 = RH_UTIL_SIGN_EXTEND_64(imm14 << 2u, 16u);
      target = pc + imm64;
      has_target = true;
    }

    if (has_target && target > pc && target < addr + n) {
      n = target - addr;
      if (n < min_bytes) return 0;
    }
  }
  return n;
}

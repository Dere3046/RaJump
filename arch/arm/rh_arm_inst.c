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

#include "rh_arm_inst.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "rh_a32.h"
#include "rh_config.h"
#include "rh_enter.h"
#include "rh_interwork.h"
#include "rh_island.h"
#include "rh_linker.h"
#include "rh_lr_rewrite.h"
#include "rh_log.h"
#include "rh_sig.h"
#include "rh_t16.h"
#include "rh_t32.h"
#include "rh_txx.h"
#include "rh_util.h"
#include "rahook.h"
#include "rh_invocation_stack.h"
#include "xdl.h"
#include "rh_errno.h"

static void rh_arm_inst_thumb_get_rewrite_info(rh_arm_inst_t *self, uintptr_t target_addr,
                                           rh_txx_rewrite_info_t *rinfo) {
  memset(rinfo, 0, sizeof(rh_txx_rewrite_info_t));

  size_t idx = 0;
  uintptr_t target_addr_offset = 0;
  uintptr_t pc = target_addr + 4;
  size_t rewrite_len = 0;

  while (rewrite_len < self->backup_len) {

    rh_t16_it_t it;
    if (rh_t16_parse_it(&it, *((uint16_t *)(target_addr + target_addr_offset)), pc)) {
      rewrite_len += (2 + it.insts_len);

      size_t it_block_idx = idx++;
      size_t it_block_len = 4 + 4;  // IT-else + IT-then
      for (size_t i = 0, j = 0; i < it.insts_cnt; i++) {
        bool is_thumb32 = rh_util_is_thumb32((uintptr_t)(&(it.insts[j])));
        if (is_thumb32) {
          it_block_len += rh_t32_get_rewrite_inst_len(it.insts[j], it.insts[j + 1]);
          rinfo->inst_lens[idx++] = 0;
          rinfo->inst_lens[idx++] = 0;
          j += 2;
        } else {
          it_block_len += rh_t16_get_rewrite_inst_len(it.insts[j]);
          rinfo->inst_lens[idx++] = 0;
          j += 1;
        }
      }
      rinfo->inst_lens[it_block_idx] = it_block_len;

      target_addr_offset += (2 + it.insts_len);
      pc += (2 + it.insts_len);
    }

    else {
      bool is_thumb32 = rh_util_is_thumb32(target_addr + target_addr_offset);
      size_t inst_len = (is_thumb32 ? 4 : 2);
      rewrite_len += inst_len;

      if (is_thumb32) {
        rinfo->inst_lens[idx++] =
            rh_t32_get_rewrite_inst_len(*((uint16_t *)(target_addr + target_addr_offset)),
                                        *((uint16_t *)(target_addr + target_addr_offset + 2)));
        rinfo->inst_lens[idx++] = 0;
      } else
        rinfo->inst_lens[idx++] =
            rh_t16_get_rewrite_inst_len(*((uint16_t *)(target_addr + target_addr_offset)));

      target_addr_offset += inst_len;
      pc += inst_len;
    }
  }

  rinfo->start_addr = target_addr;
  rinfo->end_addr = target_addr + rewrite_len;
  rinfo->buf = (uint16_t *)self->enter;
  rinfo->buf_offset = 0;
  rinfo->inst_prolog_len = 0;
  rinfo->inst_lens_cnt = idx;
}

static int rh_arm_inst_thumb_rewrite(rh_arm_inst_t *self, uintptr_t target_addr, rh_addr_info_t *addr_info,
                                 rh_arm_inst_set_orig_addr_t set_orig_addr, void *set_orig_addr_arg) {

  memcpy((void *)(self->backup), (void *)target_addr, self->backup_len);


  rh_txx_rewrite_info_t rinfo;
  rh_arm_inst_thumb_get_rewrite_info(self, target_addr, &rinfo);

  // detect dlopen thunk LR pattern
  rinfo.lr_pattern = rh_lr_detect_pattern(target_addr, self->backup_len);

  if (!addr_info->is_proc_start) {
    rinfo.buf_offset += rh_t32_restore_ip((uint16_t *)self->enter);
    rinfo.inst_prolog_len = rinfo.buf_offset;
  }

  rh_interwork_t iw;
  rh_interwork_init(&iw, (uintptr_t)target_addr);
  uintptr_t target_addr_offset = 0;
  uintptr_t pc = target_addr + 4;
  self->rewritten_len = 0;
  while (self->rewritten_len < self->backup_len) {

    rh_t16_it_t it;
    if (rh_t16_parse_it(&it, *((uint16_t *)(target_addr + target_addr_offset)), pc)) {
      self->rewritten_len += (2 + it.insts_len);

      // save space holder point of IT-else B instruction
      uintptr_t enter_inst_else_p = self->enter + rinfo.buf_offset;
      rinfo.buf_offset += 2;  // B<c> <label>
      rinfo.buf_offset += 2;  // NOP

      // rewrite IT block
      size_t enter_inst_else_len = 4;  // B<c> + NOP + B + NOP
      size_t enter_inst_then_len = 0;  // B + NOP
      uintptr_t enter_inst_then_p = 0;
      for (size_t i = 0, j = 0; i < it.insts_cnt; i++) {
        if (i == it.insts_else_cnt) {
          // save space holder point of IT-then (for B instruction)
          enter_inst_then_p = self->enter + rinfo.buf_offset;
          rinfo.buf_offset += 2;  // B <label>
          rinfo.buf_offset += 2;  // NOP

          // fill IT-else B instruction
          rh_t16_rewrite_it_else((uint16_t *)enter_inst_else_p, (uint16_t)enter_inst_else_len, &it);
        }

        // rewrite instructions in IT block
        bool is_thumb32 = rh_util_is_thumb32((uintptr_t)(&(it.insts[j])));
        size_t len;
        if (is_thumb32)
          len = rh_t32_rewrite((uint16_t *)(self->enter + rinfo.buf_offset), it.insts[j], it.insts[j + 1],
                               it.pcs[i], &rinfo);
        else
          len = rh_t16_rewrite((uint16_t *)(self->enter + rinfo.buf_offset), it.insts[j], it.pcs[i], &rinfo);
        if (0 == len) return RAHOOK_ERRNO_HOOK_REWRITE_FAILED;
        rinfo.buf_offset += len;
        j += (is_thumb32 ? 2 : 1);

        // save the total offset for ELSE/THEN in enter
        if (i < it.insts_else_cnt)
          enter_inst_else_len += len;
        else
          enter_inst_then_len += len;

        if (i == it.insts_cnt - 1) {
          // fill IT-then B instruction
          rh_t16_rewrite_it_then((uint16_t *)enter_inst_then_p, (uint16_t)enter_inst_then_len);
        }
      }

      target_addr_offset += (2 + it.insts_len);
      pc += (2 + it.insts_len);
    }

    else {
      bool is_thumb32 = rh_util_is_thumb32(target_addr + target_addr_offset);
      size_t inst_len = (is_thumb32 ? 4 : 2);
      self->rewritten_len += inst_len;

    
      RH_LOG_DEBUG("thumb rewrite: offset %zu, pc %" PRIxPTR, rinfo.buf_offset, pc);
      size_t len;

      // LR rewrite: intercept MOV Rd, LR in dlopen thunk
      if (!is_thumb32 && rinfo.lr_pattern != RH_LR_NONE) {
          uint16_t inst = *((uint16_t *)(target_addr + target_addr_offset));
          if ((inst & 0xFF78) == 0x4670) {
              len = rh_lr_emit_translate_call(
                  (uint16_t *)(self->enter + rinfo.buf_offset), inst,
                  rinfo.lr_pattern,
                  (uintptr_t)rh_invocation_translate_lr);
              if (0 == len) return RAHOOK_ERRNO_HOOK_REWRITE_FAILED;
              rinfo.buf_offset += len;
              goto lr_rewrite_done;
          }
      }

      if (is_thumb32)
        len = rh_t32_rewrite((uint16_t *)(self->enter + rinfo.buf_offset),
                             *((uint16_t *)(target_addr + target_addr_offset)),
                             *((uint16_t *)(target_addr + target_addr_offset + 2)), pc, &rinfo);
      else
        len = rh_t16_rewrite((uint16_t *)(self->enter + rinfo.buf_offset),
                             *((uint16_t *)(target_addr + target_addr_offset)), pc, &rinfo);
      if (0 == len) return RAHOOK_ERRNO_HOOK_REWRITE_FAILED;
      rinfo.buf_offset += len;

lr_rewrite_done:

      {
        uintptr_t insn_addr = target_addr + target_addr_offset;
        uint32_t insn = is_thumb32
            ? *(uint32_t *)insn_addr
            : (uint32_t)*(uint16_t *)insn_addr;
        rh_execute_state_t new_state = rh_interwork_check(&iw, pc, insn, true);
        if (new_state != RH_EXECUTE_THUMB) {
            RH_LOG("thmb-rewrite: ARM/Thmb interwork at %p, state=%d (goto-relocate-remain not yet implemented)", (void *)pc, new_state);
        }
      }

      target_addr_offset += inst_len;
      pc += inst_len;
    }
  }
  RH_LOG_DEBUG("thumb rewrite: len %zu to %zu", self->rewritten_len, rinfo.buf_offset);

  // absolute jump back to remaining original instructions (fill in enter)
  uintptr_t resume_addr = RH_UTIL_SET_BIT0(target_addr + self->rewritten_len);
  rinfo.buf_offset += rh_t32_absolute_jump((uint16_t *)(self->enter + rinfo.buf_offset), true, resume_addr);
  rh_util_clear_cache(self->enter, rinfo.buf_offset);


  if (NULL != set_orig_addr) set_orig_addr(RH_UTIL_SET_BIT0(self->enter), set_orig_addr_arg);
  return 0;
}

static int rh_arm_inst_thumb_safe_rewrite(rh_arm_inst_t *self, uintptr_t target_addr, rh_addr_info_t *addr_info,
                                      rh_arm_inst_set_orig_addr_t set_orig_addr, void *set_orig_addr_arg) {
  size_t resume_len = 26;  // thumb max rewritten_len
  if (addr_info->is_sym_addr) {
    resume_len = addr_info->dli_ssize - (target_addr - RH_UTIL_CLEAR_BIT0((uintptr_t)addr_info->dli_saddr));
  }
  if (0 != rh_util_mprotect(target_addr, resume_len, PROT_READ | PROT_WRITE | PROT_EXEC))
    return RAHOOK_ERRNO_MPROT;

  int r;
  RH_SIG_TRY(SIGSEGV, SIGBUS) {
    r = rh_arm_inst_thumb_rewrite(self, target_addr, addr_info, set_orig_addr, set_orig_addr_arg);
  }
  RH_SIG_CATCH() {
    return RAHOOK_ERRNO_HOOK_REWRITE_CRASH;
  }
  RH_SIG_EXIT
  return r;
}

static bool rh_arm_inst_thumb_is_long_enough(rh_arm_inst_t *self, uintptr_t target_addr, rh_addr_info_t *addr_info) {
  if (!addr_info->is_sym_addr) return true;

  uintptr_t dli_saddr = RH_UTIL_CLEAR_BIT0((uintptr_t)addr_info->dli_saddr);
  size_t resume_len = 0;
  if (!addr_info->is_proc_start) {
    if (0 != dli_saddr && dli_saddr <= target_addr && target_addr < dli_saddr + addr_info->dli_ssize) {
      resume_len = addr_info->dli_ssize - (target_addr - dli_saddr);
    }
  } else {
    if (0 != dli_saddr) {
      resume_len = addr_info->dli_ssize;
    }
  }
  if (resume_len >= self->backup_len) return true;

#ifdef RH_CONFIG_DETECT_THUMB_TAIL_ALIGNED
  // check align-4 in the end of symbol
  if ((self->backup_len == resume_len + 2) && ((target_addr + resume_len) % 4 == 2)) {
    uintptr_t sym_end = dli_saddr + addr_info->dli_ssize;
    if (0 != rh_util_mprotect(sym_end, 2, PROT_READ | PROT_WRITE | PROT_EXEC)) return false;

    // should be zero-ed
    if (0 != *((uint16_t *)sym_end)) return false;

    // should not belong to any symbol
    void *dlcache = NULL;
    xdl_info_t dlinfo;
    if (rh_util_get_api_level() >= __ANDROID_API_L__) {
      xdl_addr((void *)RH_UTIL_SET_BIT0(sym_end), &dlinfo, &dlcache);
    } else {
      RH_SIG_TRY(SIGSEGV, SIGBUS) {
        xdl_addr((void *)RH_UTIL_SET_BIT0(sym_end), &dlinfo, &dlcache);
      }
      RH_SIG_CATCH() {
        memset(&dlinfo, 0, sizeof(dlinfo));
        RH_LOG_WARN("thumb detect tail aligned: crashed");
      }
      RH_SIG_EXIT
    }
    xdl_addr_clean(&dlcache);
    if (0 != dlinfo.dli_saddr) return false;

    // trust here is useless alignment data
    RH_LOG_DEBUG("thumb detect tail aligned: OK %" PRIxPTR, target_addr);
    return true;
  }
#endif

  return false;
}

#ifdef RH_CONFIG_TRY_HOOK_WITH_ISLAND

// B T4: [-16M, +16M - 2]
#define RH_ARM_INST_T32_B_RANGE_LOW  (16777216)
#define RH_ARM_INST_T32_B_RANGE_HIGH (16777214)

static int rh_arm_inst_thumb_rewrite_with_island(rh_arm_inst_t *self, uintptr_t target_addr,
                                             rh_addr_info_t *addr_info, rh_arm_inst_set_orig_addr_t set_orig_addr,
                                             void *set_orig_addr_arg) {
  self->backup_len = 4;
  if (!rh_arm_inst_thumb_is_long_enough(self, target_addr, addr_info)) return RAHOOK_ERRNO_HOOK_SYMSZ;

  return rh_arm_inst_thumb_safe_rewrite(self, target_addr, addr_info, set_orig_addr, set_orig_addr_arg);
}

static int rh_arm_inst_thumb_reloc_with_island(rh_arm_inst_t *self, uintptr_t target_addr, rh_addr_info_t *addr_info,
                                           uintptr_t new_addr, bool is_rehook) {
  int r;
  uintptr_t pc = target_addr + 4;
  rh_island_t new_island_exit;
  uint32_t new_exit[3];

  // alloc an island-exit (exit jump to island-exit)
  uintptr_t island_exit_range_low = pc > RH_ARM_INST_T32_B_RANGE_LOW ? pc - RH_ARM_INST_T32_B_RANGE_LOW : 0;
  uintptr_t island_exit_range_high =
      UINTPTR_MAX - pc > RH_ARM_INST_T32_B_RANGE_HIGH ? pc + RH_ARM_INST_T32_B_RANGE_HIGH : UINTPTR_MAX;
  uintptr_t __rn = island_exit_range_low; uintptr_t __rx = island_exit_range_high; rh_island_alloc(&new_island_exit, pc, 8, __rx > __rn ? __rx - __rn : __rn - __rx);
  if (0 == new_island_exit.addr) return RAHOOK_ERRNO_HOOK_ISLAND_EXIT;

  // absolute jump to new_addr in island-exit
  rh_t32_absolute_jump((uint16_t *)new_island_exit.addr, true, new_addr);
  rh_util_clear_cache(new_island_exit.addr, new_island_exit.size);

  // relative jump to the island-exit by overwriting the head of original function
  rh_t32_relative_jump((uint16_t *)new_exit, new_island_exit.addr, pc);
  if (0 != (r = rh_util_write_inst(target_addr, new_exit, self->backup_len))) {
    rh_island_free(&new_island_exit);
    return r;
  }


  if (0 != self->island_exit.addr) rh_island_free(&self->island_exit);
  self->island_exit = new_island_exit;
  memcpy(self->exit, new_exit, self->backup_len);

  RH_LOG_DEBUG("thumb: %shook (with island) OK. target %" PRIxPTR " -> island-exit %" PRIxPTR
              " -> new %" PRIxPTR " -> enter %" PRIxPTR " -> resume %" PRIxPTR,
              is_rehook ? "re-" : "", RH_UTIL_SET_BIT0(target_addr), self->island_exit.addr, new_addr,
              RH_UTIL_SET_BIT0(self->enter), RH_UTIL_SET_BIT0(target_addr + self->rewritten_len));
  return 0;
}

static int rh_arm_inst_thumb_hook_with_island(rh_arm_inst_t *self, uintptr_t target_addr, rh_addr_info_t *addr_info,
                                          uintptr_t new_addr, rh_arm_inst_set_orig_addr_t set_orig_addr,
                                          void *set_orig_addr_arg) {
  int r;
  if (0 !=
      (r = rh_arm_inst_thumb_rewrite_with_island(self, target_addr, addr_info, set_orig_addr, set_orig_addr_arg)))
    return r;
  if (0 != (r = rh_arm_inst_thumb_reloc_with_island(self, target_addr, addr_info, new_addr, false))) return r;
  return 0;
}
#endif

#ifdef RH_CONFIG_TRY_HOOK_WITHOUT_ISLAND

static int rh_arm_inst_thumb_rewrite_without_island(rh_arm_inst_t *self, uintptr_t target_addr,
                                                rh_addr_info_t *addr_info,
                                                rh_arm_inst_set_orig_addr_t set_orig_addr,
                                                void *set_orig_addr_arg) {
  bool is_align4 = (0 == (target_addr % 4));
  self->backup_len = (is_align4 ? 8 : 10);
  if (!rh_arm_inst_thumb_is_long_enough(self, target_addr, addr_info)) return RAHOOK_ERRNO_HOOK_SYMSZ;

  return rh_arm_inst_thumb_safe_rewrite(self, target_addr, addr_info, set_orig_addr, set_orig_addr_arg);
}

static int rh_arm_inst_thumb_reloc_without_island(rh_arm_inst_t *self, uintptr_t target_addr, uintptr_t new_addr,
                                              bool is_rehook) {
  bool is_align4 = (0 == (target_addr % 4));
  uint32_t new_exit[3];
  int r;

  rh_t32_absolute_jump((uint16_t *)new_exit, is_align4, new_addr);
  if (0 != (r = rh_util_write_inst(target_addr, new_exit, self->backup_len))) return r;
  memcpy(self->exit, new_exit, self->backup_len);

  RH_LOG_DEBUG("thumb: %shook (without island) OK. target %" PRIxPTR " -> new %" PRIxPTR " -> enter %" PRIxPTR
              " -> resume %" PRIxPTR,
              is_rehook ? "re-" : "", RH_UTIL_SET_BIT0(target_addr), new_addr, RH_UTIL_SET_BIT0(self->enter),
              RH_UTIL_SET_BIT0(target_addr + self->rewritten_len));
  return 0;
}

static int rh_arm_inst_thumb_hook_without_island(rh_arm_inst_t *self, uintptr_t target_addr,
                                             rh_addr_info_t *addr_info, uintptr_t new_addr,
                                             rh_arm_inst_set_orig_addr_t set_orig_addr, void *set_orig_addr_arg) {
  int r;
  if (0 != (r = rh_arm_inst_thumb_rewrite_without_island(self, target_addr, addr_info, set_orig_addr,
                                                     set_orig_addr_arg)))
    return r;
  if (0 != (r = rh_arm_inst_thumb_reloc_without_island(self, target_addr, new_addr, false))) return r;
  return 0;
}
#endif

static int rh_arm_inst_arm_rewrite(rh_arm_inst_t *self, uintptr_t target_addr, rh_addr_info_t *addr_info,
                               rh_arm_inst_set_orig_addr_t set_orig_addr, void *set_orig_addr_arg) {

  memcpy((void *)(self->backup), (void *)target_addr, self->backup_len);


  rh_a32_rewrite_info_t rinfo;
  rinfo.start_addr = target_addr;
  rinfo.end_addr = target_addr + self->backup_len;
  rinfo.buf = (uint32_t *)self->enter;
  rinfo.buf_offset = 0;
  rinfo.inst_prolog_len = 0;
  rinfo.inst_lens_cnt = self->backup_len / 4;
  for (uintptr_t i = 0; i < self->backup_len; i += 4)
    rinfo.inst_lens[i / 4] = rh_a32_get_rewrite_inst_len(*((uint32_t *)(target_addr + i)));

  if (!addr_info->is_proc_start) {
    rinfo.buf_offset += rh_a32_restore_ip((uint32_t *)self->enter);
    rinfo.inst_prolog_len = rinfo.buf_offset;
  }


  uintptr_t pc = target_addr + 8;
  for (uintptr_t i = 0; i < self->backup_len; i += 4, pc += 4) {
    size_t offset = rh_a32_rewrite((uint32_t *)(self->enter + rinfo.buf_offset),
                                   *((uint32_t *)(target_addr + i)), pc, &rinfo);
    if (0 == offset) return RAHOOK_ERRNO_HOOK_REWRITE_FAILED;
    rinfo.buf_offset += offset;
  }

  // absolute jump back to remaining original instructions (fill in enter)
  uintptr_t resume_addr = target_addr + self->backup_len;
  rinfo.buf_offset += rh_a32_absolute_jump((uint32_t *)(self->enter + rinfo.buf_offset), resume_addr);
  rh_util_clear_cache(self->enter, rinfo.buf_offset);


  if (NULL != set_orig_addr) set_orig_addr(self->enter, set_orig_addr_arg);
  return 0;
}

static int rh_arm_inst_arm_safe_rewrite(rh_arm_inst_t *self, uintptr_t target_addr, rh_addr_info_t *addr_info,
                                    rh_arm_inst_set_orig_addr_t set_orig_addr, void *set_orig_addr_arg) {
  if (0 != rh_util_mprotect(target_addr, self->backup_len, PROT_READ | PROT_WRITE | PROT_EXEC))
    return RAHOOK_ERRNO_MPROT;

  int r;
  RH_SIG_TRY(SIGSEGV, SIGBUS) {
    r = rh_arm_inst_arm_rewrite(self, target_addr, addr_info, set_orig_addr, set_orig_addr_arg);
  }
  RH_SIG_CATCH() {
    return RAHOOK_ERRNO_HOOK_REWRITE_CRASH;
  }
  RH_SIG_EXIT
  return r;
}

#ifdef RH_CONFIG_TRY_HOOK_WITH_ISLAND

// B A1: [-32M, +32M - 4]
#define RH_ARM_INST_A32_B_RANGE_LOW  (33554432)
#define RH_ARM_INST_A32_B_RANGE_HIGH (33554428)

static int rh_arm_inst_arm_rewrite_with_island(rh_arm_inst_t *self, uintptr_t target_addr, rh_addr_info_t *addr_info,
                                           rh_arm_inst_set_orig_addr_t set_orig_addr, void *set_orig_addr_arg) {
  self->backup_len = 4;

  return rh_arm_inst_arm_safe_rewrite(self, target_addr, addr_info, set_orig_addr, set_orig_addr_arg);
}

static int rh_arm_inst_arm_reloc_with_island(rh_arm_inst_t *self, uintptr_t target_addr, rh_addr_info_t *addr_info,
                                         uintptr_t new_addr, bool is_rehook) {
  int r;
  uintptr_t pc = target_addr + 8;
  rh_island_t new_island_exit;
  uint32_t new_exit[3];

  // alloc an island-exit (exit jump to island-exit)
  uintptr_t island_exit_range_low = pc > RH_ARM_INST_A32_B_RANGE_LOW ? pc - RH_ARM_INST_A32_B_RANGE_LOW : 0;
  uintptr_t island_exit_range_high =
      UINTPTR_MAX - pc > RH_ARM_INST_A32_B_RANGE_HIGH ? pc + RH_ARM_INST_A32_B_RANGE_HIGH : UINTPTR_MAX;
  uintptr_t __rn = island_exit_range_low; uintptr_t __rx = island_exit_range_high; rh_island_alloc(&new_island_exit, pc, 8, __rx > __rn ? __rx - __rn : __rn - __rx);
  if (0 == new_island_exit.addr) return RAHOOK_ERRNO_HOOK_ISLAND_EXIT;

  // absolute jump to new_addr in island-exit
  rh_a32_absolute_jump((uint32_t *)new_island_exit.addr, new_addr);
  rh_util_clear_cache(new_island_exit.addr, new_island_exit.size);

  // relative jump to the island-exit by overwriting the head of original function
  rh_a32_relative_jump((uint32_t *)new_exit, new_island_exit.addr, pc);
  if (0 != (r = rh_util_write_inst(target_addr, new_exit, self->backup_len))) {
    rh_island_free(&new_island_exit);
    return r;
  }


  if (0 != self->island_exit.addr) rh_island_free(&self->island_exit);
  self->island_exit = new_island_exit;
  memcpy(self->exit, new_exit, self->backup_len);

  RH_LOG_DEBUG("a32: %shook (with island) OK. target %" PRIxPTR " -> island-exit %" PRIxPTR " -> new %" PRIxPTR
              " -> enter %" PRIxPTR " -> resume %" PRIxPTR,
              is_rehook ? "re-" : "", target_addr, self->island_exit.addr, new_addr, self->enter,
              target_addr + self->backup_len);
  return 0;
}

static int rh_arm_inst_arm_hook_with_island(rh_arm_inst_t *self, uintptr_t target_addr, rh_addr_info_t *addr_info,
                                        uintptr_t new_addr, rh_arm_inst_set_orig_addr_t set_orig_addr,
                                        void *set_orig_addr_arg) {
  int r;
  if (0 !=
      (r = rh_arm_inst_arm_rewrite_with_island(self, target_addr, addr_info, set_orig_addr, set_orig_addr_arg)))
    return r;
  if (0 != (r = rh_arm_inst_arm_reloc_with_island(self, target_addr, addr_info, new_addr, false))) return r;
  return 0;
}
#endif

#ifdef RH_CONFIG_TRY_HOOK_WITHOUT_ISLAND

static int rh_arm_inst_arm_rewrite_without_island(rh_arm_inst_t *self, uintptr_t target_addr,
                                              rh_addr_info_t *addr_info,
                                              rh_arm_inst_set_orig_addr_t set_orig_addr,
                                              void *set_orig_addr_arg) {
  self->backup_len = 8;

  if (addr_info->is_sym_addr) {
    size_t resume_len = 0;
    if (!addr_info->is_proc_start) {
      if (NULL != addr_info->dli_saddr && (uintptr_t)addr_info->dli_saddr <= target_addr &&
          target_addr < (uintptr_t)addr_info->dli_saddr + addr_info->dli_ssize) {
        resume_len = addr_info->dli_ssize - (target_addr - (uintptr_t)addr_info->dli_saddr);
      }
    } else {
      if (NULL != addr_info->dli_saddr) {
        resume_len = addr_info->dli_ssize;
      }
    }
    if (resume_len < self->backup_len) return RAHOOK_ERRNO_HOOK_SYMSZ;
  }

  return rh_arm_inst_arm_safe_rewrite(self, target_addr, addr_info, set_orig_addr, set_orig_addr_arg);
}

static int rh_arm_inst_arm_reloc_without_island(rh_arm_inst_t *self, uintptr_t target_addr, uintptr_t new_addr,
                                            bool is_rehook) {
  uint32_t new_exit[3];
  int r;

  rh_a32_absolute_jump((uint32_t *)new_exit, new_addr);
  if (0 != (r = rh_util_write_inst(target_addr, new_exit, self->backup_len))) return r;
  memcpy(self->exit, new_exit, self->backup_len);

  RH_LOG_DEBUG("a32: %shook (without island) OK. target %" PRIxPTR " -> new %" PRIxPTR " -> enter %" PRIxPTR
              " -> resume %" PRIxPTR,
              is_rehook ? "re-" : "", target_addr, new_addr, self->enter, target_addr + self->backup_len);
  return 0;
}

static int rh_arm_inst_arm_hook_without_island(rh_arm_inst_t *self, uintptr_t target_addr, rh_addr_info_t *addr_info,
                                           uintptr_t new_addr, rh_arm_inst_set_orig_addr_t set_orig_addr,
                                           void *set_orig_addr_arg) {
  int r;
  if (0 != (r = rh_arm_inst_arm_rewrite_without_island(self, target_addr, addr_info, set_orig_addr,
                                                   set_orig_addr_arg)))
    return r;
  if (0 != (r = rh_arm_inst_arm_reloc_without_island(self, target_addr, new_addr, false))) return r;
  return 0;
}
#endif

int rh_arm_inst_hook_full(rh_arm_inst_t *self, uintptr_t target_addr, rh_addr_info_t *addr_info, uintptr_t new_addr,
                 bool is_to_interceptor, rh_arm_inst_set_orig_addr_t set_orig_addr, void *set_orig_addr_arg) {
  (void)is_to_interceptor;

  self->enter = rh_enter_alloc();
  if (0 == self->enter) return RAHOOK_ERRNO_HOOK_ENTER;

  int r = -1;
  if (RH_UTIL_IS_THUMB(target_addr)) {
    if (NULL == addr_info->dli_saddr && addr_info->is_sym_addr) {
      if (0 != (r = rh_linker_get_addr_info_by_addr(addr_info, (void *)target_addr, addr_info->is_sym_addr,
                                                    addr_info->is_proc_start, false)))
        goto err;
    }
    target_addr = RH_UTIL_CLEAR_BIT0(target_addr);
#ifdef RH_CONFIG_TRY_HOOK_WITH_ISLAND
    if (0 == (r = rh_arm_inst_thumb_hook_with_island(self, target_addr, addr_info, new_addr, set_orig_addr,
                                                 set_orig_addr_arg)))
      return r;
#endif
#ifdef RH_CONFIG_TRY_HOOK_WITHOUT_ISLAND
    if (0 == (r = rh_arm_inst_thumb_hook_without_island(self, target_addr, addr_info, new_addr, set_orig_addr,
                                                    set_orig_addr_arg)))
      return r;
#endif
  } else {
#ifdef RH_CONFIG_TRY_HOOK_WITH_ISLAND
    if (0 == (r = rh_arm_inst_arm_hook_with_island(self, target_addr, addr_info, new_addr, set_orig_addr,
                                               set_orig_addr_arg)))
      return r;
#endif
#ifdef RH_CONFIG_TRY_HOOK_WITHOUT_ISLAND
    if (NULL == addr_info->dli_saddr && addr_info->is_sym_addr) {
      if (0 != (r = rh_linker_get_addr_info_by_addr(addr_info, (void *)target_addr, addr_info->is_sym_addr,
                                                    addr_info->is_proc_start, false)))
        goto err;
    }
    if (0 == (r = rh_arm_inst_arm_hook_without_island(self, target_addr, addr_info, new_addr, set_orig_addr,
                                                  set_orig_addr_arg)))
      return r;
#endif
  }

err:
  // hook failed
  if (NULL != set_orig_addr) set_orig_addr(0, set_orig_addr_arg);
  rh_enter_free(self->enter);
  return r;
}

int rh_arm_inst_rehook(rh_arm_inst_t *self, uintptr_t target_addr, rh_addr_info_t *addr_info, uintptr_t new_addr,
                   bool is_to_interceptor) {
  (void)is_to_interceptor;

  if (RH_UTIL_IS_THUMB(target_addr)) {
    target_addr = RH_UTIL_CLEAR_BIT0(target_addr);
    if (4 == self->backup_len) {
#ifdef RH_CONFIG_TRY_HOOK_WITH_ISLAND
      return rh_arm_inst_thumb_reloc_with_island(self, target_addr, addr_info, new_addr, true);
#else
      abort();
#endif
    } else {
#ifdef RH_CONFIG_TRY_HOOK_WITHOUT_ISLAND
      (void)addr_info;
      return rh_arm_inst_thumb_reloc_without_island(self, target_addr, new_addr, true);
#else
      abort();
#endif
    }
  } else {
    if (4 == self->backup_len) {
#ifdef RH_CONFIG_TRY_HOOK_WITH_ISLAND
      return rh_arm_inst_arm_reloc_with_island(self, target_addr, addr_info, new_addr, true);
#else
      abort();
#endif
    } else {
#ifdef RH_CONFIG_TRY_HOOK_WITHOUT_ISLAND
      (void)addr_info;
      return rh_arm_inst_arm_reloc_without_island(self, target_addr, new_addr, true);
#else
      abort();
#endif
    }
  }
}

int rh_arm_inst_unhook(rh_arm_inst_t *self, void *target) {
  uintptr_t target_addr = (uintptr_t)target;
  int r;
  bool is_thumb = RH_UTIL_IS_THUMB(target_addr);
  if (is_thumb) target_addr = RH_UTIL_CLEAR_BIT0(target_addr);

  // restore the instructions at the target address
  RH_SIG_TRY(SIGSEGV, SIGBUS) {
    r = memcmp((void *)target_addr, self->exit, self->backup_len);
  }
  RH_SIG_CATCH() {
    return RAHOOK_ERRNO_UNHOOK_CMP_CRASH;
  }
  RH_SIG_EXIT
  if (0 != r) return RAHOOK_ERRNO_UNHOOK_TRAMPO_MISMATCH;
  if (0 != (r = rh_util_write_inst(target_addr, self->backup, self->backup_len))) return r;
  __atomic_thread_fence(__ATOMIC_SEQ_CST);

  // free memory space for island-exit
  if (0 != self->island_exit.addr) rh_island_free(&self->island_exit);

  // free memory space for enter
  rh_enter_free(self->enter);

  RH_LOG_DEBUG("%s: unhook OK. target %" PRIxPTR, is_thumb ? "thumb" : "a32", target_addr);
  return 0;
}

void rh_arm_inst_free_after_dlclose(rh_arm_inst_t *self, uintptr_t target_addr) {
  // free memory space for island-exit
  if (0 != self->island_exit.addr) rh_island_free_after_dlclose(&self->island_exit);

  // free memory space for enter
  rh_enter_free(self->enter);

  bool is_thumb = RH_UTIL_IS_THUMB(target_addr);
  RH_LOG_DEBUG("%s: free_after_dlclose OK. target %" PRIxPTR, is_thumb ? "thumb" : "a32", target_addr);
}

extern void rahook_interceptor_glue(void);
extern void rahook_interceptor_glue_vfpv3d16(void);
extern void rahook_interceptor_glue_vfpv3d32(void);
void rh_arm_inst_build_glue_launcher(void *buf, void *ctx) {
  uint32_t *b = (uint32_t *)buf;
  // instruction sets: arm
#ifdef RH_CONFIG_CORRUPT_IP_REGS
  b[0] = 0xE50DC004;
  b[1] = 0xE59FC004;
  b[2] = 0xE51FF004;
#else
  b[0] = 0xE50D0004;
  b[1] = 0xE59F0004;
  b[2] = 0xE51FF004;
#endif
  size_t cpu_feat = rh_util_get_arm_cpu_features();
  if (cpu_feat & RH_UTIL_ARM_CPU_FEATURE_VFPV3D32)
    b[3] = (uint32_t)rahook_interceptor_glue_vfpv3d32;
  else if (cpu_feat & RH_UTIL_ARM_CPU_FEATURE_VFPV3D16)
    b[3] = (uint32_t)rahook_interceptor_glue_vfpv3d16;
  else
    b[3] = (uint32_t)rahook_interceptor_glue;
  b[4] = (uint32_t)ctx;
}

// simple wrapper matching the public rahook API
int rh_arm_inst_hook(rh_arm_inst_t *self, void *target, void *replace, void **origin)
{
    rh_addr_info_t addr_info;
    memset(&addr_info, 0, sizeof(addr_info));
    addr_info.is_sym_addr = true;
    addr_info.is_proc_start = true;

    uintptr_t result_origin = 0;
    int r = rh_arm_inst_hook_full(self, (uintptr_t)target, &addr_info, (uintptr_t)replace,
                                    false, NULL, NULL);
    if (r == 0 && self->enter)
        result_origin = self->enter;
    if (origin) *origin = (void *)result_origin;
    return r;
}

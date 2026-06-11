#include "rh_x64.h"
#include "rh_x64_inst.h"
#include "core/rh_trampo.h"
#include "third_party/hde64/hde64.h"
#include "core/rh_sig.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define RH_X64_NEAR_LIMIT 0x7FFFFFFFLL

static int is_cet_endbr(const uint8_t *code)
{
    const uint8_t *p = code;
    int has_f3 = 0;
    while (*p == 0x66 || *p == 0x67 || *p == 0xF0 || *p == 0xF2 || *p == 0xF3 ||
           (*p >= 0x26 && *p <= 0x2E) || *p == 0x64 || *p == 0x65) {
        if (*p == 0xF3) has_f3 = 1;
        p++;
    }
    return has_f3 && p[0] == 0x0F && p[1] == 0x1E && p[2] == 0xFA;
}

int rh_x64_inst_hook(rh_x64_inst_t *inst, void *target, void *replace, void **origin)
{
    if (!inst || !target || !replace || !origin) return -1;

    memset(inst, 0, sizeof(*inst));

    uint8_t *tgt = (uint8_t *)target;
    int cet_skip = 0;

    if (is_cet_endbr(tgt)) {
        cet_skip = 4;
        tgt += 4;
    }

    size_t total = 0;
    while (total < 5) {
        hde64s hs;
        size_t len = hde64_disasm(tgt + total, &hs);
        if (len == 0) len = 1;
        total += len;
        if (total > sizeof(inst->backup)) return -1;
    }

    uintptr_t tp = (uintptr_t)tgt;
    int64_t diff = (int64_t)((uint8_t *)replace - (uint8_t *)(tp + 5));

    if (diff >= -RH_X64_NEAR_LIMIT - 1 && diff <= RH_X64_NEAR_LIMIT) {
        inst->exit[0] = 0xE9;
        *(int32_t *)(inst->exit + 1) = (int32_t)diff;
        for (size_t i = 5; i < total && i < sizeof(inst->exit); i++)
            inst->exit[i] = 0x90;
    } else {
        /* 6-byte FF 25 <disp32> + 8-byte forward stub */
        if (total < 6) {
            while (total < 6) {
                hde64s hs2;
                size_t len = hde64_disasm(tgt + total, &hs2);
                if (len == 0) len = 1;
                total += len;
                if (total > sizeof(inst->backup)) return -1;
            }
        }
        uintptr_t stub_addr = rh_trampo_alloc_near_3tier(
            rh_trampo_get_global(), 8,
            tp - 0x7FFFFFFFLL, tp + 0x7FFFFFFFLL);
        if (stub_addr) {
            *(uintptr_t *)stub_addr = (uintptr_t)replace;
            int32_t disp = (int32_t)(stub_addr - (tp + 6));
            inst->exit[0] = 0xFF;
            inst->exit[1] = 0x25;
            *(int32_t *)(inst->exit + 2) = disp;
            for (size_t i = 6; i < total && i < sizeof(inst->exit); i++)
                inst->exit[i] = 0x90;
        } else {
            /* fallback: MOVABS RAX, imm64; JMP RAX (12 bytes) */
            if (total < 12) {
                while (total < 12) {
                    hde64s hs2;
                    size_t len = hde64_disasm(tgt + total, &hs2);
                    if (len == 0) len = 1;
                    total += len;
                    if (total > sizeof(inst->backup)) return -1;
                }
            }
            size_t off = 0;
            inst->exit[off++] = 0x48;
            inst->exit[off++] = 0xB8;
            *(uint64_t *)(inst->exit + off) = (uint64_t)replace; off += 8;
            inst->exit[off++] = 0xFF;
            inst->exit[off++] = 0xE0;
            for (; off < total && off < sizeof(inst->exit); off++)
                inst->exit[off] = 0x90;
        }
    }
    inst->backup_len = total;

    memcpy(inst->backup, tgt, inst->backup_len);

    size_t enter_prefix = (size_t)cet_skip;
    size_t enter_max = enter_prefix + inst->backup_len * 6 + 32;
    uintptr_t enter_addr = rh_trampo_alloc_near_3tier(
        rh_trampo_get_global(), enter_max,
        tp - 0x7FFFFFFFLL, tp + 0x7FFFFFFFLL);
    if (!enter_addr) return -1;
    void *enter_mem = (void *)enter_addr;

    uint8_t *enter_buf = (uint8_t *)enter_mem;

    if (cet_skip)
        memcpy(enter_buf, (uint8_t *)target, 4);

    inst->enter_size = rh_x64_relocate_instructions(
        inst->backup, enter_buf + enter_prefix, inst->backup_len,
        (uintptr_t)tgt, (uintptr_t)(enter_buf + enter_prefix));
    inst->enter_size += enter_prefix;
    inst->enter = enter_buf;

    {
        int rh_safe_write_ok = 0;
        rh_sig_jmp_t __sj;
        if (0 == rh_sig_setjmp(&__sj, SIGSEGV, -1)) {
            uintptr_t page = (uintptr_t)tgt & ~0xFFFUL;
            if (mprotect((void *)page, 4096, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
                rh_sig_exit(&__sj);
                rh_trampo_free(rh_trampo_get_global(), (uintptr_t)enter_buf, enter_max);
                return -1;
            }
            memcpy(tgt, inst->exit, inst->backup_len);
            *origin = inst->enter;
            rh_safe_write_ok = 1;
        }
        rh_sig_exit(&__sj);
        if (!rh_safe_write_ok) {
            rh_trampo_free(rh_trampo_get_global(), (uintptr_t)enter_buf, enter_max);
            return -1;
        }
    }
    return 0;
}

int rh_x64_inst_unhook(rh_x64_inst_t *inst, void *target)
{
    if (!inst || !target) return -1;

    uint8_t *tgt = (uint8_t *)target;
    if (is_cet_endbr(tgt)) tgt += 4;

    mprotect((void *)((uintptr_t)tgt & ~0xFFFUL), 4096,
             PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy(tgt, inst->backup, inst->backup_len);
    mprotect((void *)((uintptr_t)tgt & ~0xFFFUL), 4096, PROT_READ | PROT_EXEC);

    if (inst->enter) {
        size_t cet_skip_inner = is_cet_endbr((uint8_t *)target) ? 4 : 0;
        size_t free_size = cet_skip_inner + inst->backup_len * 6 + 32;
        rh_trampo_free(rh_trampo_get_global(), (uintptr_t)inst->enter, free_size);
    }

    if (inst->island_exit.addr) {
        munmap((void *)inst->island_exit.addr, inst->island_exit.size);
    }
    if (inst->island_forward.addr) {
        munmap((void *)inst->island_forward.addr, inst->island_forward.size);
    }

    memset(inst, 0, sizeof(*inst));
    return 0;
}

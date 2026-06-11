#include "rh_ia32.h"
#include "rh_x86_inst.h"
#include "core/rh_sig.h"
#include "third_party/hde64/hde64.h"
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define RH_IA32_NEAR_LIMIT 0x7FFFFFFF

static int make_rwx(void *addr, size_t size)
{
    return mprotect(addr, size, PROT_READ | PROT_WRITE | PROT_EXEC);
}

int rh_x86_inst_hook(rh_x86_inst_t *inst, void *target, void *replace, void **origin)
{
    if (!inst || !target || !replace || !origin) return -1;

    memset(inst, 0, sizeof(*inst));

    /* scan instructions at target until cumulative size >= 5 */
    size_t total = 0;
    while (total < 5) {
        hde64s hs;
        size_t len = hde64_disasm((const uint8_t *)target + total, &hs);
        if (len == 0) len = 1;
        total += len;
        if (total > sizeof(inst->backup)) {
            /* need more backup than we have space for */
            return -1;
        }
    }
    inst->backup_len = total;

    /* build exit trampoline */
    uintptr_t tp = (uintptr_t)target;
    uintptr_t rp = (uintptr_t)replace;
    intptr_t diff = (intptr_t)((uint8_t *)rp - (uint8_t *)(tp + 5));

    /* on IA-32, near JMP always reaches the full 4GB space */
    if (diff >= -(intptr_t)RH_IA32_NEAR_LIMIT - 1 && diff <= (intptr_t)RH_IA32_NEAR_LIMIT) {
        inst->exit[0] = 0xE9;
        *(uint32_t *)(inst->exit + 1) = (uint32_t)(int32_t)diff;
        /* NOP-fill remaining bytes at target beyond the JMP */
        for (size_t i = 5; i < inst->backup_len; i++) {
            inst->exit[i] = 0x90;
        }
    } else {
        /* far: MOV EAX, replace; JMP EAX (7 bytes) */
        inst->exit[0] = 0xB8;
        *(uint64_t *)(inst->exit + 1) = (uint64_t)replace;
        inst->exit[5] = 0xFF;
        inst->exit[6] = 0xE0;
        for (size_t i = 7; i < inst->backup_len; i++) {
            inst->exit[i] = 0x90;
        }
        if (inst->backup_len < 7) inst->backup_len = 7;
    }

    /* save original bytes */
    memcpy(inst->backup, target, inst->backup_len);

    /* allocate enter trampoline */
    size_t enter_max = inst->backup_len * 4 + 16;
    void *enter_mem = mmap(NULL, enter_max, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (enter_mem == MAP_FAILED) return -1;
    if (make_rwx(enter_mem, enter_max) != 0) {
        munmap(enter_mem, enter_max);
        return -1;
    }

    /* relocate backup to enter */
    inst->enter_size = rh_ia32_relocate_instructions(
        inst->backup, enter_mem, inst->backup_len,
        (uintptr_t)target, (uintptr_t)enter_mem);
    inst->enter = enter_mem;

    /* patch target */
    {
        int rh_safe_write_ok = 0;
        rh_sig_jmp_t __sj;
        if (0 == rh_sig_setjmp(&__sj, SIGSEGV, -1)) {
            if (mprotect((void *)((uintptr_t)target & ~0xFFFUL), 4096,
                         PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
                rh_sig_exit(&__sj);
                munmap(enter_mem, enter_max);
                return -1;
            }
            memcpy(target, inst->exit, inst->backup_len);
            *origin = inst->enter;
            rh_safe_write_ok = 1;
        }
        rh_sig_exit(&__sj);
        if (!rh_safe_write_ok) {
            munmap(enter_mem, enter_max);
            return -1;
        }
    }

    return 0;

    return 0;
}

int rh_x86_inst_unhook(rh_x86_inst_t *inst, void *target)
{
    if (!inst || !target) return -1;

    /* restore original bytes */
    mprotect((void *)((uintptr_t)target & ~0xFFFUL), 4096,
             PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy(target, inst->backup, inst->backup_len);
    mprotect((void *)((uintptr_t)target & ~0xFFFUL), 4096, PROT_READ | PROT_EXEC);

    /* free enter trampoline */
    if (inst->enter) {
        /* TODO: rh_trampo_free */
        munmap(inst->enter, inst->enter_size * 2 + 16);
    }

    /* free island if allocated */
    if (inst->island_exit.addr) {
        munmap((void *)inst->island_exit.addr, inst->island_exit.size);
    }

    memset(inst, 0, sizeof(*inst));
    return 0;
}

/*
 * Copyright (C) 2019-2024 HookZz Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Adapted for RaHook:
 * Copyright (C) 2026 Dere3046
 */

#include "rh_bridge.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

__attribute__((naked)) static void rh_arm_bridge_template(void) {
    __asm__ volatile(
        ".arm\n\t"

        "sub sp, sp, #(14*4)\n\t"
        "str lr, [sp, #(13*4)]\n\t"
        "str r12, [sp, #(12*4)]\n\t"
        "str r11, [sp, #(11*4)]\n\t"
        "str r10, [sp, #(10*4)]\n\t"
        "str r9, [sp, #(9*4)]\n\t"
        "str r8, [sp, #(8*4)]\n\t"
        "str r7, [sp, #(7*4)]\n\t"
        "str r6, [sp, #(6*4)]\n\t"
        "str r5, [sp, #(5*4)]\n\t"
        "str r4, [sp, #(4*4)]\n\t"
        "str r3, [sp, #(3*4)]\n\t"
        "str r2, [sp, #(2*4)]\n\t"
        "str r1, [sp, #(1*4)]\n\t"
        "str r0, [sp, #(0*4)]\n\t"

        "sub sp, sp, #8\n\t"

        "mov r0, sp\n\t"
        "mov r1, r12\n\t"
        "bl rh_arm_bridge_handler\n\t"

        "add sp, sp, #8\n\t"

        "ldr r0, [sp], #4\n\t"
        "ldr r1, [sp], #4\n\t"
        "ldr r2, [sp], #4\n\t"
        "ldr r3, [sp], #4\n\t"
        "ldr r4, [sp], #4\n\t"
        "ldr r5, [sp], #4\n\t"
        "ldr r6, [sp], #4\n\t"
        "ldr r7, [sp], #4\n\t"
        "ldr r8, [sp], #4\n\t"
        "ldr r9, [sp], #4\n\t"
        "ldr r10, [sp], #4\n\t"
        "ldr r11, [sp], #4\n\t"
        "ldr r12, [sp], #4\n\t"
        "ldr lr, [sp], #4\n\t"

        "str r12, [sp, #-4]\n\t"
        "ldr pc, [sp, #-4]\n\t"
    );
}

void rh_arm_bridge_handler(rh_arm_reg_state_t *rs, rh_arm_bridge_data_t *cbd) {
    void (*handler)(rh_arm_reg_state_t *rs, rh_arm_bridge_data_t *cbd) = cbd->user_code;
    handler(rs, cbd);
}

int rh_bridge_init(void) {
    return 0;
}

rh_arm_bridge_page_t *rh_arm_bridge_alloc(void) {
    long page_size = sysconf(_SC_PAGESIZE);

    void *page = mmap(NULL, (size_t)page_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED)
        return NULL;

    if (mprotect(page, (size_t)page_size, PROT_READ | PROT_EXEC)) {
        munmap(page, (size_t)page_size);
        return NULL;
    }

    rh_arm_bridge_page_t *bp = malloc(sizeof(rh_arm_bridge_page_t));
    if (!bp) {
        munmap(page, (size_t)page_size);
        return NULL;
    }

    bp->page          = page;
    bp->enter_bridge  = (void *)rh_arm_bridge_template;
    bp->leave_bridge  = (void *)rh_arm_bridge_template;
    bp->page_size     = (size_t)page_size;

    return bp;
}

void rh_arm_bridge_free(rh_arm_bridge_page_t *page) {
    if (!page)
        return;
    munmap(page->page, page->page_size);
    free(page);
}

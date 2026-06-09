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

__attribute__((naked)) static void rh_bridge_template(void) {
    __asm__ volatile(
        "stp fp, lr, [sp, #-16]!\n\t"
        "mov fp, sp\n\t"

        "sub sp, sp, #(8*16)\n\t"
        "stp q6, q7, [sp, #(6*16)]\n\t"
        "stp q4, q5, [sp, #(4*16)]\n\t"
        "stp q2, q3, [sp, #(2*16)]\n\t"
        "stp q0, q1, [sp, #(0*16)]\n\t"

        "sub sp, sp, #(30*8)\n\t"
        "stp x29, x30, [sp, #(28*8)]\n\t"
        "stp x27, x28, [sp, #(26*8)]\n\t"
        "stp x25, x26, [sp, #(24*8)]\n\t"
        "stp x23, x24, [sp, #(22*8)]\n\t"
        "stp x21, x22, [sp, #(20*8)]\n\t"
        "stp x19, x20, [sp, #(18*8)]\n\t"
        "stp x17, x18, [sp, #(16*8)]\n\t"
        "stp x15, x16, [sp, #(14*8)]\n\t"
        "stp x13, x14, [sp, #(12*8)]\n\t"
        "stp x11, x12, [sp, #(10*8)]\n\t"
        "stp x9, x10, [sp, #(8*8)]\n\t"
        "stp x7, x8, [sp, #(6*8)]\n\t"
        "stp x5, x6, [sp, #(4*8)]\n\t"
        "stp x3, x4, [sp, #(2*8)]\n\t"
        "stp x1, x2, [sp, #(0*8)]\n\t"

        "sub sp, sp, #(2*8)\n\t"
        "str x0, [sp, #8]\n\t"

        "mov x0, sp\n\t"
        "mov x1, x14\n\t"
        "bl rh_bridge_handler\n\t"

        "ldr x0, [sp, #8]\n\t"
        "add sp, sp, #(2*8)\n\t"

        "ldp x1, x2, [sp], #16\n\t"
        "ldp x3, x4, [sp], #16\n\t"
        "ldp x5, x6, [sp], #16\n\t"
        "ldp x7, x8, [sp], #16\n\t"
        "ldp x9, x10, [sp], #16\n\t"
        "ldp x11, x12, [sp], #16\n\t"
        "ldp x13, x14, [sp], #16\n\t"
        "ldp x15, x16, [sp], #16\n\t"
        "ldp x17, x18, [sp], #16\n\t"
        "ldp x19, x20, [sp], #16\n\t"
        "ldp x21, x22, [sp], #16\n\t"
        "ldp x23, x24, [sp], #16\n\t"
        "ldp x25, x26, [sp], #16\n\t"
        "ldp x27, x28, [sp], #16\n\t"
        "ldp x29, x30, [sp], #16\n\t"

        "ldp q0, q1, [sp], #32\n\t"
        "ldp q2, q3, [sp], #32\n\t"
        "ldp q4, q5, [sp], #32\n\t"
        "ldp q6, q7, [sp], #32\n\t"

        "mov sp, fp\n\t"
        "ldp fp, lr, [sp], #16\n\t"

        "br x15\n\t"
    );
}

void rh_bridge_handler(rh_reg_state_t *rs, rh_bridge_data_t *cbd) {
    void (*handler)(rh_reg_state_t *rs, rh_bridge_data_t *cbd) = cbd->user_code;
    handler(rs, cbd);
}

int rh_bridge_init(void) {
    return 0;
}

rh_bridge_page_t *rh_bridge_alloc(void) {
    long page_size = sysconf(_SC_PAGESIZE);

    void *page = mmap(NULL, (size_t)page_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED)
        return NULL;

    if (mprotect(page, (size_t)page_size, PROT_READ | PROT_EXEC)) {
        munmap(page, (size_t)page_size);
        return NULL;
    }

    rh_bridge_page_t *bp = malloc(sizeof(rh_bridge_page_t));
    if (!bp) {
        munmap(page, (size_t)page_size);
        return NULL;
    }

    bp->page          = page;
    bp->enter_bridge  = (void *)rh_bridge_template;
    bp->leave_bridge  = (void *)rh_bridge_template;
    bp->page_size     = (size_t)page_size;

    return bp;
}

void rh_bridge_free(rh_bridge_page_t *page) {
    if (!page)
        return;
    munmap(page->page, page->page_size);
    free(page);
}

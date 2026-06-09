/*
 * Copyright (C) 2019-2024 jmpews@gmail.com
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

#pragma once
#include <stdbool.h>
#include <stdint.h>

#define RH_ADRP_ALIGN(n, a) (((uintptr_t)(n) + (a) - 1) & ~((uintptr_t)(a) - 1))

static inline bool
rh_adrp_can_reach(uintptr_t from, uintptr_t to) {
    uint64_t distance;

    if (from > to)
        distance = from - to;
    else
        distance = to - from;

    return distance < ((uint64_t)1 << 32);
}

static inline void
rh_adrp_emit_trampoline(uint32_t *buf, uintptr_t from, uintptr_t to) {
    uintptr_t from_page = RH_ADRP_ALIGN(from, 0x1000);
    uintptr_t to_page   = RH_ADRP_ALIGN(to, 0x1000);
    int64_t   page_off  = (int64_t)((to_page - from_page) >> 12);
    uintptr_t to_pgoff  = to & 0xFFF;

    uint32_t immlo = (page_off & 3) << 29;
    uint32_t immhi = ((page_off >> 2) & 0x7FFFF) << 5;

    buf[0] = 0x90000000 | immlo | immhi | 17;          /* ADRP X17, page_of(to) */
    buf[1] = 0x91000000 | (to_pgoff << 10) | (17 << 5) | 17; /* ADD X17, X17, #(to & 0xFFF) */
    buf[2] = 0xD61F0000 | (17 << 5);                   /* BR X17 */
}

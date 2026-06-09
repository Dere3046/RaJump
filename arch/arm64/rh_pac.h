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

#if defined(__arm64__) && __has_feature(ptrauth_calls)
#include <ptrauth.h>
#endif

static inline bool
rh_pac_is_sign(uint32_t insn) {
    return insn == 0xD503233F || insn == 0xD503237F;
}

static inline bool
rh_pac_is_auth(uint32_t insn) {
    return insn == 0xD50323BF || insn == 0xD50323FF;
}

static inline bool
rh_pac_is_hint_pac(uint32_t insn) {
    return (insn & 0xFFFFFF00) == 0xD5032300;
}

static inline uintptr_t
rh_pac_strip(uintptr_t ptr) {
    if (ptr == 0)
        return 0;

#if defined(__arm64__) && __has_feature(ptrauth_calls)
    return (uintptr_t)ptrauth_strip((void *)ptr, ptrauth_key_asia);
#elif defined(__aarch64__)
    __asm__ __volatile__("xpaclri" : "+r"(ptr));
    return ptr;
#else
    return ptr;
#endif
}

static inline uintptr_t
rh_pac_sign(uintptr_t ptr, uintptr_t context) {
    (void)context;
    if (ptr == 0)
        return 0;

#if defined(__arm64__) && __has_feature(ptrauth_calls)
    return (uintptr_t)ptrauth_sign_unauthenticated((void *)ptr, ptrauth_key_asia, 0);
#elif defined(__aarch64__)
    uintptr_t dst;
    __asm__ __volatile__("paciza %0" : "=r"(dst) : "r"(ptr));
    return dst;
#else
    return ptr;
#endif
}

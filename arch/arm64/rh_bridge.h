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

#ifndef RH_BRIDGE_ARM64_H
#define RH_BRIDGE_ARM64_H

#include <stddef.h>
#include <stdint.h>

typedef union {
    __int128_t q;
    struct {
        double d0;
        double d1;
    } d;
} rh_fp_reg_t;

typedef struct {
    uint64_t pad;
    uint64_t x[29];
    uint64_t fp;
    uint64_t lr;
    rh_fp_reg_t q[8];
} rh_reg_state_t;

typedef struct {
    void *user_code;
    void *user_data;
    void *next_hop;
} rh_bridge_data_t;

typedef struct {
    void *page;
    void *enter_bridge;
    void *leave_bridge;
    size_t page_size;
} rh_bridge_page_t;

rh_bridge_page_t *rh_bridge_alloc(void);
void rh_bridge_free(rh_bridge_page_t *page);

int rh_bridge_init(void);

#endif

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

#ifndef RH_BRIDGE_ARM_H
#define RH_BRIDGE_ARM_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t dummy_0;
    uint32_t dummy_1;
    uint32_t r[13];
    uint32_t lr;
} rh_arm_reg_state_t;

typedef struct {
    void *user_code;
    void *user_data;
    void *next_hop;
} rh_arm_bridge_data_t;

typedef struct {
    void *page;
    void *enter_bridge;
    void *leave_bridge;
    size_t page_size;
} rh_arm_bridge_page_t;

rh_arm_bridge_page_t *rh_arm_bridge_alloc(void);
void rh_arm_bridge_free(rh_arm_bridge_page_t *page);

int rh_bridge_init(void);

#endif

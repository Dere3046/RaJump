// Copyright (c) 2024-2026 dere3046
//
// Portions derived from ShadowHook:
// Copyright (c) 2021-2025 ByteDance Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// ...
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND...

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RH_RECORDER_OP_HOOK_INSTR_ADDR      0
#define RH_RECORDER_OP_HOOK_FUNC_ADDR       1
#define RH_RECORDER_OP_HOOK_SYM_ADDR        2
#define RH_RECORDER_OP_HOOK_SYM_NAME        3
#define RH_RECORDER_OP_UNHOOK               4
#define RH_RECORDER_OP_INTERCEPT_INSTR_ADDR 5
#define RH_RECORDER_OP_INTERCEPT_FUNC_ADDR  6
#define RH_RECORDER_OP_INTERCEPT_SYM_ADDR   7
#define RH_RECORDER_OP_INTERCEPT_SYM_NAME   8
#define RH_RECORDER_OP_UNINTERCEPT          9

bool rh_recorder_get_recordable(void);
void rh_recorder_set_recordable(bool recordable);

int rh_recorder_add_op(int error_number, uint8_t op, uintptr_t sym_addr, const char *lib_name,
                       const char *sym_name, uintptr_t new_addr, uint32_t flags, size_t backup_len,
                       uintptr_t stub, uintptr_t caller_addr, const char *caller_lib_name);
int rh_recorder_add_unop(int error_number, uint8_t op, uintptr_t stub, uintptr_t caller_addr,
                         const char *caller_lib_name);

char *rh_recorder_get(uint32_t item_flags);
void rh_recorder_dump(int fd, uint32_t item_flags);

// compat wrappers for rahook.c
void rh_recorder_start(void);
void rh_recorder_stop(void);
bool rh_recorder_is_active(void);
char *rh_recorder_export(void);
void rh_recorder_free(char *json);
int rh_recorder_add(const char *lib_name, const char *sym_name, uintptr_t target_addr,
                    uintptr_t new_addr, uintptr_t *orig_addr, uint32_t flags, size_t backup_len,
                    uintptr_t stub);

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define RH_INVOCATION_STACK_DEPTH 32

typedef struct {
    uintptr_t hook_target;
    uintptr_t caller_ret_addr;
    uintptr_t on_leave_addr;
} rh_invocation_entry_t;

typedef struct {
    rh_invocation_entry_t entries[RH_INVOCATION_STACK_DEPTH];
    int depth;
} rh_invocation_stack_t;

rh_invocation_stack_t *rh_invocation_stack_get(void);

void rh_invocation_push(uintptr_t hook_target, uintptr_t caller_lr);

void rh_invocation_pop(void);

const rh_invocation_entry_t *rh_invocation_peek(void);

int rh_invocation_depth(void);

uintptr_t rh_invocation_translate_lr(uintptr_t return_address);

bool rh_invocation_is_active(uintptr_t hook_target);

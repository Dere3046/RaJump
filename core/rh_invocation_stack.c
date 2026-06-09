#include "rh_invocation_stack.h"

static _Thread_local rh_invocation_stack_t g_stack = {{{0}}, 0};

rh_invocation_stack_t *rh_invocation_stack_get(void) {
    return &g_stack;
}

void rh_invocation_push(uintptr_t hook_target, uintptr_t caller_lr) {
    if (g_stack.depth < RH_INVOCATION_STACK_DEPTH) {
        g_stack.entries[g_stack.depth].hook_target = hook_target;
        g_stack.entries[g_stack.depth].caller_ret_addr = caller_lr;
        g_stack.entries[g_stack.depth].on_leave_addr = 0;
        g_stack.depth++;
    }
}

void rh_invocation_pop(void) {
    if (g_stack.depth > 0)
        g_stack.depth--;
}

const rh_invocation_entry_t *rh_invocation_peek(void) {
    if (g_stack.depth > 0)
        return &g_stack.entries[g_stack.depth - 1];
    return NULL;
}

int rh_invocation_depth(void) {
    return g_stack.depth;
}

uintptr_t rh_invocation_translate_lr(uintptr_t return_address) {
    if (g_stack.depth > 0) {
        const rh_invocation_entry_t *top = &g_stack.entries[g_stack.depth - 1];
        if (top->on_leave_addr != 0 && top->on_leave_addr == return_address)
            return top->caller_ret_addr;
    }
    return return_address;
}

bool rh_invocation_is_active(uintptr_t hook_target) {
    for (int i = 0; i < g_stack.depth; i++) {
        if (g_stack.entries[i].hook_target == hook_target)
            return true;
    }
    return false;
}

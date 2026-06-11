#pragma once
#include <stddef.h>
#include <stdint.h>

// error codes
#define RAHOOK_ERRNO_OK              0
#define RAHOOK_ERRNO_INIT_HUB       -10
#define RAHOOK_ERRNO_INVALID_ARG    -1
#define RAHOOK_ERRNO_NOT_FOUND      -2
#define RAHOOK_ERRNO_NO_MEMORY      -3
#define RAHOOK_ERRNO_HOOK_CFG_UNSAFE -11
#define RAHOOK_ERRNO_HOOK_ISLAND_REWRITE -12
#define RAHOOK_ERRNO_HOOK_ENTER -13
#define RAHOOK_ERRNO_HOOK_SYMSZ -14

// likely/unlikely
#define __predict_true(x)   __builtin_expect(!!(x), 1)
#define __predict_false(x)  __builtin_expect(!!(x), 0)

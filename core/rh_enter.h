#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>

static inline uintptr_t rh_enter_alloc(void) {
    return (uintptr_t)mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}
static inline void rh_enter_free(uintptr_t addr) {
    if (addr) munmap((void*)addr, 4096);
}

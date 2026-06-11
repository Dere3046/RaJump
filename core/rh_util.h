#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define RH_PROT_RWX (PROT_READ | PROT_WRITE | PROT_EXEC)
#define RH_PROT_RX  (PROT_READ | PROT_EXEC)

#define rh_util_min(a, b) (((a) < (b)) ? (a) : (b))
#define rh_util_max(a, b) (((a) > (b)) ? (a) : (b))

#define RH_UTIL_GET_BITS_32(n, high, low)  ((uint32_t)((uint32_t)(n) << (31u - (uint32_t)(high))) >> (31u - (uint32_t)(high) + (uint32_t)(low)))
#define RH_UTIL_SIGN_EXTEND_64(n, len)     ((int64_t)((uint64_t)(n) << (64u - (uint64_t)(len))) >> (64u - (uint64_t)(len)))

#define rh_util_align_up(x, align)   (((uintptr_t)(x) + (uintptr_t)(align) - 1) & ~((uintptr_t)(align) - 1))
#define rh_util_align_down(x, align) ((uintptr_t)(x) & ~((uintptr_t)(align) - 1))

#if defined(__arm__)
  #define rh_util_set_bit0(x)    ((uintptr_t)(x) | 1u)
  #define rh_util_clear_bit0(x)  ((uintptr_t)(x) & 0xFFFFFFFEu)
  #define rh_util_is_thumb(x)    ((uintptr_t)(x) & 1u)
#else
  #define rh_util_set_bit0(x)    ((uintptr_t)(x))
  #define rh_util_clear_bit0(x)  ((uintptr_t)(x))
  #define rh_util_is_thumb(x)    false
#endif

#define rh_util_get_page_start(addr) ((uintptr_t)(addr) & ~(rh_page_size - 1))
#define rh_util_get_page_end(addr)   rh_util_get_page_start((uintptr_t)(addr) + rh_page_size - 1)

#define rh_util_is_within(a, start, end) ((uintptr_t)(a) >= (uintptr_t)(start) && (uintptr_t)(a) <= (uintptr_t)(end))

static size_t rh_page_size = 0;
#ifndef PAGE_SIZE
#define PAGE_SIZE    rh_page_size
#endif
#define RH_PAGE_SIZE rh_page_size

static int (*rh_real_mprotect)(void *, size_t, int) = NULL;

static inline void rh_util_init(void) {
    rh_page_size = (size_t)sysconf(_SC_PAGESIZE);
    rh_real_mprotect = &mprotect;
}

static inline int rh_util_mprotect(uintptr_t addr, size_t len, int prot) {
    uintptr_t start = rh_util_get_page_start(addr);
    uintptr_t end = rh_util_get_page_end(addr + len - 1);
    return rh_real_mprotect((void *)start, end - start, prot);
}

#if defined(__aarch64__)
static inline void rh_util_cache_flush(uintptr_t addr, size_t size) {
    uintptr_t end = addr + size;
    uintptr_t line = addr & ~(63UL);
    for (; line < end; line += 64) {
        __asm__ volatile("dc cvau, %0" : : "r"(line) : "memory");
    }
    __asm__ volatile("dsb ish" ::: "memory");
    for (line = addr & ~(63UL); line < end; line += 64) {
        __asm__ volatile("ic ivau, %0" : : "r"(line) : "memory");
    }
    __asm__ volatile("dsb ish; isb" ::: "memory");
}
#elif defined(__arm__)
static inline void rh_util_cache_flush(uintptr_t addr, size_t size) {
    __builtin___clear_cache((char *)addr, (char *)(addr + size));
}
#else
static inline void rh_util_cache_flush(uintptr_t addr, size_t size) {
    (void)addr;
    (void)size;
}
#endif

static inline int rh_util_write_inst(void *addr, const void *code, size_t size) {
    if (0 != mprotect((void *)((uintptr_t)addr & ~(size_t)4095), 4096, PROT_READ | PROT_WRITE | PROT_EXEC))
        return -1;
    memcpy(addr, code, size);
    rh_util_cache_flush((uintptr_t)addr, size);
    return 0;
}

static inline void rh_util_write_and_flush(uintptr_t addr, const void *buf, size_t size) {
    memcpy((void *)addr, buf, size);
    rh_util_cache_flush(addr, size);
}

static inline void rh_util_clear_cache(uintptr_t addr, size_t size) {
    rh_util_cache_flush(addr, size);
}

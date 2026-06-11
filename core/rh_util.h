#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>
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
  #define RH_UTIL_SET_BIT0(x)    rh_util_set_bit0(x)
#define RH_UTIL_CLEAR_BIT0(x)  rh_util_clear_bit0(x)
#define RH_UTIL_IS_THUMB(x)    rh_util_is_thumb(x)
  #define rh_util_clear_bit0(x)  ((uintptr_t)(x) & 0xFFFFFFFEu)
  #define rh_util_is_thumb(x)    ((uintptr_t)(x) & 1u)
#else
  #define rh_util_set_bit0(x)    ((uintptr_t)(x))
  #define rh_util_clear_bit0(x)  ((uintptr_t)(x))
  #define rh_util_is_thumb(x)    false
#endif

#define rh_util_get_page_start(addr) ((uintptr_t)(addr) & ~(rh_page_size - 1))
#define rh_util_get_page_end(addr)   rh_util_get_page_start((uintptr_t)(addr) + rh_page_size - 1)
#define RH_UTIL_ARM_CPU_FEATURE_VFPV3D16  0x2
#define RH_UTIL_ARM_CPU_FEATURE_VFPV3D32  0x4

static inline size_t rh_util_get_arm_cpu_features(void) { return 0x4; }

#define rh_util_is_within(a, start, end) ((uintptr_t)(a) >= (uintptr_t)(start) && (uintptr_t)(a) <= (uintptr_t)(end))

static size_t rh_page_size = 0;
#ifndef PAGE_SIZE
#define PAGE_SIZE    rh_page_size
#endif
#define RH_PAGE_SIZE rh_page_size

static int (*rh_real_mprotect)(void *, size_t, int) = NULL;

#define RH_REGION_CACHE_MAX 1024

typedef struct {
    uintptr_t start;
    uintptr_t end;
    unsigned int perms;
} rh_region_t;

static rh_region_t g_rh_regions[RH_REGION_CACHE_MAX];
static size_t g_rh_region_count = 0;

static void rh_region_cache_init(void) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f) && g_rh_region_count < RH_REGION_CACHE_MAX) {
        uintptr_t start, end;
        char perms[5];
        if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) >= 3) {
            g_rh_regions[g_rh_region_count].start = start;
            g_rh_regions[g_rh_region_count].end = end;
            g_rh_regions[g_rh_region_count].perms =
                ((perms[0] == 'r') ? PROT_READ : 0) |
                ((perms[1] == 'w') ? PROT_WRITE : 0) |
                ((perms[2] == 'x') ? PROT_EXEC : 0);
            g_rh_region_count++;
        }
    }
    fclose(f);
}

static int rh_region_find_perm(uintptr_t addr) {
    for (size_t i = 0; i < g_rh_region_count; i++)
        if (addr >= g_rh_regions[i].start && addr < g_rh_regions[i].end)
            return (int)g_rh_regions[i].perms;
    return PROT_READ | PROT_EXEC;
}

static inline void rh_util_init(void) {
    rh_page_size = (size_t)sysconf(_SC_PAGESIZE);
    rh_real_mprotect = &mprotect;
    rh_region_cache_init();
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

// mremap-based atomic page replacement — Linux 5.13+ (MREMAP_DONTUNMAP)
static inline int rh_mremap_patch(uintptr_t page_start, size_t page_size,
                                   const void *code, size_t code_size) {
    void *backup = mmap(NULL, page_size, PROT_NONE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (backup == MAP_FAILED) return -1;

    void *r = mremap((void *)page_start, page_size, page_size,
                     MREMAP_FIXED | MREMAP_MAYMOVE | MREMAP_DONTUNMAP, backup);
    if (r == MAP_FAILED || r != backup) {
        munmap(backup, page_size);
        return -1;
    }

    r = mmap((void *)page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
    if (r == MAP_FAILED) {
        mremap(backup, page_size, page_size, MREMAP_FIXED | MREMAP_MAYMOVE, (void *)page_start);
        munmap(backup, page_size);
        return -1;
    }

    memcpy((void *)page_start, code, code_size);
    munmap(backup, page_size);
    return 0;
}

typedef struct {
    uintptr_t pages[256];
    size_t count;
} rh_flush_batch_t;

static inline void rh_flush_batch_init(rh_flush_batch_t *b) { b->count = 0; }

static inline void rh_flush_batch_add(rh_flush_batch_t *b, uintptr_t addr, size_t len) {
    uintptr_t start = addr & ~(uintptr_t)4095;
    uintptr_t end = (addr + len - 1) & ~(uintptr_t)4095;
    for (uintptr_t p = start; p <= end; p += 4096) {
        int dup = 0;
        for (size_t i = 0; i < b->count; i++)
            if (b->pages[i] == p) { dup = 1; break; }
        if (!dup && b->count < 256) b->pages[b->count++] = p;
    }
}

static inline void rh_flush_batch_commit(rh_flush_batch_t *b) {
    for (size_t i = 0; i < b->count; i++)
        __builtin___clear_cache((void *)b->pages[i], (void *)(b->pages[i] + 4096));
}

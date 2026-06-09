#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct {
    void *rx_addr;
    void *rw_addr;
    size_t size;
} rh_code_page_t;

static inline int rh_code_alloc(size_t size, rh_code_page_t *page) {
    memset(page, 0, sizeof(*page));
    page->size = (size + 4095) & ~4095UL;

#if defined(__linux__) && defined(MFD_CLOEXEC)
    int fd = (int)syscall(426 /* __NR_memfd_create */, "rahook_code", 1 /* MFD_CLOEXEC */);
    if (fd >= 0) {
        if ((size_t)syscall(77 /* __NR_ftruncate */, fd, (off_t)page->size) == 0) {
            page->rx_addr = mmap(NULL, page->size, PROT_READ | PROT_EXEC,
                                 MAP_SHARED, fd, 0);
            page->rw_addr = mmap(NULL, page->size, PROT_READ | PROT_WRITE,
                                 MAP_SHARED, fd, 0);
        }
        close(fd);
        if (page->rx_addr != MAP_FAILED && page->rw_addr != MAP_FAILED &&
            page->rx_addr != page->rw_addr) {
            return 0;
        }
        if (page->rx_addr != MAP_FAILED) munmap(page->rx_addr, page->size);
        if (page->rw_addr != MAP_FAILED) munmap(page->rw_addr, page->size);
        memset(page, 0, sizeof(*page));
    }
#endif

    page->rx_addr = page->rw_addr = mmap(NULL, page->size,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page->rx_addr == MAP_FAILED) return -1;
    return 0;
}

static inline void rh_code_write(rh_code_page_t *page, size_t offset,
                                  const void *data, size_t size) {
    memcpy((uint8_t *)page->rw_addr + offset, data, size);
}

static inline void rh_code_flush(rh_code_page_t *page) {
    (void)page;
#if defined(__aarch64__)
    extern void __clear_cache(void *, void *);
    __clear_cache(page->rx_addr, (uint8_t *)page->rx_addr + page->size);
#endif
}

static inline void rh_code_free(rh_code_page_t *page) {
    if (page->rx_addr) munmap(page->rx_addr, page->size);
    if (page->rw_addr && page->rw_addr != page->rx_addr) munmap(page->rw_addr, page->size);
    memset(page, 0, sizeof(*page));
}

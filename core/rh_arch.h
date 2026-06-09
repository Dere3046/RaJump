#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __aarch64__
  #define RH_ARCH_ARM64       1
  #define RH_ISLAND_RANGE     0x8000000
  #define RH_ISLAND_EXIT_SIZE 16
  #define RH_ISLAND_ENTER_SIZE 8
  #define RH_INSN_ALIGN       4
#elif defined(__arm__)
  #define RH_ARCH_ARM         1
  #define RH_ISLAND_RANGE     0x2000000
  #define RH_ISLAND_EXIT_SIZE 8
  #define RH_ISLAND_ENTER_SIZE 8
  #define RH_INSN_ALIGN       4
#elif defined(__i386__)
  #define RH_ARCH_X86         1
  #define RH_ISLAND_RANGE     0x80000000
  #define RH_ISLAND_EXIT_SIZE 6
  #define RH_ISLAND_ENTER_SIZE 6
  #define RH_INSN_ALIGN       1
#elif defined(__x86_64__)
  #define RH_ARCH_X86_64      1
  #define RH_ISLAND_RANGE     0x80000000
  #define RH_ISLAND_EXIT_SIZE 14
  #define RH_ISLAND_ENTER_SIZE 14
  #define RH_INSN_ALIGN       1
#else
  #error "Unsupported architecture"
#endif

typedef struct {
    int (*hook)(void *target, void *replace, void **origin, uint8_t *backup, size_t *backup_len);
    int (*unhook)(void *target, uint8_t *backup, size_t backup_len);
    int (*get_rewrite_size)(void *addr, size_t *size);
} rh_arch_ops_t;

extern const rh_arch_ops_t *rh_arch_ops;

void rh_arch_init(void);

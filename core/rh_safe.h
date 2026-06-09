#pragma once
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

int rh_safe_init(void);

void *rh_safe_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int rh_safe_munmap(void *addr, size_t size);
int rh_safe_mprotect(void *addr, size_t len, int prot);
void *rh_safe_malloc(size_t size);
void rh_safe_free(void *ptr);
int rh_safe_pthread_mutex_lock(pthread_mutex_t *mutex);
int rh_safe_pthread_mutex_unlock(pthread_mutex_t *mutex);
void rh_safe_abort(void);

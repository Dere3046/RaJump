#pragma once
#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <unistd.h>

int rh_bytesig_init(int signum);
void rh_bytesig_protect(pid_t tid, sigjmp_buf *jbuf, const int signums[], size_t cnt);
void rh_bytesig_unprotect(pid_t tid, const int signums[], size_t cnt);

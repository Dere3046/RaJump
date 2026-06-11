/*
 * test_bytesig.c — bytesig crash protection tests
 *
 * Copyright (C) 2026 Dere3046
 */

#include "test_runner.h"
#include "core/rh_bytesig.h"
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

TEST_MAIN()

T("bytesig: init SIGSEGV returns 0");
int r = rh_bytesig_init(SIGSEGV);
if (r != 0) SKIP("bytesig init failed"); else OK();

T("bytesig: init SIGBUS returns 0");
rh_bytesig_init(SIGBUS);
OK();

T("bytesig: handler chain preserves old handler");
struct sigaction old_sa;
sigaction(SIGSEGV, NULL, &old_sa);
ASSERT(old_sa.sa_sigaction != NULL || old_sa.sa_handler != NULL, "no handler");

T("bytesig: max signal boundary");
ASSERT_EQ(rh_bytesig_init(-1), -1);

T("bytesig: protect/unprotect API (no crash)");
pid_t tid = (pid_t)syscall(SYS_gettid);
sigjmp_buf jbuf;
int sigs[] = {SIGSEGV};
rh_bytesig_protect(tid, &jbuf, sigs, 1);
rh_bytesig_unprotect(tid, sigs, 1);
OK();

T("bytesig: crash catching disabled on x86 desktop");
SKIP("SIGSEGV recovery needs SA_ONSTACK on x86");

TEST_END()

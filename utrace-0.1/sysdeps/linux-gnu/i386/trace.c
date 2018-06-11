//Simon scanned
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <asm/ptrace.h>

#include "ltrace.h"

#if (!defined(PTRACE_PEEKUSER) && defined(PTRACE_PEEKUSR))
# define PTRACE_PEEKUSER PTRACE_PEEKUSR
#endif

#if (!defined(PTRACE_POKEUSER) && defined(PTRACE_POKEUSR))
# define PTRACE_POKEUSER PTRACE_POKEUSR
#endif

void
get_arch_dep(struct thread * proc) {
}

/* Returns 1 if syscall, 2 if sysret, 0 otherwise.
 */
int
syscall_p(struct thread * thr, int status, int * sysnum) {
	//printf("Simon: >> syscall_p: status = %x\n", status);
	if (WIFSTOPPED(status) && WSTOPSIG(status)==SIGTRAP) {
		*sysnum = ptrace(PTRACE_PEEKUSER, thr->tid, 4*ORIG_EAX, 0);

	        //printf("Simon: syscall_p: status WIFSTOPPED & WSTOPSIG, callstack_depth = %d, is_syscall=%d\n",
	        //			thr->callstack_depth, thr->callstack[thr->callstack_depth-1].is_syscall);
		if (thr->callstack_depth > 0 &&
		    thr->callstack[thr->callstack_depth-1].is_syscall &&
		    thr->callstack[thr->callstack_depth-1].c_un.syscall == *sysnum) {
			return 2;
		}

		if (*sysnum>=0) {
			return 1;
		}
	}
	return 0;
}

long
gimme_arg(enum tof type, struct thread * thr, int arg_num) {
	if (arg_num==-1) {		/* return value */
		return ptrace(PTRACE_PEEKUSER, thr->tid, 4*EAX, 0);
	}

	if (type==LT_TOF_FUNCTION || type==LT_TOF_FUNCTIONR) {
		return ptrace(PTRACE_PEEKTEXT, thr->tid, thr->stack_pointer+4*(arg_num+1), 0);
	} else if (type==LT_TOF_SYSCALL || type==LT_TOF_SYSCALLR) {
#if 0
		switch(arg_num) {
			case 0:	return ptrace(PTRACE_PEEKUSER, thr->tid, 4*EBX, 0);
			case 1:	return ptrace(PTRACE_PEEKUSER, thr->tid, 4*ECX, 0);
			case 2:	return ptrace(PTRACE_PEEKUSER, thr->tid, 4*EDX, 0);
			case 3:	return ptrace(PTRACE_PEEKUSER, thr->tid, 4*ESI, 0);
			case 4:	return ptrace(PTRACE_PEEKUSER, thr->tid, 4*EDI, 0);
			default:
				fprintf(stderr, "gimme_arg called with wrong arguments\n");
				exit(2);
		}
#else
		return ptrace(PTRACE_PEEKUSER, thr->tid, 4*arg_num, 0);
#endif
	} else {
		fprintf(stderr, "gimme_arg called with wrong arguments\n");
		exit(1);
	}

	return 0;
}

void
save_register_args(enum tof type, struct thread * proc) {
}

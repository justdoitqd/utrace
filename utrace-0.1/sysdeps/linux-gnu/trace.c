#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include "ptrace.h"
#include <asm/unistd.h>

#include "ltrace.h"
#include "options.h"

/* If the system headers did not provide the constants, hard-code the normal
 *    values.  */
#ifndef PTRACE_EVENT_FORK

#define PTRACE_SETOPTIONS       0x4200                   
#define PTRACE_GETEVENTMSG      0x4201

/* options set using PTRACE_SETOPTIONS */             
#define PTRACE_O_TRACESYSGOOD   0x00000001  
#define PTRACE_O_TRACEFORK      0x00000002
#define PTRACE_O_TRACEVFORK     0x00000004            
#define PTRACE_O_TRACECLONE     0x00000008 
#define PTRACE_O_TRACEEXEC      0x00000010
#define PTRACE_O_TRACEVFORKDONE 0x00000020 
#define PTRACE_O_TRACEEXIT      0x00000040 

/* Wait extended result codes for the above trace options.  */
#define PTRACE_EVENT_FORK       1
#define PTRACE_EVENT_VFORK      2
#define PTRACE_EVENT_CLONE      3
#define PTRACE_EVENT_EXEC       4
#define PTRACE_EVENT_VFORK_DONE 5
#define PTRACE_EVENT_EXIT       6

#endif /* PTRACE_EVENT_FORK */

unsigned long ptrace_gettid(pid_t pid)
{
	 unsigned long tid = 0;
         if (ptrace(PTRACE_GETEVENTMSG, pid, 0, &tid) != -1)
	 {
	 }
	 return tid;
}

/* Returns 1 if the sysnum may make a new child to be created
 * (ie, with fork() or clone())
 * Returns 0 otherwise.
 */
int
fork_p(int sysnum) {
        return 0
#if defined(__NR_fork)
                || (sysnum == __NR_fork)
#endif
#if defined(__NR_vfork)
                || (sysnum == __NR_vfork)
#endif
                ;
}
/* Returns 1 if the sysnum may clone a new child to be created
 * (ie, with or clone())
 * Returns 0 otherwise.
 * Simon: I assume thread create will use clone();
 *                 process create will use fork()/vfork();
 */
int
clone_p(int sysnum) {
        return 0
#if defined(__NR_clone)
                || (sysnum == __NR_clone)
#endif
                ;
}

/* Returns 1 if the sysnum may make the process exec other program
 */
int
exec_p(int sysnum) {
        return (sysnum == __NR_execve);
}

void
trace_me(void) {
        if (ptrace(PTRACE_TRACEME, 0, 1, 0)<0) {
                perror("PTRACE_TRACEME");
                exit(1);
        }
}

/* Simon: need set some option to enable tracing child process/threads:
 * TODO: The perfect implementation will require to "test" whether this linux version
 * support trace_fork:
 *    refer to linux_test_for_tracefork() in "gdb"'s source code
 * This part work is skipped at present. We always think current system support trace_fork.
 */ 

int linux_supports_tracefork(pid_t pid)
{
        return 1;
}

static int current_ptrace_options = PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACEEXEC | PTRACE_O_TRACECLONE|PTRACE_O_TRACEVFORKDONE;
int trace_option(pid_t pid)
{
	//the option will be set at enable_all_breakpoints
	return 1;

	printf("Simon: >> trace_option: %d\n", pid);
        if (pid == 0)
                 return 0;

        if (! linux_supports_tracefork (pid))
                 return 0;                                                

        if (ptrace(PTRACE_SETOPTIONS, pid, 0, (void*)current_ptrace_options) == -1)
	{
		perror("PTRACE_SETOPTION \n");
		return 0;
	}
        return 1;
}


int
trace_pid(pid_t pid) {
	printf("Simon: >> trace_pid:%d\n", pid);
        if (ptrace(PTRACE_ATTACH, pid, 1, 0) < 0) {
                perror("trace_pid");
                return -1;
        }
        return trace_option(pid);
}

void
untrace_pid(pid_t pid) {
        ptrace(PTRACE_DETACH, pid, 1, 0);
}

void
continue_after_signal(pid_t pid, int signum) {
        /* We should always trace syscalls to be able to control fork(), clone(), execve()... */
        ptrace(PTRACE_SYSCALL, pid, 0, signum);
}

void
continue_process(pid_t pid) {
        continue_after_signal(pid, 0);
}

void
continue_enabling_breakpoint(pid_t pid, struct breakpoint * sbp) {
        enable_breakpoint(pid, sbp);
        continue_process(pid);
}

void
continue_after_breakpoint(struct thread *thr, struct breakpoint * sbp) {
        if (sbp->enabled) disable_breakpoint(thr->tid, sbp);
        set_instruction_pointer(thr->tid, sbp->addr);
        if (sbp->enabled == 0) {
                continue_process(thr->tid);
        } else {
                thr->breakpoint_being_enabled = sbp;
#ifdef __sparc__
                continue_process(thr->tid);
#else
                ptrace(PTRACE_SINGLESTEP, thr->tid, 0, 0);
#endif
        }
}

int
umovestr(struct thread * thr, void * addr, int len, void * laddr) {
        union { long a; char c[sizeof(long)]; } a;
        int i;
        int offset=0;

        while(offset<len) {
                a.a = ptrace(PTRACE_PEEKTEXT, thr->tid, addr+offset, 0);
                for(i=0; i<sizeof(long); i++) {
                        if (a.c[i] && offset+i < len) {
                                *(char *)(laddr+offset+i) = a.c[i];
                        } else {
                                *(char *)(laddr+offset+i) = '\0';
                                return 0;
                        }
                }
                offset += sizeof(long);
        }
        *(char *)(laddr+offset) = '\0';
        return 0;
}

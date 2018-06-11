//Simon scanned
#if HAVE_CONFIG_H
#include "config.h"
#endif

#define	_GNU_SOURCE	1
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include "ltrace.h"
#include "options.h"
#include "debug.h"

#include <sys/ptrace.h>

static struct event event;

/* This should also update `current_process' */

static struct process * pid2proc(int pid);
static struct thread * tid2thread(int tid, struct process* proc);

/* Simon: function to detach */
static void simonDetach(pid_t pid)
{
	ptrace(PTRACE_DETACH, pid, 0, 0);
	while(1) sleep(1);
	return;
}
/*end */

struct event *
wait_for_something(void) {
	pid_t pid;
	int status;
	int tmp;
	//int my = 0;
	pid_t tid;

	if (!list_of_processes) {
		debug(1, "No more children");
		exit(0);
	}

	tid = waitpid(-1, &status, __WALL);
	if (tid==-1) {
		if (errno==ECHILD) {
			debug(1, "No more children");
			exit(0);
		} else if (errno==EINTR) {
			debug(1, "wait received EINTR ?");
			event.thing = LT_EV_NONE;
			return &event;
		}
		perror("wait");
		exit(1);
	}

	//memset(event, 0, sizeof(event));
	//Simon: the pid might be a LWP id; translate to thread group ID
	//TODO: pid2tgid name should be changed to tid2pid(tid);
	pid = pid2tgid(tid);
	event.proc = pid2proc(pid);
        debug(3, "Simon: wait() return tid %u, translates to pid %u, proc %x", tid, pid, event.proc);
	if (!event.proc) {
		fprintf(stderr, "signal from wrong tid %u ?!?, pid=%u\n", tid, pid);
		exit(1);
	}

	event.thr = tid2thread(tid, event.proc);
	if (event.thr == NULL)
	{
		event.thr = reg_tid(event.proc, tid);
	}

	/*Each time, we need to check the threadid */
	if (opt_f)
	{
		pid_t tmpId = ptrace_gettid(pid);
                debug(1, "Simon: ptrace_gettid return:%u, wait() return:%u, pid:%u", tmpId, tid, pid);
	}

	get_arch_dep(event.thr);
	event.thr->instruction_pointer = NULL;
	//debug(3, "signal from pid %u,tid= %u", pid, tid);
	if (event.proc->breakpoints_enabled == -1) {
		enable_all_breakpoints(event.proc);
		event.thing = LT_EV_NONE;
#if 0 //Simon
		simonDetach(pid);
		event.proc->breakpoints_enabled = -2;
		wait( &status);  //block here .......
#else
		continue_process(tid);
#endif

		return &event;
	}
	if (opt_i) {
		event.thr->instruction_pointer = get_instruction_pointer(event.thr->tid);
	}
		//printf("Simon: before syscall_p, event.thing = %d\n", event.thing); 
		//my = syscall_p(event.proc, status, &tmp);
		//printf("Simon: syscall_p return %d\n", my);
	switch(syscall_p(event.thr, status, &tmp)) {
		case 1:	event.thing = LT_EV_SYSCALL;
			event.e_un.sysnum = tmp;
			return &event;
		case 2:	event.thing = LT_EV_SYSRET;
			event.e_un.sysnum = tmp;
			return &event;
		default:
	        	break;
	}
		//printf("Simon: after syscall_p switch, event.thing = %d\n", event.thing); 
	if (WIFEXITED(status)) {
		event.thing = LT_EV_EXIT;
		event.e_un.ret_val = WEXITSTATUS(status);
		return &event;
	}
	if (WIFSIGNALED(status)) {
		event.thing = LT_EV_EXIT_SIGNAL;
		event.e_un.signum = WTERMSIG(status);
		return &event;
	}
	if (!WIFSTOPPED(status)) {
		event.thing = LT_EV_UNKNOWN;
		return &event;
	}
	if (WSTOPSIG(status) != SIGTRAP) {
		event.thing = LT_EV_SIGNAL;
		event.e_un.signum = WSTOPSIG(status);
		return &event;
	}
	event.thing = LT_EV_BREAKPOINT;
	if (!event.thr->instruction_pointer) {
		event.thr->instruction_pointer = get_instruction_pointer(event.thr->tid);
	}
	event.e_un.brk_addr = event.thr->instruction_pointer - DECR_PC_AFTER_BREAK;
	return &event;
}

static struct process *
pid2proc(pid_t pid) {
	struct process * tmp;
	debug(3, "Simon: [pid2proc]:pid=%u, list_of_processes=%x\n", pid, list_of_processes);

	tmp = list_of_processes;
	while(tmp) {
		debug(3, "Simon: tmp=%x, tmp->pid=%u\n", tmp, tmp->pid);
		if (pid == tmp->pid) {
			return tmp;
		}
		tmp = tmp->next;
	}
	return NULL;
}

static struct thread *
tid2thread(pid_t tid, struct process* proc) {
	if (proc == NULL)
		return NULL;

	if (tid == 0)
		return NULL;

        if ((proc->hot_tid_idx != -1) && (proc->threads[proc->hot_tid_idx].tid == tid))
	{
		return &proc->threads[proc->hot_tid_idx];
	}

	int i = 0;
	for (; i < MAX_THREADS_TRACED; i++)
	{
		if (proc->threads[i].tid == tid)
			return &proc->threads[i];
	}

	return NULL;
}

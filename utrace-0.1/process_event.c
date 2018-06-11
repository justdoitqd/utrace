//Simon scanned
#if HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <sys/time.h>

#include "ltrace.h"
#include "output.h"
#include "options.h"
#include "elf.h"
#include "debug.h"

#ifdef __powerpc__
#include <sys/ptrace.h>
#endif

static void process_signal(struct event * event);
static void process_exit(struct event * event);
static void process_exit_signal(struct event * event);
static void process_syscall(struct event * event);
static void process_sysret(struct event * event);
void process_breakpoint(struct event * event);
static void remove_proc(struct process * proc);

static void callstack_push_syscall(struct thread * proc, int sysnum);
static void callstack_push_symfunc(struct thread * proc, struct library_symbol * sym);
static void callstack_pop(struct thread * proc);

static char *
shortsignal(int signum) {
	static char * signalent0[] = {
	#include "signalent.h"
	};
	int nsignals0 = sizeof signalent0 / sizeof signalent0[0];

	if (signum < 0 || signum >= nsignals0) {
		return "UNKNOWN_SIGNAL";
	} else {
		return signalent0[signum];
	}
}

static char *
sysname(int sysnum) {
	static char result[128];
	static char * syscalent0[] = {
	#include "syscallent.h"
	};
	int nsyscals0 = sizeof syscalent0 / sizeof syscalent0[0];

	if (sysnum < 0 || sysnum >= nsyscals0) {
		sprintf(result, "SYS_%d", sysnum);
		return result;
	} else {
		sprintf(result, "SYS_%s", syscalent0[sysnum]);
		return result;
	}
}

void
process_event(struct event * event) {
	switch (event->thing) {
		case LT_EV_NONE:
			debug(1, "event: none");
			return;
		case LT_EV_SIGNAL:
			debug(1, "event: signal (%s [%d])", shortsignal(event->e_un.signum), event->e_un.signum);
			process_signal(event);
			return;
		case LT_EV_EXIT:
			debug(1, "event: exit (%d)", event->e_un.ret_val);
			process_exit(event);
			return;
		case LT_EV_EXIT_SIGNAL:
			debug(1, "event: exit signal (%s [%d])", shortsignal(event->e_un.signum), event->e_un.signum);
			process_exit_signal(event);
			return;
		case LT_EV_SYSCALL:
			debug(1, "event: syscall (%s [%d])", sysname (event->e_un.sysnum), event->e_un.sysnum);
			process_syscall(event);
			return;
		case LT_EV_SYSRET:
			debug(1, "event: sysret (%s [%d])", sysname (event->e_un.sysnum), event->e_un.sysnum);
			process_sysret(event);
			return;
		case LT_EV_BREAKPOINT:
			debug(1, "event: breakpoint");
			process_breakpoint(event);
			return;
		default:
			fprintf(stderr, "Error! unknown event?\n");
			exit(1);
	}
}

static void
process_signal(struct event * event) {
	if (exiting && event->e_un.signum == SIGSTOP) {
		pid_t pid = event->proc->pid;
		disable_all_breakpoints(event->proc);
		untrace_pid(pid);
		remove_proc(event->proc);
		continue_after_signal(event->thr->tid, event->e_un.signum);
		return;
	}
	output_line(event->thr, "--- %s (%s) ---",
		shortsignal(event->e_un.signum), strsignal(event->e_un.signum));
	continue_after_signal(event->thr->tid, event->e_un.signum);
}

static void
process_exit(struct event * event) {
	output_line(event->thr, "+++ exited (status %d) +++",
		event->e_un.ret_val);
	remove_proc(event->proc);
}

static void
process_exit_signal(struct event * event) {
	output_line(event->thr, "+++ killed by %s +++",
		shortsignal(event->e_un.signum));
	remove_proc(event->proc);
}

static void
remove_proc(struct process * proc) {
	struct process *tmp, *tmp2;

	debug(1, "Removing pid %u\n", proc->pid);
	/*Simon TODO: Need consider dereg_tid() instead of remove the whole process*/
	// ....

	if (list_of_processes == proc) {
		tmp = list_of_processes;
		list_of_processes = list_of_processes->next;
		free(tmp);
		return;
	}
	tmp = list_of_processes;
	while(tmp->next) {
		if (tmp->next==proc) {
			tmp2 = tmp->next;
			tmp->next = tmp2->next;
			free(tmp2);
			continue;
		}
		tmp = tmp->next;
	}
}

static void
process_syscall(struct event * event) {
	if (opt_S) {
		output_left(LT_TOF_SYSCALL, event->thr, sysname(event->e_un.sysnum));
	}
	if (clone_p(event->e_un.sysnum)) {
		//Simon: don't need to disable original breakpoints
		//disable_all_breakpoints(event->proc);
	} else if (!event->proc->breakpoints_enabled) {
		enable_all_breakpoints(event->proc);
	}
	if (!exec_p(event->e_un.sysnum)) //Simon: execv() has different behavior at different linux version, so skip execv()
		                         // e.g. It doesn't have sys_ret at linux version of lincase
	    callstack_push_syscall(event->thr, event->e_un.sysnum);
	continue_process(event->thr->tid);
}

struct timeval current_time_spent;

static void
calc_time_spent(struct thread * thr) {
	struct timeval tv;
	struct timezone tz;
	struct timeval diff;
	struct callstack_element * elem;

	elem = & thr->callstack[thr->callstack_depth-1];

	gettimeofday(&tv, &tz);

	diff.tv_sec = tv.tv_sec - elem->time_spent.tv_sec;
	if (tv.tv_usec >= elem->time_spent.tv_usec) {
		diff.tv_usec = tv.tv_usec - elem->time_spent.tv_usec;
	} else {
		diff.tv_sec++;
		diff.tv_usec = 1000000 + tv.tv_usec - elem->time_spent.tv_usec;
	}
	current_time_spent = diff;
}

static void
process_sysret(struct event * event) {

	if (opt_T || opt_c) {
		calc_time_spent(event->thr);
	}

	if (fork_p(event->e_un.sysnum)) {
		if (opt_f) {
			pid_t child = gimme_arg(LT_TOF_SYSCALLR,event->thr,-1);
			if (child>0) {
	                         debug(1, "fork new child:%d", child);
			         open_pid(child, 0);
			}
		        enable_all_breakpoints(event->thr);
		}
	}
	if (clone_p(event->e_un.sysnum)) {
		if (opt_f) {
			pid_t child = gimme_arg(LT_TOF_SYSCALLR,event->thr,-1);
			if (child>0) {
	                         debug(1, "clone new child:%d", child);
			}
			reg_tid(event->proc, child);
		}
	}
	//Simon: execv() has different behavior at different linux version, so skip execv()
	if (!exec_p(event->e_un.sysnum)) 
	        callstack_pop(event->thr);
	if (opt_S) {
		output_right(LT_TOF_SYSCALLR, event->thr, sysname(event->e_un.sysnum));
	}
	if (exec_p(event->e_un.sysnum)) {
		if (gimme_arg(LT_TOF_SYSCALLR,event->thr,-1)==0) {
			event->proc->filename = pid2name(event->proc->pid);
			breakpoints_init(event->proc);
		}
	}
	//printf("Simon:to continue %d\n", event->thr->tid);
	continue_process(event->thr->tid);
}

void
process_breakpoint(struct event * event) {
	struct library_symbol * tmp;
	int i,j;

	debug(2, "event: breakpoint (%p)", event->e_un.brk_addr);
	if (event->thr->breakpoint_being_enabled) {
		/* Reinsert breakpoint */
		continue_enabling_breakpoint(event->thr->tid, event->thr->breakpoint_being_enabled);
		event->thr->breakpoint_being_enabled = NULL;
		return;
	}

	for(i=event->thr->callstack_depth-1; i>=0; i--) {
		if (event->e_un.brk_addr == event->thr->callstack[i].return_addr) {
#ifdef __powerpc__
                       unsigned long a;
                       unsigned long addr = event->thr->callstack[i].c_un.libfunc->enter_addr;
                       struct breakpoint *sbp = address2bpstruct(event->proc, addr);
                       unsigned char break_insn[] = BREAKPOINT_VALUE;

                       /*
                        * PPC HACK! (XXX FIXME TODO)
                        * The PLT gets modified during the first call,
                        * so be sure to re-enable the breakpoint.
                        */
                       a = ptrace(PTRACE_PEEKTEXT, event->thr->tid, addr);

                       if (memcmp(&a, break_insn, 4)) {
                               sbp->enabled--;
                               insert_breakpoint(event->proc, addr);
                       }
#endif
			for(j=event->thr->callstack_depth-1; j>i; j--) {
				callstack_pop(event->thr);
			}
			if (opt_T || opt_c) {
				calc_time_spent(event->thr);
			}
			callstack_pop(event->thr);
			event->thr->return_addr = event->e_un.brk_addr;
			output_right(LT_TOF_FUNCTIONR, event->thr,
					event->thr->callstack[i].c_un.libfunc->name);
			continue_after_breakpoint(event->thr,
					address2bpstruct(event->proc, event->e_un.brk_addr));
			return;
		}
	}

	tmp = event->proc->list_of_symbols;
	while(tmp) {
		if (event->e_un.brk_addr == tmp->enter_addr) {
			event->thr->stack_pointer = get_stack_pointer(event->thr->tid);
			event->thr->return_addr = get_return_addr(event->thr->tid, event->thr->stack_pointer);
			output_left(LT_TOF_FUNCTION, event->thr, tmp->name);
			callstack_push_symfunc(event->thr, tmp);
			continue_after_breakpoint(event->thr, address2bpstruct(event->proc, tmp->enter_addr));
			return;
		}
		tmp = tmp->next;
	}
	output_line(event->thr, "breakpointed at %p (?)",
		(void *)event->e_un.brk_addr);
	continue_process(event->thr->tid);
}

void
callstack_push_syscall(struct thread * thr, int sysnum) {
	struct callstack_element * elem;

	/* FIXME: not good -- should use dynamic allocation. 19990703 mortene. */
	if (thr->callstack_depth == MAX_CALLDEPTH-1) {
		fprintf(stderr, "Error: call nesting too deep!\n");
		return;
	}

	elem = & thr->callstack[thr->callstack_depth];
	elem->is_syscall = 1;
	elem->c_un.syscall = sysnum;
	elem->return_addr = NULL;

	thr->callstack_depth++;
	if (opt_T || opt_c) {
		struct timezone tz;
		gettimeofday(&elem->time_spent, &tz);
	}
}

static void
callstack_push_symfunc(struct thread * thr, struct library_symbol * sym) {
	struct callstack_element * elem;

	struct process* proc = get_proc(thr);

	/* FIXME: not good -- should use dynamic allocation. 19990703 mortene. */
	if (thr->callstack_depth == MAX_CALLDEPTH-1) {
		fprintf(stderr, "Error: call nesting too deep!\n");
		return;
	}

	elem = & thr->callstack[thr->callstack_depth];
	elem->is_syscall = 0;
	elem->c_un.libfunc = sym;

	elem->return_addr = thr->return_addr;
	debug(3, "Simon: [callstack_push_symfunc]: return_addr=%x\n", elem->return_addr);
	insert_breakpoint(proc, elem->return_addr);

	thr->callstack_depth++;
	if (opt_T || opt_c) {
		struct timezone tz;
		gettimeofday(&elem->time_spent, &tz);
	}
}

static void
callstack_pop(struct thread * thr) {
	struct callstack_element * elem;
	assert(thr->callstack_depth > 0);

	struct process* proc = get_proc(thr);
	if (proc == NULL)
	{
		debug(1, "!!!![callstack_pop]: proc == null, thr:%x, thr_idx:%d\n", thr, thr->child_idx);
		return;
	}
		
	elem = & thr->callstack[thr->callstack_depth-1];
	if (!elem->is_syscall) {
		delete_breakpoint(proc, elem->return_addr);
	}
	thr->callstack_depth--;
}

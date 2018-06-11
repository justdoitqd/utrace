//Simon scanned
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "ltrace.h"
#include "options.h"
#include "elf.h"
#include "debug.h"

struct process *
open_program(char * filename) {
	struct process * proc;
	proc = malloc(sizeof(struct process));
	if (!proc) {
		perror("malloc");
		exit(1);
	}
	memset(proc, 0, sizeof(struct process));
	proc->filename = filename;
	proc->pid = 0;
	proc->breakpoints = NULL;
	proc->breakpoints_enabled = -1;

	breakpoints_init(proc);
	proc->next = list_of_processes;
	list_of_processes = proc;
	return proc;
}

void
open_pid(pid_t pid, int verbose) {
	struct process * proc;
	char * filename;

	if (trace_pid(pid)<0) {
		fprintf(stderr, "Cannot attach to pid %u: %s\n", pid, strerror(errno));
		return;
	}

	filename = pid2name(pid);

	if (!filename) {
		if (verbose) {
			fprintf(stderr, "Cannot trace pid %u: %s\n", pid, strerror(errno));
		}
		return;
	}

	proc = open_program(filename);
	proc->pid = pid;

#if 0
	//Simon: set it as breakenabled
	if (brk_enabled)
	{
		proc->breakpoints_enabled = 1;
	}
#endif

	reg_tid(proc, pid);
	return;
}

struct thread*
reg_tid(struct process * proc, pid_t tid)
{
	int i = 0;
	int idx_ava = -1;
	debug(3, ">>reg_tid, tid=%u, proc->pid=%u\n", tid, proc->pid);
	for (; i < MAX_THREADS_TRACED; i++)
	{
		if ((proc->threads[i].tid == 0) && (idx_ava == -1))
		{
			idx_ava = i;
			continue;
		}

		if (proc->threads[i].tid == tid)
		{
			debug(3, "[reg_tid]: the tid %u has been at %d index of threads[]\n", tid, i);
			return &proc->threads[i];
		}
	}

	if (idx_ava == -1)
	{
		debug(1, "No space to insert tid %u, process will exit...\n", tid);
		exit(1);
	}

        proc->threads[idx_ava].tid = tid;
        proc->threads[idx_ava].child_idx = i;

	//the thread id will be prone to be used recently, update cache
	proc->hot_tid_idx = i;

	return &proc->threads[i];
}

void
dereg_tid(struct process * proc, pid_t tid)
{
	int i = 0;
	debug(3, ">>dereg_tid, tid=%u, proc->pid=%u\n", tid, proc->pid);

	if (tid == 0)
		return;

	for (; i < MAX_THREADS_TRACED; i++)
	{
		if (proc->threads[i].tid == tid)
		{
			break;
		}
	}

	if (i == MAX_THREADS_TRACED)
	{
	        debug(1, "[dereg_tid]: cannot find an existing tid %u to be deregistered!!!!\n", tid);
		return;
	}

	proc->threads[i].tid = 0;

	if (proc->hot_tid_idx == i)
	{
		proc->hot_tid_idx = -1;
	}

	return;
}

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})


struct process* get_proc(struct thread* thr)
{
	struct thread* thr_arry = thr - thr->child_idx;
	struct process* proc = container_of(thr_arry, struct process, threads);
	return proc;
}

#ifndef _HCK_LTRACE_H
#define _HCK_LTRACE_H

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>

#include "defs.h"
#include "dict.h"

/* BREAKPOINT_LENGTH is defined in "sysdep.h" */
#include "sysdep.h"

#define MAX_LIBRARY	30

extern char * command;

extern int exiting;		/* =1 if we have to exit ASAP */

struct breakpoint {
	void * addr;
	unsigned char orig_value[BREAKPOINT_LENGTH];
	int enabled;
};

enum arg_type {
	ARGTYPE_UNKNOWN=-1,
	ARGTYPE_VOID,
	ARGTYPE_INT,
	ARGTYPE_UINT,
	ARGTYPE_OCTAL,
	ARGTYPE_CHAR,
	ARGTYPE_ADDR,
	ARGTYPE_FILE,
	ARGTYPE_FORMAT,     /* printf-like format */
	ARGTYPE_STRING,
	ARGTYPE_STRING0,    /* stringN: string up to (arg N) bytes */
	ARGTYPE_STRING1,
	ARGTYPE_STRING2,
	ARGTYPE_STRING3
};

enum tof {
	LT_TOF_NONE=0,
	LT_TOF_FUNCTION,	/* A real library function */
	LT_TOF_FUNCTIONR,	/* Return from a real library function */
	LT_TOF_SYSCALL,		/* A syscall */
	LT_TOF_SYSCALLR		/* Return from a syscall */
};

struct function {
	const char * name;
	enum arg_type return_type;
	int num_params;
	enum arg_type arg_types[MAX_ARGS];
	int params_right;
	struct function * next;
};

extern struct function * list_of_functions;

struct library_symbol {
	char * name;
	void * enter_addr;

	struct library_symbol * next;
};

struct callstack_element {
	union {
		int syscall;
		struct library_symbol * libfunc;
	} c_un;
	int is_syscall;
	void * return_addr;
	struct timeval time_spent;
};

#define MAX_CALLDEPTH 100

#define MAX_THREADS_TRACED 256
/*Simon TODO:
 * need add thread[]  data structure 
 * since instruction_pointer, stack_pointer, etc are thread specific
 */
struct thread {
	pid_t tid;                       //it is actually tid or LWP id or pid of that thread
	int callstack_depth;
	struct callstack_element callstack[MAX_CALLDEPTH];

	/* Arch-dependent: */
	void * instruction_pointer;
	void * stack_pointer;		/* To get return addr, args... */
	void * return_addr;
	struct breakpoint * breakpoint_being_enabled;
	void * arch_ptr;

	/* output: */
	enum tof type_being_displayed;

	unsigned char child_idx; // the index this thread locates at process.threads[]
};

struct process {
	char * filename;
	pid_t pid;                //it is thread group id or pid of main thread in that process
	int hot_tid_idx;          // a cache for tids[], which quickly indicates hot_tid belongs to this process; locality
	struct dict * breakpoints;
	int breakpoints_enabled;	/* -1:not enabled yet, 0:disabled, 1:enabled */

	struct library_symbol * list_of_symbols;

	struct process *next;

	struct thread threads[MAX_THREADS_TRACED];
};


struct event {
	struct process *proc;
	struct thread *thr;
	enum {
		LT_EV_UNKNOWN,
		LT_EV_NONE,
		LT_EV_SIGNAL,
		LT_EV_EXIT,
		LT_EV_EXIT_SIGNAL,
		LT_EV_SYSCALL,
		LT_EV_SYSRET,
		LT_EV_BREAKPOINT
	} thing;
	union {
		int ret_val;		/* _EV_EXIT */
		int signum;		/* _EV_SIGNAL, _EV_EXIT_SIGNAL */
		int sysnum;		/* _EV_SYSCALL, _EV_SYSRET */
		void * brk_addr;	/* _EV_BREAKPOINT */
	} e_un;
};

struct opt_c_struct {
	int count;
	struct timeval tv;
};
extern struct dict * dict_opt_c;

extern struct process * list_of_processes;

extern void * instruction_pointer;

extern struct event * wait_for_something(void);
extern void process_event(struct event * event);
extern void execute_program(struct process *, char **);
extern int display_arg(enum tof type, struct thread * proc, int arg_num, enum arg_type at);
extern struct breakpoint * address2bpstruct(struct process * proc, void * addr);
extern void breakpoints_init(struct process * proc);
extern void insert_breakpoint(struct process * proc, void * addr);
extern void delete_breakpoint(struct process * proc, void * addr);
extern void enable_all_breakpoints(struct process * proc);
extern void disable_all_breakpoints(struct process * proc);
extern struct process * open_program(char * filename);
extern void open_pid(pid_t pid, int verbose);
extern struct thread* reg_tid(struct process * proc, pid_t tid);
extern void dereg_tid(struct process * proc, pid_t tid);
extern void show_summary(void);


/* Arch-dependent stuff: */
extern char * pid2name(pid_t pid);
extern void trace_me(void);
extern int trace_pid(pid_t pid);
extern void untrace_pid(pid_t pid);
extern void get_arch_dep(struct thread * proc);
extern void * get_instruction_pointer(pid_t pid);
extern void set_instruction_pointer(pid_t pid, void * addr);
extern void * get_stack_pointer(pid_t pid);
extern void * get_return_addr(pid_t pid, void * stack_pointer);
extern void enable_breakpoint(pid_t pid, struct breakpoint * sbp);
extern void disable_breakpoint(pid_t pid, const struct breakpoint * sbp);
extern int fork_p(int sysnum);
extern int clone_p(int sysnum);
extern int exec_p(int sysnum);
extern int syscall_p(struct thread * thr, int status, int * sysnum);
extern void continue_process(pid_t pid);
extern void continue_after_signal(pid_t pid, int signum);
extern void continue_after_breakpoint(struct thread * proc, struct breakpoint * sbp);
extern void continue_enabling_breakpoint(pid_t pid, struct breakpoint * sbp);
extern long gimme_arg(enum tof type, struct thread * proc, int arg_num);
extern void save_register_args(enum tof type, struct thread * proc);
extern int umovestr(struct thread * proc, void * addr, int len, void * laddr);
extern int ffcheck(void *maddr);
#if 0	/* not yet */
extern int umoven(struct process * proc, void * addr, int len, void * laddr);
#endif

extern int trace_option(pid_t pid);
unsigned long ptrace_gettid(pid_t pid);
extern void pid2tgid_cache_flush();
extern pid_t pid2tgid(pid_t pid);

extern struct process* get_proc(struct thread* thr);

#endif

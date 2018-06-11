#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include "ptrace.h"
#include "ltrace.h"

void *
get_instruction_pointer(struct process * proc) {
	proc_archdep *a = (proc_archdep *)(proc->arch_ptr);
	if (a->valid)
		return (void *)a->regs.r_pc;
	return (void *)-1;
}

void
set_instruction_pointer(struct process * proc, void * addr) {
	proc_archdep *a = (proc_archdep *)(proc->arch_ptr);
	if (a->valid)
		a->regs.r_pc = (long)addr;
}

void *
get_stack_pointer(struct process * proc) {
	proc_archdep *a = (proc_archdep *)(proc->arch_ptr);
	if (a->valid)
		return (void *)a->regs.r_o6;
	return (void *)-1;
}

void *
get_return_addr(struct process * proc, void * stack_pointer) {
	proc_archdep *a = (proc_archdep *)(proc->arch_ptr);
	unsigned int t;
	if (!a->valid)
		return (void *)-1;
	/* Work around structure returns */
	t = ptrace(PTRACE_PEEKTEXT, proc->pid, a->regs.r_o7 + 8, 0);
	if (t < 0x400000)
		return (void *)a->regs.r_o7 + 12;
	return (void *)a->regs.r_o7 + 8;
}

#include <sys/types.h>

#include "ltrace.h"

void output_line(struct thread * proc, char *fmt, ...);

void output_left(enum tof type, struct thread * proc, char * function_name);
void output_right(enum tof type, struct thread * proc, char * function_name);

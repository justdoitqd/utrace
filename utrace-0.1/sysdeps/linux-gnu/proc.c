#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

/* /proc/pid doesn't exist just after the fork, and sometimes `ltrace'
 * couldn't open it to find the executable.  So it may be necessary to
 * have a bit delay
 */

#define	MAX_DELAY	100000		/* 100000 microseconds = 0.1 seconds */

typedef struct _PID2TGID_ENTRY {
	pid_t pid;
	pid_t tgid;
	unsigned long ref_count;
} PID2TGID_ENTRY;

#define PID2TGID_CACHE_SIZE  10
PID2TGID_ENTRY g_pid2tgid_cache[10];

/*
 * Returns a file name corresponding to a running pid
 */
char *
pid2name(pid_t pid) {
	char proc_exe[1024];

	if (!kill(pid, 0)) {
		int delay=0;

		sprintf(proc_exe, "/proc/%d/exe", pid);

		while(delay<MAX_DELAY) {
			if (!access(proc_exe, F_OK)) {
				return strdup(proc_exe);
			}
			delay += 1000;	/* 1 milisecond */
		}
	}
	return NULL;
}

static pid_t 
pid2tgid_cache_get(pid_t pid)
{
	int i;

	if (pid == 0)
		return 0;

	for (i = 0; i < PID2TGID_CACHE_SIZE; i++)
	{
		if (g_pid2tgid_cache[i].pid == pid)
		{
			g_pid2tgid_cache[i].ref_count++;
			return g_pid2tgid_cache[i].tgid;
		}
	}
	return 0;
}

static void
pid2tgid_cache_put(pid_t pid, pid_t tgid)
{
	int i;
	unsigned long ref_count = 0xFFFFFFFF;
	int idx = 0;

	if (pid == 0)
		return;

	for (i = 0; i < PID2TGID_CACHE_SIZE; i++)
	{
		if (g_pid2tgid_cache[i].pid == pid)
			return; 

		if (g_pid2tgid_cache[i].pid == 0)
		{
			g_pid2tgid_cache[i].pid = pid;
			g_pid2tgid_cache[i].tgid = tgid;
			return;
		}

		if (ref_count > g_pid2tgid_cache[i].ref_count)
			idx = i;
	}
        	
	/* replace the LFU cache */
	g_pid2tgid_cache[idx].pid = pid;
	g_pid2tgid_cache[idx].tgid = tgid;
	return;
}

void
pid2tgid_cache_flush()
{
	memset(g_pid2tgid_cache, 0, sizeof(g_pid2tgid_cache));
	return;
}

/* An example of proc file output:
 * simon@lionteeth:~$ cat /proc/5675/status
 * Name:   test
 * State:  T (tracing stop)
 * Tgid:   5674
 * Pid:    5675
 * PPid:   5673
 * TracerPid:      5673
 * Uid:    1000    1000    1000    1000
 * Gid:    1000    1000    1000    1000
 * FDSize: 32
 * Groups: 4 20 24 25 29 30 44 46 107 108 109 1000 
 * VmPeak:    10852 kB
 * VmSize:    10852 kB
 * VmLck:         0 kB
 * VmHWM:       896 kB
 * VmRSS:       896 kB
 * VmData:     8392 kB
 * VmStk:        84 kB
 * VmExe:         4 kB
 * VmLib:      2300 kB
 * VmPTE:        12 kB
 * Threads:        2
 * SigQ:   0/7168
 * SigPnd: 0000000000000000
 * ShdPnd: 0000000000000000
 * SigBlk: 0000000000000000
 * SigIgn: 0000000000000000
 * SigCgt: 0000000180000000
 * CapInh: 0000000000000000
 * CapPrm: 0000000000000000
 * CapEff: 0000000000000000
 * voluntary_ctxt_switches:        1
 * nonvoluntary_ctxt_switches:     0
 */
static pid_t
parse_tgid_str(char* proc_str)
{
	char* p = strstr(proc_str, "Tgid:");
	char buf[10];
	char* lp = &buf[0];

	if (p == 0)
		return 0;

	p += strlen("Tgid:");

	// move to digits section
        while ((*p != 0) && (*p != '\n') && ((*p > '9') || (*p < '0'))) 
	{
		p++;
	}

	if ((*p > '9') && (*p < '0'))
		return 0;

        while ((*p <= '9') && (*p >= '0'))
	{
             *lp = *p;
	     lp++; p++;
	     if (lp - buf > sizeof(buf) - 1)
		     return 0; //cannot be so large
	}

	*lp = '\0';
	return (pid_t)atoi(&buf[0]);
}

/*
 * Returns a tgid corresponding to a running pid(LWP id)
 */
pid_t 
pid2tgid(pid_t pid) {
	pid_t tgid = 0;
	tgid = pid2tgid_cache_get(pid);

	if (tgid != 0)
		return tgid;

	char proc_status_path[30];
	char proc_status_output[1024];

	if (!kill(pid, 0)) {
		sprintf(proc_status_path, "/proc/%d/status", pid);
		int fd = open(proc_status_path, O_RDONLY);
		if (fd == -1)
			return 0;

		int byte_count = read(fd, proc_status_output, 1024);
		if (byte_count == -1)
			return 0;
	}

	tgid = parse_tgid_str(proc_status_output);
	if (tgid != 0)
	{
		pid2tgid_cache_put(pid, tgid);
	}
	return tgid;
}


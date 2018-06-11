# Generated automatically from Makefile.in by configure.
#
# ltrace's Makefile.in
#

#OS		:= $(shell uname -s)
OS		:= linux-gnu

TOPDIR		= $(shell pwd)

prefix		= /usr/local
sysconfdir	= ${prefix}/etc
bindir		= $(prefix)/bin
mandir		= ${prefix}/man
docdir		= $(prefix)/doc/ltrace

CC	=	gcc
CPPFLAGS =	-I$(TOPDIR) -I$(TOPDIR)/sysdeps/$(OS) -I- -DHAVE_CONFIG_H -DHAVE_LIBIBERTY
# CPPFLAGS =	-I$(TOPDIR) -I$(TOPDIR)/sysdeps/$(OS) -I- -DHAVE_CONFIG_H 
# CFLAGS	=	-Wall -g -O2 -DSYSCONFDIR=\"$(sysconfdir)\"
CFLAGS	=	-Wall -g -O2 -DSYSCONFDIR=\"$(sysconfdir)\" -DHAVE_LIBIBERTY -D__NR_fork -D__NR_clone -D__NR_vfork
LDFLAGS	=
LIBS	=	-liberty 

OBJ	=	ltrace.o options.o elf.o output.o read_config_file.o	\
		execute_program.o wait_for_something.o process_event.o	\
		display_args.o breakpoints.o proc.o demangle.o dict.o \
		debug.o summary.o

all:		utrace

utrace:		sysdeps/sysdep.o $(OBJ)
		$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

sysdeps/sysdep.o: dummy
		$(MAKE) -C sysdeps/$(OS)

clean:
		$(MAKE) -C sysdeps/$(OS) clean
		$(RM) ltrace $(OBJ)
		$(RM) *~ *.bak a.out core KK

distclean:	clean
		$(RM) config.cache config.status config.log config.h Makefile ltrace.spec configure.scan

realclean:	distclean
		$(RM) configure config.h.in

dist:		clean
		( cd .. ; tar zcvf ltrace-`date +%y%m%d`.tgz ltrace )

install:	ltrace
		/usr/bin/install -c -d $(DESTDIR)$(bindir) $(DESTDIR)$(docdir) $(DESTDIR)$(mandir)/man1
		/usr/bin/install -c -d $(DESTDIR)$(sysconfdir)
		/usr/bin/install -c -s ltrace $(DESTDIR)$(bindir)
		/usr/bin/install -c -m 644 etc/ltrace.conf $(DESTDIR)$(sysconfdir)
		/usr/bin/install -c -m 644 COPYING README TODO BUGS ChangeLog $(DESTDIR)$(docdir)
		/usr/bin/install -c -m 644 ltrace.1 $(DESTDIR)$(mandir)/man1

dummy:

.PHONY:		all clean distclean dist install dummy

.EXPORT_ALL_VARIABLES:

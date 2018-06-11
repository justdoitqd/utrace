#if HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * This file contains functions specific to ELF binaries
 *
 * Silvio Cesare <silvio@big.net.au>
 *
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

#include "ltrace.h"
#include "elf.h"
#include "debug.h"

static void do_init_elf(struct ltelf *lte, const char *filename);
static void do_close_elf(struct ltelf *lte);
static void do_load_elf_symtab(struct ltelf *lte, int load_local);
static void do_init_load_libraries(void);
static void do_close_load_libraries(void);
static int in_load_libraries(const char *func);
static void add_library_symbol(
	struct ltelf *lte,
	int i,
	struct library_symbol **library_symbolspp
);
static void add_local_symbol(
	struct ltelf *lte,
	int i,
	struct library_symbol **library_symbolspp
);

struct ltelf library_lte[MAX_LIBRARY];

static void
do_init_elf(struct ltelf *lte, const char *filename) {
	struct stat sbuf;

	debug(1, "Reading ELF from %s...", filename);

	lte->fd = open(filename, O_RDONLY);
	if (lte->fd == -1) {
		fprintf(
			stderr,
			"Can't open \"%s\": %s\n",
			filename,
			strerror(errno)
		);
		exit(1);
	}
	if (fstat(lte->fd, &sbuf) == -1) {
		fprintf(
			stderr,
			"Can't stat \"%s\": %s\n",
			filename,
			strerror(errno)
		);
		exit(1);
	}
	if (sbuf.st_size < sizeof(Elf_Ehdr)) {
		fprintf(
			stderr,
			"\"%s\" is not an ELF binary object\n",
			filename
		);
		exit(1);
	}
	lte->maddr = mmap(
		NULL, sbuf.st_size, PROT_READ, MAP_SHARED, lte->fd, 0
	);
	if (lte->maddr == (void*)-1) {
		fprintf(
			stderr,
			"Can't mmap \"%s\": %s\n",
			filename,
			strerror(errno)
		);
		exit(1);
	}

#if defined(FILEFORMAT_CHECK)
	if (! ffcheck(lte->maddr)) {
		fprintf(
				stderr,
				"%s: wrong architecture or ELF format\n",
				filename
			   );
		exit(1);
	}
#endif

	lte->ehdr = lte->maddr;

	if (strncmp(lte->ehdr->e_ident, ELFMAG, SELFMAG)) {
		fprintf(
			stderr,
			"\"%s\" is not an ELF binary object\n",
			filename
		);
		exit(1);
	}

/*
	more ELF checks should go here - the e_arch/e_machine fields in the
	ELF header are specific to each architecture. perhaps move some code
	into sysdeps (have check_ehdr_arch) - silvio
*/

	lte->strtab = NULL;
	lte->local_strtab = NULL;

	lte->symtab = NULL;
	lte->local_symtab = NULL;
	lte->symtab_len = 0;
	lte->local_symtab_len = 0;

	lte->local_shstrtab = NULL;
}

static void
do_close_elf(struct ltelf *lte) {
	close(lte->fd);
}

static void
do_load_elf_symtab(struct ltelf *lte, int load_local) {
	void *maddr = lte->maddr;
	Elf_Ehdr *ehdr = lte->ehdr;
	Elf_Shdr *shdr = (Elf_Shdr *)(maddr + ehdr->e_shoff);
	int i;

/*
	an ELF object should only ever one dynamic symbol section (DYNSYM), but
	can have multiple string tables.  the sh_link entry from DYNSYM points
	to the correct STRTAB section - silvio
*/

	for(i = 0; i < ehdr->e_shnum; i++) {
		if (shdr[i].sh_type == SHT_DYNSYM) {
			lte->symtab = (Elf_Sym *)(maddr + shdr[i].sh_offset);
			lte->symtab_len = shdr[i].sh_size;
			lte->strtab = (char *)(
				maddr + shdr[shdr[i].sh_link].sh_offset
			);
		}
		if (load_local && 
		    (shdr[i].sh_type == SHT_SYMTAB)) 
		{
			lte->local_symtab = (Elf_Sym *)(maddr + shdr[i].sh_offset);
			lte->local_symtab_len = shdr[i].sh_size;
			lte->local_strtab = (char *)(
				maddr + shdr[shdr[i].sh_link].sh_offset
			);
		}
	}

	lte->local_shstrtab = (char *) maddr + shdr[lte->ehdr->e_shstrndx].sh_offset;

	debug(2, "symtab: %p", lte->symtab);
	debug(2, "symtab_len: %lu", lte->symtab_len);
	debug(2, "strtab: %p", lte->strtab);
	debug(2, "local_symtab: %p", lte->local_symtab);
	debug(2, "local_symtab_len: %lu", lte->local_symtab_len);
	debug(2, "local_strtab: %p", lte->local_strtab);
}

static void
add_local_symbol(
		struct ltelf *lte,
		int i,
		struct library_symbol **library_symbolspp) {
	struct library_symbol *tmp = *library_symbolspp;
	struct library_symbol *library_symbols;

	*library_symbolspp = (struct library_symbol *)malloc(
		sizeof(struct library_symbol)
	);
	library_symbols = *library_symbolspp;
	if (library_symbols == NULL) {
		perror("ltrace: malloc");
		exit(1);
	}

#ifdef __sparc__
	library_symbols->enter_addr = (void *)(lte->local_symtab[i].st_value + 4 /* plt(?) */);
#else
	library_symbols->enter_addr = (void *)lte->local_symtab[i].st_value;
#endif
	library_symbols->name = &lte->local_strtab[lte->local_symtab[i].st_name];
	library_symbols->next = tmp;

	debug(2, "addr: %p, symbol: \"%s\"",
			lte->local_symtab[i].st_value,
			&lte->local_strtab[lte->local_symtab[i].st_name]);
}
static void
add_library_symbol(
		struct ltelf *lte,
		int i,
		struct library_symbol **library_symbolspp) {
	struct library_symbol *tmp = *library_symbolspp;
	struct library_symbol *library_symbols;

	*library_symbolspp = (struct library_symbol *)malloc(
		sizeof(struct library_symbol)
	);
	library_symbols = *library_symbolspp;
	if (library_symbols == NULL) {
		perror("ltrace: malloc");
		exit(1);
	}

#ifdef __sparc__
	library_symbols->enter_addr = (void *)(lte->symtab[i].st_value + 4 /* plt(?) */);
#else
	library_symbols->enter_addr = (void *)lte->symtab[i].st_value;
#endif
	library_symbols->name = &lte->strtab[lte->symtab[i].st_name];
	library_symbols->next = tmp;

	debug(2, "addr: %p, symbol: \"%s\"",
			lte->symtab[i].st_value,
			&lte->strtab[lte->symtab[i].st_name]);
}

/*
	this is all pretty slow. perhaps using .hash would be faster, or
	even just a custum built hash table. its all initialization though,
	so its not that bad - silvio
*/

static void
do_init_load_libraries(void) {
	int i;

	for (i = 0; i < library_num; i++) {
		do_init_elf(&library_lte[i], library[i]);
		do_load_elf_symtab(&library_lte[i], 0);
	}
}

static void
do_close_load_libraries(void) {
	int i;

	for (i = 0; i < library_num; i++) {
		do_close_elf(&library_lte[i]);
	}
}

static int
in_load_libraries(const char *func) {
	int i, j;
/*
	if no libraries are specified, assume we want all
*/
	if (library_num == 0) return 1;

	for (i = 0; i < library_num; i++) {
		Elf_Sym *symtab = library_lte[i].symtab;
		char *strtab = library_lte[i].strtab;

		for(
			j = 0;
			j < library_lte[i].symtab_len / sizeof(Elf_Sym);
			j++
		) {
			if (
				symtab[j].st_value &&
				!strcmp(func, &strtab[symtab[j].st_name])
			) return 1;
		}
	}
	return 0;
}

/*
	this is the main function
*/

struct library_symbol *
read_elf(const char *filename) {
	struct library_symbol *library_symbols = NULL;
	struct ltelf lte;
	int i;

	do_init_elf(&lte, filename);
	do_load_elf_symtab(&lte, 1);
	do_init_load_libraries();

	for(i = 0; i < lte.symtab_len / sizeof(Elf_Sym); i++) {
		Elf_Sym *symtab = lte.symtab;
		char *strtab = lte.strtab;
#if 0
	        printf("Simon dynsym: [%d] %28s st_value:%8x st_shndx:%4x st_size:%4x st_info:%x st_other:%4x, inLibrary=%d\n",
				        i,
					&strtab[symtab[i].st_name],
					symtab[i].st_value,
					symtab[i].st_shndx,
					symtab[i].st_size,
					symtab[i].st_info,
					symtab[i].st_other,
					in_load_libraries(&strtab[symtab[i].st_name]));
#endif

		if (!symtab[i].st_shndx && symtab[i].st_value) {
			if (in_load_libraries(&strtab[symtab[i].st_name])) {
				//Simon: Don't add other symbols
				//add_library_symbol(&lte, i, &library_symbols);
			}
		}
	}

	//Simon: now also load local Symbols
	for(i = 0; i < lte.local_symtab_len / sizeof(Elf_Sym); i++) {
		Elf_Sym *symtab = lte.local_symtab;
		char *strtab = lte.local_strtab;
		char st_info = symtab[i].st_info;
		int shndx = symtab[i].st_shndx;
		Elf_Shdr *shdr = (Elf_Shdr *)((char*)(lte.maddr)+lte.ehdr->e_shoff);

		if ((symtab[i].st_value) &&
                    ((symtab[i].st_size) > 0) && 
                    ((symtab[i].st_other) == 0) && 
                    //("&strtab[shdr[shndx].sh_name] == ) &&      //The symbol locates at .text
		    (strcmp(&lte.local_shstrtab[shdr[shndx].sh_name], ".text") == 0 ) &&
                    ((st_info & 0xf) == STT_FUNC) && (st_info >> 4)==STB_GLOBAL)
		{
#if 0
                     //if (i > lte.local_symtab_len / sizeof(Elf_Sym) / 2)
                     //if (i == 928)
		     {
			printf("Simon: %25s section:%10s st_value:%4x st_shndx:%4x st_size:%4x st_info:%x st_other:%4x\n",
					&strtab[symtab[i].st_name],
					&strtab[shdr[shndx].sh_name],
					symtab[i].st_value,
					symtab[i].st_shndx,
					symtab[i].st_size,
					symtab[i].st_info,
					symtab[i].st_other);
#endif
			//if (strstr(&strtab[symtab[i].st_name], "threadFunc1")!=0)
		        add_local_symbol(&lte, i, &library_symbols);
		     //}
		}
	}

	do_close_load_libraries();
	do_close_elf(&lte);

	return library_symbols;
}

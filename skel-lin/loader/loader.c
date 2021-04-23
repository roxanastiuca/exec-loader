/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "exec_parser.h"

static so_exec_t *exec; /* executable file information */

static int fd; /* file descriptor for executable file (read-only) */
static int page_size; /* size of one page - 4096 */
static struct sigaction old_action; /* old handler for signal SIGSEGV */

/*
 * Description: finds the segment from the executable file where
 address addr is.
 * Return: pointer to that segment.
 */
static struct so_seg *find_segm(uintptr_t addr) {
	struct so_seg *segm = NULL;

	for (int i = 0; i < exec->segments_no && !segm; i++) {
		struct so_seg *curr_segm = &exec->segments[i];

		if (addr >= curr_segm->vaddr &&
			addr < curr_segm->vaddr + curr_segm->mem_size) {
			/* Found. Address is between segment's boundaries. */
			segm = curr_segm;
		}
	}

	return segm;
}

/*
 * Description: handler for page faults (signal SIGSEGV). If page acessed is
 already mapped or if it's not in any segment, call old handler. Else, map
 that page.
 */
static void pagefault_handler(int signum, siginfo_t *info, void *context) {
	uintptr_t addr; /* address accessed */
	int pagenum; /* page in segment where addr is */
	int mapflags = 0; /* flags used for mapping page */
	int mapdif = 0; /* bytes mapped, that are outside of file limits */
	char *p; /* aux pointer, used to check return of mmap */

	/* Check signal is SIGSEGV */
	if (signum != SIGSEGV)
		return;

	addr = (uintptr_t) info->si_addr;

	/* Find segment */
	struct so_seg *segm = find_segm(addr);

	if (!segm) {
		/* Address accessed is not in any of the segments. */
		old_action.sa_sigaction(signum, info, context);
		return;
	}

	char *is_mapped = (char *) segm->data;
	pagenum = (addr - segm->vaddr) / page_size;
	if (is_mapped[pagenum] == 1) {
		/* Page is already mapped. Access not allowed. */
		old_action.sa_sigaction(signum, info, context);
		return;
	}

	mapflags = MAP_PRIVATE | MAP_FIXED;

	/* Resolve corner cases: */
	if (segm->file_size < segm->mem_size) {
		if (segm->file_size < pagenum * page_size) {
			/* Undefined behaviour */
			mapflags |= MAP_ANONYMOUS;
		} else if ((pagenum + 1) * page_size > segm->file_size) {
			/* It exceeds file boundaries by mapdif bytes */
			mapdif = (pagenum + 1) * page_size - segm->file_size;
		}
	}

	/* Map page: */
	p = mmap((void *) segm->vaddr + pagenum * page_size,
		page_size, segm->perm, mapflags,
		fd, segm->offset + pagenum * page_size);
	if (p == MAP_FAILED)
		exit(-ENOMEM);

	if (mapdif != 0) {
		/* We mapped outside of file boundaries. Zero those bytes */
		uintptr_t aux = segm->vaddr + pagenum * page_size +
			(page_size - mapdif);
		memset((char *) aux, 0, mapdif);
	}

	/* Mark page as mapped: */
	is_mapped[pagenum] = 1;
}

/*
 * Description: after a successful execution, free memory used
 by each segment in data field.
 */
static void so_end_exec() {
	int rc;

	/* Go through every segment: */
	for (int i = 0; i < exec->segments_no; i++) {
		struct so_seg *segm = &exec->segments[i];
		char *is_mapped = (char *) segm->data;

		int page_count = segm->mem_size / page_size;
		if (segm->mem_size % page_size)
			page_count++;

		/* Go through every page and if it's mapped, unmap it. */
		for (int j = 0; j < page_count; j++) {
			if (is_mapped[j] == 1) {
				uintptr_t addr = segm->vaddr + j * page_size;
				rc = munmap((char *) addr, page_size);
				if (rc == -1)
					exit(-ENOMEM);
			}
		}

		free(segm->data);
	}
}

/*
 * Description: initialize loader. Set new handle for signal SIGSEGV.
 */
int so_init_loader(void)
{
	page_size = getpagesize();

	struct sigaction action;
	int rc;

	action.sa_sigaction = pagefault_handler;
	sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGSEGV);
	action.sa_flags = SA_SIGINFO;

	rc = sigaction(SIGSEGV, &action, &old_action);
	if (rc == -1)
		exit(-1);

	return -1;
}

/*
 * Description: parse binary file and run executable.
 */
int so_execute(char *path, char *argv[])
{
	/* Open file in read-only mode. */
	fd = open(path, O_RDONLY);
	if (fd == -1)
		exit(-ENOENT);

	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	/*
	 * For each segment, use data field in order to keep track
	 * of which pages have been mapped already. */
	for (int i = 0; i < exec->segments_no; i++) {
		int page_count = exec->segments[i].mem_size / page_size;
		if (exec->segments[i].mem_size % page_size)
			page_count++;
		char *is_mapped = calloc(page_count, sizeof(char));
		if (!is_mapped)	return -1;
		exec->segments[i].data = is_mapped;
	}

	/* Start execution: */
	so_start_exec(exec, argv);

	/* Free memory: */
	so_end_exec();

	return -1;
}

/*
 * peekfd.c - Intercept file descriptor read and writes
 *
 * Copyright (C) 2007 Trent Waddington <trent.waddington@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <asm/ptrace.h>
#include <byteswap.h>
#include <endian.h>
#include <sys/user.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "i18n.h"

#ifdef ARM64
#include <sys/uio.h>
#include <linux/elf.h>
#endif

#ifdef I386
	#define REG_ORIG_ACCUM orig_eax
	#define REG_ACCUM eax
	#define REG_PARAM1 ebx
	#define REG_PARAM2 ecx
	#define REG_PARAM3 edx
#elif X86_64
	#define REG_ORIG_ACCUM orig_rax
	#define REG_ACCUM rax
	#define REG_PARAM1 rdi
	#define REG_PARAM2 rsi
	#define REG_PARAM3 rdx
#elif PPC
	#if !defined(__WORDSIZE)
	#include <bits/reg.h>
	#endif

	#define REG_ORIG_ACCUM gpr[0]
	#define REG_ACCUM gpr[3]
	#define REG_PARAM1 orig_gpr3
	#define REG_PARAM2 gpr[4]
	#define REG_PARAM3 gpr[5]
#ifndef PT_ORIG_R3
	#define PT_ORIG_R3 34
#endif
#elif defined(ARM)
#ifndef __ARM_EABI__
#error arm oabi not supported
#endif
	#define REG_ORIG_ACCUM ARM_r7
	#define REG_ACCUM ARM_r0
	#define REG_PARAM1 ARM_ORIG_r0
	#define REG_PARAM2 ARM_r1
	#define REG_PARAM3 ARM_r2

#elif defined(ARM64)
        #define REG_ORIG_ACCUM  regs[8]
        #define REG_ACCUM       regs[0]
        #define REG_PARAM1      regs[0]
        #define REG_PARAM2      regs[1]
        #define REG_PARAM3      regs[2]



#elif defined(MIPS)
#ifndef MIPSEL
#error only little endian supported
#endif
	#define REG_ORIG_ACCUM regs[3]
	#define REG_ACCUM regs[2]
	#define REG_PARAM1 regs[4]
	#define REG_PARAM2 regs[5]
	#define REG_PARAM3 regs[6]
#elif defined(M68K)
	#define REG_ORIG_ACCUM orig_d0
	#define REG_ACCUM d0
	#define REG_PARAM1 d1
	#define REG_PARAM2 d2
	#define REG_PARAM3 d3
#endif

#define MAX_ATTACHED_PIDS 1024
int num_attached_pids = 0;
pid_t attached_pids[MAX_ATTACHED_PIDS];
int *fds = NULL;

#ifdef ARM64
struct user_pt_regs_node {
	struct user_pt_regs regs;
	struct user_pt_regs_node *user_pt_regs_next;
};

void user_pt_regs_insert(struct user_pt_regs_node** user_pt_regs_head, struct user_pt_regs *regs)
{
    struct user_pt_regs_node* new_node =
            (struct user_pt_regs_node*) malloc(sizeof(struct user_pt_regs_node));

	memcpy(&new_node->regs, regs, sizeof(struct user_pt_regs));
    new_node->user_pt_regs_next = (*user_pt_regs_head);
    (*user_pt_regs_head) = new_node;
}

struct user_pt_regs * user_pt_regs_search(struct user_pt_regs_node** user_pt_regs_head, struct user_pt_regs *regs)
{
	struct user_pt_regs_node* current = *user_pt_regs_head;
	while (current != NULL)
	{
		if ((current->regs.REG_ORIG_ACCUM == regs->REG_ORIG_ACCUM) && (current->regs.REG_PARAM2 == regs->REG_PARAM2))
			return &current->regs;
		current = current->user_pt_regs_next;
	}
	return NULL;
}


int user_pt_regs_delete(struct user_pt_regs_node** user_pt_regs_head, struct user_pt_regs *regs)
{
	struct user_pt_regs_node* temp = *user_pt_regs_head, *prev;

	 if (temp != NULL && (&temp->regs == regs))
	 {
		 *user_pt_regs_head = temp->user_pt_regs_next;
		 free(temp);
		 return 0;
	 }

	 while (temp != NULL && (&temp->regs != regs))
	 {
		 prev = temp;
		 temp = temp->user_pt_regs_next;
	 }

	 if (temp == NULL) return -1;
	 prev->user_pt_regs_next = temp->user_pt_regs_next;
	 free(temp);
     return 0;
}
#endif

void detach(int signum) {
	int i;
	for (i = 0; i < num_attached_pids; i++)	
		ptrace(PTRACE_DETACH, attached_pids[i], 0, 0);
	if (fds)
		free(fds);
	signal(SIGINT, SIG_DFL);
	raise(SIGINT);
}

void attach(pid_t pid) {
	if (num_attached_pids >= MAX_ATTACHED_PIDS)
		return;
	attached_pids[num_attached_pids] = pid;
	if (ptrace(PTRACE_ATTACH, pid, 0, 0) == -1) {
		fprintf(stderr, _("Error attaching to pid %i\n"), pid);
		return;
	}
	num_attached_pids++;
}

void print_version()
{
  fprintf(stderr, _("peekfd (PSmisc) %s\n"), VERSION);
  fprintf(stderr, _(
    "Copyright (C) 2007 Trent Waddington\n\n"));
  fprintf(stderr, _(
    "PSmisc comes with ABSOLUTELY NO WARRANTY.\n"
    "This is free software, and you are welcome to redistribute it under\n"
    "the terms of the GNU General Public License.\n"
    "For more information about these matters, see the files named COPYING.\n"));
}

void usage() {
	fprintf(stderr, _(
      "Usage: peekfd [-8] [-n] [-c] [-d] [-V] [-h] <pid> [<fd> ..]\n"
	  "    -8, --eight-bit-clean        output 8 bit clean streams.\n"
	  "    -n, --no-headers             don't display read/write from fd headers.\n"
	  "    -c, --follow                 peek at any new child processes too.\n"
	  "    -t, --tgid                   peek at all threads where tgid equals <pid>.\n"
	  "    -d, --duplicates-removed     remove duplicate read/writes from the output.\n"
	  "    -V, --version                prints version info.\n"
	  "    -h, --help                   prints this help.\n"
	  "\n"
	  "  Press CTRL-C to end output.\n"));
}

int bufdiff(pid_t pid, unsigned char *lastbuf, unsigned long addr, unsigned long len) {
	unsigned long i;
	for (i = 0; i < len; i++)
		if (lastbuf[i] != (ptrace(PTRACE_PEEKTEXT, pid, addr + i, 0) & 0xff))
			return 1;
	return 0;
}

int main(int argc, char **argv)
{
	int eight_bit_clean = 0;
	int no_headers = 0;
	int follow_forks = 0;
	int follow_clones = 0;
	int tgid = 0;
	int remove_duplicates = 0;
	int optc;
    int target_pid = 0;
    int numfds = 0;
    int i;
    unsigned long j;

    struct option options[] = {
      {"eight-bit-clean", 0, NULL, '8'},
      {"no-headers", 0, NULL, 'n'},
      {"follow", 0, NULL, 'c'},
      {"tgid", 0, NULL, 't'},
      {"duplicates-removed", 0, NULL, 'd'},
      {"help", 0, NULL, 'h'},
      {"version", 0, NULL, 'V'},
    };

  /* Setup the i18n */
#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

	if (argc < 2) {
		usage();
		return 1;
	}

	while ((optc = getopt_long(argc, argv, "8nctdhV", options, NULL)) != -1) {
		switch(optc) {
			case '8':
				eight_bit_clean = 1;
				break;
			case 'n':
				no_headers = 1;
				break;
			case 'c':
				follow_forks = 1;
				follow_clones = 1;
				break;
			case 't':
				tgid = 1;
				follow_clones = 1;
				break;
			case 'd':
				remove_duplicates = 1;
				break;
			case 'V':
				print_version();
				return 1;
			case 'h':
			case '?':
				usage();
				return 1;
		}
	}
    /* First arg off the options is the PID to see */
    if (optind >= argc) {
      usage();
      return -1;
    }
    target_pid = atoi(argv[optind++]);

    if (optind < argc) {
      numfds = argc - optind;
      fds = malloc(sizeof(int) * numfds);
	  for (i = 0; i < numfds; i++)
		fds[i] = atoi(argv[optind + i]);
    }

	attach(target_pid);

	if (tgid) {
		DIR *taskdir;
		struct dirent *dt;
		char taskpath[24];

		snprintf(taskpath, 24, "/proc/%d/task", target_pid);

		if ((taskdir = opendir(taskpath)) != 0) {
			while ((dt = readdir(taskdir)) != NULL) {
				int thread = atoi(dt->d_name);
				if ((thread != 0) && (thread != target_pid))
					attach(thread);
			}
			closedir(taskdir);
		}
	}

	if (num_attached_pids == 0)
		return 1;

	signal(SIGINT, detach);

	for (i = 0; i < num_attached_pids; i++)
		ptrace(PTRACE_SYSCALL, attached_pids[i], 0, 0);

	/*int count = 0;*/
	int lastfd = numfds > 0 ? fds[0] : 0;
	int lastdir = 3;
	unsigned char *lastbuf = NULL;
	unsigned long last_buf_size = -1;

#ifdef ARM64
	struct user_pt_regs_node* user_pt_regs_head = NULL;
#endif

	for(;;) {
		int status;
		pid_t pid = wait(&status);
		if (WIFSTOPPED(status)) {
#ifdef PPC
			struct pt_regs regs;
			regs.gpr[0] = ptrace(PTRACE_PEEKUSER, pid, __WORDSIZE/8 * PT_R0, 0);
			regs.gpr[3] = ptrace(PTRACE_PEEKUSER, pid, __WORDSIZE/8 * PT_R3, 0);
			regs.gpr[4] = ptrace(PTRACE_PEEKUSER, pid, __WORDSIZE/8 * PT_R4, 0);
			regs.gpr[5] = ptrace(PTRACE_PEEKUSER, pid, __WORDSIZE/8 * PT_R5, 0);
			regs.orig_gpr3 = ptrace(PTRACE_PEEKUSER, pid, __WORDSIZE/8 * PT_ORIG_R3, 0);
#elif defined(ARM)
			struct pt_regs regs;
			ptrace(PTRACE_GETREGS, pid, 0, &regs);

#elif defined(ARM64)
			struct user_pt_regs regs, *old_regs;
			struct iovec io;
			io.iov_base = &regs;
			io.iov_len = sizeof(regs);

			if (ptrace(PTRACE_GETREGSET, pid, (void*) NT_PRSTATUS, (void*) &io) == -1) {
				printf("ARM64: PTRACE_GETREGSET: %s\n", strerror(errno));
				return errno;
			}

#elif defined(MIPS)
			struct pt_regs regs;
			long pc = ptrace(PTRACE_PEEKUSER, pid, 64, 0);
			regs.regs[2] = ptrace(PTRACE_PEEKUSER,pid,2,0);
			regs.regs[3] = ptrace(PTRACE_PEEKTEXT, pid, pc - 8, 0) & 0xffff;
			regs.regs[4] = ptrace(PTRACE_PEEKUSER,pid,4,0);
			regs.regs[5] = ptrace(PTRACE_PEEKUSER,pid,5,0);
			regs.regs[6] = ptrace(PTRACE_PEEKUSER,pid,6,0);
#else
			struct user_regs_struct regs;
			ptrace(PTRACE_GETREGS, pid, 0, &regs);
#endif		
			/*unsigned int b = ptrace(PTRACE_PEEKTEXT, pid, regs.eip, 0);*/

#if defined(ARM64)
			if (follow_forks && regs.REG_ORIG_ACCUM == SYS_clone) {
#else
                        if ((follow_forks && regs.REG_ORIG_ACCUM == SYS_fork)
                         || (follow_clones && regs.REG_ORIG_ACCUM == SYS_clone)) {
#endif
				if (regs.REG_ACCUM > 0)
					attach(regs.REG_ACCUM);
			}
			if ((regs.REG_ORIG_ACCUM == SYS_read || regs.REG_ORIG_ACCUM == SYS_write) && (regs.REG_PARAM3 == regs.REG_ACCUM)) {
#ifdef ARM64
				/* ARM64 doesn't expose orig_x0 to user space,
				   so retrive orig_x0 from older user pt regs */
				old_regs = user_pt_regs_search(&user_pt_regs_head, &regs);
				if (old_regs != NULL) {
					regs.REG_PARAM1 = old_regs->REG_PARAM1;
					user_pt_regs_delete(&user_pt_regs_head, old_regs);
				}
#endif
				for (i = 0; i < numfds; i++)
					if (fds[i] == (int)regs.REG_PARAM1)
						break;
				if (i != numfds || numfds == 0) {
					if ((int)regs.REG_PARAM1 != lastfd || (int)regs.REG_ORIG_ACCUM != lastdir) {
						lastfd = regs.REG_PARAM1;
						lastdir = regs.REG_ORIG_ACCUM;
						if (!no_headers) {
							printf("\n%sing fd %i", regs.REG_ORIG_ACCUM == SYS_read ? "read" : "writ", lastfd);
							if (tgid)
								printf(" (thread %d)", pid);
							printf(":\n");
						}
					}
					if (!remove_duplicates || lastbuf == NULL
							||  last_buf_size != regs.REG_PARAM3 || 
							bufdiff(pid, lastbuf, regs.REG_PARAM2, regs.REG_PARAM3)) {

						if (remove_duplicates) {
							if (lastbuf)
								free(lastbuf);
							if ( NULL == (lastbuf = malloc(regs.REG_PARAM3))) {
							    perror("lastbuf malloc");
							    exit(1);
							}
							last_buf_size = regs.REG_PARAM3;
						}

						for (j = 0; j < regs.REG_PARAM3; j++) {
#if BYTE_ORDER == BIG_ENDIAN
#if __WORDSIZE == 64
							unsigned int a = bswap_64(ptrace(PTRACE_PEEKTEXT, pid, regs.REG_PARAM2 + j, 0));
#else
							unsigned int a = bswap_32(ptrace(PTRACE_PEEKTEXT, pid, regs.REG_PARAM2 + j, 0));
#endif
#else
							unsigned int a = ptrace(PTRACE_PEEKTEXT, pid, regs.REG_PARAM2 + j, 0);
#endif
							if (remove_duplicates)
								lastbuf[j] = a & 0xff;

							if (eight_bit_clean)
								putchar(a & 0xff);
							else {
								if (isprint(a & 0xff) || (a & 0xff) == '\n')
									printf("%c", a & 0xff);
								else if ((a & 0xff) == 0x0d)
									printf("\n");
								else if ((a & 0xff) == 0x7f)
									printf("\b");
								else if (a & 0xff)
									printf(" [%02x] ", a & 0xff);
							}
						}
					}
					fflush(stdout);
				}
			}
#ifdef ARM64
			else if (regs.REG_ORIG_ACCUM == SYS_read || regs.REG_ORIG_ACCUM == SYS_write)
			{
				user_pt_regs_insert(&user_pt_regs_head,&regs);
			}
#endif
			ptrace(PTRACE_SYSCALL, pid, 0, 0);
		}
	}

	return 0;
}

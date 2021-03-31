/*
 * pslog.c - print process log paths.
 *
 * Copyright (C) 2015-2017 Vito Mule'
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#include "i18n.h"


#ifndef PATH_MAX
#define PATH_MAX 4096
#endif /* PATH_MAX */

static int
usage ()
{
  fprintf(stderr,
    "Usage: pslog PID...\n"
    "       pslog -V, --version\n\n"

    "  -V,--version display version information\n\n");
  exit(255);
}

void
print_version()
{
  fprintf(stderr, "pslog (PSmisc) %s\n", VERSION);
  fprintf(stderr,
    "Copyright (C) 2015-2017 Vito Mule'.\n\n");
  fprintf(stderr,
    "PSmisc comes with ABSOLUTELY NO WARRANTY.\n"
    "This is free software, and you are welcome to redistribute it under\n"
    "the terms of the GNU General Public License.\n"
    "For more information about these matters, see the files named COPYING.\n");
}

int
main(int argc, char const *argv[])
{
  regex_t re_log;
  regex_t re_pid;
  char *fullpath = NULL;

  if (argc < 2) {
    usage();
  }

  /*
   * Allowed on the command line:
   * --version
   *  -V
   *  /proc/nnnn
   *  nnnn
   *  where nnnn is any number that doesn't begin with 0.
   *  If --version or -V are present, further arguments are ignored
   *  completely.
   */

  regcomp(&re_pid, "^((/proc/+)?[1-9][0-9]*|-V|--version)$",
          REG_EXTENDED|REG_NOSUB);

  if (regexec(&re_pid, argv[1], 0, NULL, 0) != 0) {
     fprintf(stderr, "pslog: invalid process id: %s\n\n", argv[1]);
     usage();
  }
  else if (!strcmp("-V", argv[1]) || !strcmp("--version", argv[1])) {
    print_version();
    return 0;
  }

  regfree(&re_pid);
  regcomp(&re_log, "^(.*log)$",REG_EXTENDED|REG_NOSUB);

  /*
   * At this point, all arguments are in the form /proc/nnnn
   * or nnnn, so a simple check based on the first char is
   * possible.
   */

  struct dirent *namelist;

  char* linkpath = (char*) malloc(PATH_MAX+1);
  if (!linkpath) {
    perror ("malloc");
    return 1;
  }

  ssize_t linkname_size;
  char buf[PATH_MAX+1];
  DIR *pid_dir;

  if (argv[1][0] != '/') {
    if (asprintf(&fullpath, "/proc/%s/fd/", argv[1]) < 0) {
      perror ("asprintf");
      return 1;
    }
  } else {
    if (asprintf(&fullpath, "%s/fd/", argv[1]) < 0) {
        perror("asprintf");
        return 1;
    }
  }

  pid_dir = opendir(fullpath);
  if (!pid_dir) {
    perror("opendir");
    free(fullpath);
    return 1;
  }

  fprintf(stdout, "Pid no %s:\n", argv[1]);

  while((namelist = readdir(pid_dir))) {
    strncpy(linkpath, fullpath, PATH_MAX);
    strncat(linkpath, namelist->d_name, PATH_MAX - strlen(linkpath));
    linkname_size = readlink(linkpath, buf, PATH_MAX -1);
    buf[linkname_size+1] = '\0';

    if (regexec(&re_log, buf, 0, NULL, 0) == 0) {
      fprintf(stdout, "Log path: %s\n", buf);
    }
    memset(&linkpath[0], 0, sizeof(*linkpath));
    memset(&buf[0], 0, sizeof(buf));
  }

  free(linkpath);
  free(fullpath);
  regfree(&re_log);

  if (closedir(pid_dir)) {
    perror ("closedir");
    return 1;
  }

  return 0;
}

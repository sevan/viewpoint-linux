/* Determine the program name of a given process.
   Copyright (C) 2019-2020 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2019.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#ifndef _GET_PROGNAME_OF_H
#define _GET_PROGNAME_OF_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the base name of the program that executes the given process,
   possibly truncated, as a freshly allocated string, or NULL if it cannot
   be determined.  */
extern char *get_progname_of (pid_t pid);

#ifdef __cplusplus
}
#endif

#endif /* _GET_PROGNAME_OF_H */

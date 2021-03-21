/* bugreports.h -- explain how to report bugs
   Copyright (C) 2016-2021 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
/* Written by James Youngman <jay@gnu.org>.
 */
#if !defined BUGREPORTS_H
# define BUGREPORTS_H
# include <stdio.h>

/* Interpreetation of the return code is as for fprintf. */
int explain_how_to_report_bugs (FILE *f, const char *program_name);

#endif

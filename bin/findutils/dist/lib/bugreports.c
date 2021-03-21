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
#include <config.h>
#include <stdio.h>

#include "bugreports.h"
#include "system.h"

int
explain_how_to_report_bugs (FILE *f, const char *program_name)
{
  return fprintf (f,_(""
"Please see also the documentation at %s.\n"
"You can report (and track progress on fixing) bugs in the \"%s\"\n"
"program via the %s bug-reporting page at\n"
"%s or, if\n"
"you have no web access, by sending email to <%s>.\n"),
		  PACKAGE_URL,
		  program_name,
		  PACKAGE_NAME,
		  PACKAGE_BUGREPORT_URL,
		  PACKAGE_BUGREPORT);
}

/* safe-atoi.c -- checked string-to-int conversion.
   Copyright (C) 2007-2021 Free Software Foundation, Inc.

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
/* config.h must be included first. */
#include <config.h>

/* system headers. */
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

/* gnulib headers. */
#include "quotearg.h"

/* find headers. */
#include "system.h"
#include "die.h"
#include "safe-atoi.h"


int
safe_atoi (const char *s, enum quoting_style style)
{
  long lval;
  char *end;

  errno = 0;
  lval = strtol (s, &end, 10);
  if ( (LONG_MAX == lval) || (LONG_MIN == lval) )
    {
      /* max/min possible value, or an error. */
      if (errno == ERANGE)
	{
	  /* too big, or too small. */
	  die (EXIT_FAILURE, errno, "%s", s);
	}
      else
	{
	  /* not a valid number */
	  die (EXIT_FAILURE, errno, "%s", s);
	}
      /* Otherwise, we do a range check against INT_MAX and INT_MIN
       * below.
       */
    }

  if (lval > INT_MAX || lval < INT_MIN)
    {
      /* The number was in range for long, but not int. */
      errno = ERANGE;
      die (EXIT_FAILURE, errno, "%s", s);
    }
  else if (*end)
    {
      die (EXIT_FAILURE, errno, _("Unexpected suffix %s on %s"),
	   quotearg_n_style (0, style, end),
	   quotearg_n_style (1, style, s));
    }
  else if (end == s)
    {
      die (EXIT_FAILURE, errno, _("Expected an integer: %s"),
	   quotearg_n_style (0, style, s));
    }
  return (int)lval;
}

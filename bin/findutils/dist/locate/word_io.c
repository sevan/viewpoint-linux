/* word_io.c -- word oriented I/O
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
#include <stdbool.h>		/* for bool */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* gnulib headers. */
#include "byteswap.h"
#include "error.h"
#include "quotearg.h"

/* find headers. */
#include "system.h"
#include "die.h"
#include "locatedb.h"


enum { WORDBYTES=4 };

static int
decode_value (const unsigned char data[],
	      int limit,
	      GetwordEndianState *endian_state_flag,
	      const char *filename)
{
  int swapped;
  union
  {
    int ival;			/* native representation */
    unsigned char data[WORDBYTES];
  } u;
  u.ival = 0;
  memcpy (&u.data, data, WORDBYTES);
  swapped = bswap_32(u.ival);	/* byteswapped */

  if (*endian_state_flag == GetwordEndianStateInitial)
    {
      if (u.ival <= limit)
	{
	  if (swapped > limit)
	    {
	      /* the native value is inside the limit and the
	       * swapped value is not.  We take this as proof
	       * that we should be using the native byte order.
	       */
	      *endian_state_flag = GetwordEndianStateNative;
	    }
	  return u.ival;
	}
      else
	{
	  if (swapped <= limit)
	    {
	      /* Aha, now we know we have to byte-swap. */
	      error (0, 0,
		     _("WARNING: locate database %s was "
		       "built with a different byte order"),
		     quotearg_n_style (0, locale_quoting_style, filename));
	      *endian_state_flag = GetwordEndianStateSwab;
	      return swapped;
	    }
	  else
	    {
	      /* u.ival > limit and swapped > limit.  For the moment, assume
	       * native ordering.
	       */
	      return u.ival;
	    }
	}
    }
  else
    {
      /* We already know the byte order. */
      if (*endian_state_flag == GetwordEndianStateSwab)
	return swapped;
      else
	return u.ival;
    }
}



int
getword (FILE *fp,
	 const char *filename,
	 size_t maxvalue,
	 GetwordEndianState *endian_state_flag)
{
  unsigned char data[4];
  size_t bytes_read;

  clearerr (fp);
  bytes_read = fread (data, WORDBYTES, 1, fp);
  if (bytes_read != 1)
    {
      const char * quoted_name = quotearg_n_style (0, locale_quoting_style,
						   filename);
      /* Distinguish between a truncated database and an I/O error.
       * Either condition is fatal.
       */
      if (feof (fp))
	die (EXIT_FAILURE, 0, _("unexpected EOF in %s"), quoted_name);
      else
	die (EXIT_FAILURE, errno,
	     _("error reading a word from %s"), quoted_name);
      abort ();
    }
  else
    {
      return decode_value (data, maxvalue, endian_state_flag, filename);
    }
}

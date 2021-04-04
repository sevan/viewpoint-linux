/* cleansess.c - Clean up the pty and frob utmp/wtmp accordingly after logout
  Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
  2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015,
  2016, 2017, 2018, 2019, 2020, 2021 Free Software Foundation, Inc.

  This file is part of GNU Inetutils.

  GNU Inetutils is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or (at
  your option) any later version.

  GNU Inetutils is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see `http://www.gnu.org/licenses/'. */

/* Written by Miles Bader.  */

#include <config.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#ifdef HAVE_UTMP_H
# ifdef HAVE_UTIL_H
#  include <util.h>
# endif
# ifdef HAVE_LIBUTIL_H
#  include <libutil.h>
# endif
# include <utmp.h>
#else
# ifdef  HAVE_UTMPX_H
#  include <utmpx.h>
#  define utmp utmpx		/* make utmpx look more like utmp */
# endif
#endif
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined HAVE_LOGOUT && HAVE_LOGWTMP

/* Update umtp & wtmp as necessary, and change tty & pty permissions back to
   what they should be.  */
void
cleanup_session (char *tty, int pty_fd)
{
  char *line;

# ifdef PATH_TTY_PFX
  if (strncmp (tty, PATH_TTY_PFX, sizeof PATH_TTY_PFX - 1) == 0)
    line = tty + sizeof PATH_TTY_PFX - 1;
  else
# endif /* PATH_TTY_PFX */
    line = tty;

  if (logout (line))
    logwtmp (line, "", "");

  chmod (tty, 0666);
  chown (tty, 0, 0);
  fchmod (pty_fd, 0666);
  fchown (pty_fd, 0, 0);
}
#endif /* HAVE_LOGOUT && HAVE_LOGWTMP */

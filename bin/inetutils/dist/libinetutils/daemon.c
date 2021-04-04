/*
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

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <error.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unused-parameter.h>

/*
  According to Unix-FAQ maintained by Andrew Gierth:

  1.fork() so the parent can exit, this returns control to the command
  line or shell invoking your program. This step is required so that the
  new process is guaranteed not to be a process group leader. The next
  step, setsid(), fails if you're a process group leader.

  2.setsid() to become a process group and session group leader. Since a
  controlling terminal is associated with a session, and this new session
  has not yet acquired a controlling terminal our process now has no
  controlling terminal, which is a Good Thing for daemons.

  3.fork() again so the parent, (the session group leader), can exit. This
  means that we, as a non-session group leader, can never regain a
  controlling terminal.

  4.chdir("/") to ensure that our process doesn't keep any directory in use.
  Failure to do this could make it so that an administrator couldn't unmount
  a file system, because it was our current directory.
  [Equivalently, we could change to any directory containing files important
  to the daemon's operation.]

  5.umask(0) so that we have complete control over the permissions of
  anything we write. We don't know what umask we may have inherited.
  [This step is optional]

  6.close() fds 0, 1, and 2. This releases the standard in, out, and error
  we inherited from our parent process. We have no way of knowing where
  these fds might have been redirected to. Note that many daemons use
  sysconf() to determine the limit _SC_OPEN_MAX. _SC_OPEN_MAX tells you the
  maximun open files/process. Then in a loop, the daemon can close all
  possible file descriptors. You have to decide if you need to do this or not.
  If you think that there might be file-descriptors open you should close
  them, since there's a limit on number of concurrent file descriptors.

  7.Establish new open descriptors for stdin, stdout and stderr. Even if
  you don't plan to use them, it is still a good idea to have them open.
  The precise handling of these is a matter of taste; if you have a logfile,
  for example, you might wish to open it as stdout or stderr, and open
  `/dev/null' as stdin; alternatively, you could open `/dev/console' as
  stderr and/or stdout, and `/dev/null' as stdin, or any other combination
  that makes sense for your particular daemon.  */

#define MAXFD 64

void
waitdaemon_timeout (int signo _GL_UNUSED_PARAMETER)
{
  int left;

  left = alarm (0);
  signal (SIGALRM, SIG_DFL);
  if (left == 0)
    error (EXIT_FAILURE, 0, "timed out waiting for child");
}

/* waitdaemon is like daemon, but optionally the parent pause up
   until maxwait before exiting. Return -1, on error, otherwise
   waitdaemon will return the pid of the parent.  */

int
waitdaemon (int nochdir, int noclose, int maxwait)
{
  int fd;
  pid_t childpid;
  pid_t ppid;

  ppid = getpid ();

  switch (childpid = fork ())
    {
    case -1:			/* Something went wrong.  */
      return (-1);

    case 0:			/* In the child.  */
      break;

    default:			/* In the parent.  */
      if (maxwait > 0)
	{
	  signal (SIGALRM, waitdaemon_timeout);
	  alarm (maxwait);
	  pause ();
	}
      _exit (EXIT_SUCCESS);
    }

  if (setsid () == -1)
    return -1;

  /* SIGHUP must be ignored, because when the session leader terminates,
     then SIGHUP is sent to all process belonging to the same session,
     i.e., also to the second child.
   */
  signal (SIGHUP, SIG_IGN);

  switch (fork ())
    {
    case 0:
      break;

    case -1:
      return -1;

    default:
      _exit (EXIT_SUCCESS);
    }

  if (!nochdir && chdir ("/") < 0)
    return -1;			/* Unlikely failure, but check it.  */

  if (!noclose)
    {
      int i;
      long fdlimit = -1;

#if defined _SC_OPEN_MAX
      fdlimit = sysconf (_SC_OPEN_MAX);
#elif defined (HAVE_GETDTABLESIZE)
      fdlimit = getdtablesize ();
#endif

      if (fdlimit == -1)
	fdlimit = MAXFD;

      for (i = 0; i < fdlimit; i++)
	close (i);

      fd = open (PATH_DEVNULL, O_RDWR, 0);
      if (fd != -1)
	{
	  dup2 (fd, STDIN_FILENO);
	  dup2 (fd, STDOUT_FILENO);
	  dup2 (fd, STDERR_FILENO);
	  if (fd > 2)
	    close (fd);
	}
    }
  return ppid;
}

int
daemon (int nochdir, int noclose)
{
  return (waitdaemon (nochdir, noclose, 0) == -1) ? -1 : 0;
}

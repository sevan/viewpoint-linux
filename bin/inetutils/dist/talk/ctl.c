/*
  Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
  2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014,
  2015, 2016, 2017, 2018, 2019, 2020, 2021 Free Software Foundation,
  Inc.

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
 * Copyright (c) 1983, 1993
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

/*
 * This file handles haggling with the various talk daemons to
 * get a socket to talk to. sockt is opened and connected in
 * the progress
 */

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <protocols/talkd.h>
#include <netinet/in.h>
#include "talk.h"
#include "talk_ctl.h"

struct sockaddr_in daemon_addr;
struct sockaddr_in ctl_addr;
struct sockaddr_in my_addr;

	/* inet addresses of the two machines */
struct in_addr my_machine_addr;
struct in_addr his_machine_addr;

unsigned short daemon_port;		/* port number of the talk daemon */

int ctl_sockt;
int sockt;
int invitation_waiting = 0;

CTL_MSG msg;

int
open_sockt (void)
{
  socklen_t length;

  my_addr.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  my_addr.sin_len = sizeof (my_addr);
#endif
  my_addr.sin_addr = my_machine_addr;
  my_addr.sin_port = 0;
  sockt = socket (AF_INET, SOCK_STREAM, 0);
  if (sockt <= 0)
    p_error ("Bad socket");
  if (bind (sockt, (struct sockaddr *) &my_addr, sizeof (my_addr)) != 0)
    p_error ("Binding local socket");
  length = sizeof (my_addr);
  if (getsockname (sockt, (struct sockaddr *) &my_addr, &length) == -1)
    p_error ("Bad address for socket");

  return 0;
}

/* open the ctl socket */
int
open_ctl (void)
{
  socklen_t length;

  ctl_addr.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  ctl_addr.sin_len = sizeof (ctl_addr);
#endif
  ctl_addr.sin_port = 0;
  ctl_addr.sin_addr = my_machine_addr;
  ctl_sockt = socket (AF_INET, SOCK_DGRAM, 0);
  if (ctl_sockt <= 0)
    p_error ("Bad socket");
  if (bind (ctl_sockt, (struct sockaddr *) &ctl_addr, sizeof (ctl_addr)) != 0)
    p_error ("Couldn't bind to control socket");
  length = sizeof (ctl_addr);
  if (getsockname (ctl_sockt, (struct sockaddr *) &ctl_addr, &length) == -1)
    p_error ("Bad address for ctl socket");

  return 0;
}

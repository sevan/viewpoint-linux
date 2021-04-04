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
 * Copyright (c) 1988, 1990, 1993
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

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <fcntl.h>

#include <signal.h>
#include <netdb.h>
#include <ctype.h>
#include <pwd.h>
#include <stdarg.h>
#include <errno.h>

#include <stdlib.h>
#include <limits.h>	/* LLONG_MAX for Solaris. */

#include <arpa/inet.h>
#include <arpa/telnet.h>

#if defined HAVE_IDN2_H && defined HAVE_IDN2
# include <idn2.h>
#elif defined HAVE_IDNA_H
# include <idna.h>
#endif

#include <unused-parameter.h>
#include <libinetutils.h>

#include "general.h"

#include "ring.h"

#include "externs.h"
#include "defines.h"
#include "types.h"

#include "xalloc.h"
#include "xvasprintf.h"
#include "xalloc.h"

#ifdef	ENCRYPTION
# include <libtelnet/encrypt.h>
#endif
#if defined AUTHENTICATION || defined ENCRYPTION
# include <libtelnet/misc.h>
#endif

#if !defined CRAY && !defined sysV88
# ifdef HAVE_NETINET_IN_SYSTM_H
#  include <netinet/in_systm.h>
# endif
# if (defined vax || defined tahoe || defined hp300) && !defined ultrix
#  include <machine/endian.h>
# endif	/* vax */
#endif /* !defined(CRAY) && !defined(sysV88) */

#ifdef HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif

char *hostname = 0;

extern char *getenv (const char *);

extern int isprefix (register char *s1, register char *s2);
extern char **genget (char *, char **, int);
extern int Ambiguous ();

typedef int (*intrtn_t) (int, char *[]);
static int call (intrtn_t, ...);

typedef struct
{
  const char *name;		/* command name */
  const char *help;		/* help string (NULL for no help) */
  int (*handler) ();		/* routine which executes command */
  int needconnect;		/* Do we need to be connected to execute? */
} Command;

static char line[256];
static char saveline[256];
static int margc;
static char *margv[20];

static void
makeargv (void)
{
  register char *cp, *cp2, c;
  register char **argp = margv;

  margc = 0;
  cp = line;
  if (*cp == '!')
    {				/* Special case shell escape */
      strcpy (saveline, line);	/* save for shell command */
      *argp++ = "!";		/* No room in string to get this */
      margc++;
      cp++;
    }
  while ((c = *cp))
    {
      register int inquote = 0;
      while (isspace (c))
	c = *++cp;
      if (c == '\0')
	break;
      *argp++ = cp;
      margc += 1;
      for (cp2 = cp; c != '\0'; c = *++cp)
	{
	  if (inquote)
	    {
	      if (c == inquote)
		{
		  inquote = 0;
		  continue;
		}
	    }
	  else
	    {
	      if (c == '\\')
		{
		  if ((c = *++cp) == '\0')
		    break;
		}
	      else if (c == '"')
		{
		  inquote = '"';
		  continue;
		}
	      else if (c == '\'')
		{
		  inquote = '\'';
		  continue;
		}
	      else if (isspace (c))
		break;
	    }
	  *cp2++ = c;
	}
      *cp2 = '\0';
      if (c == '\0')
	break;
      cp++;
    }
  *argp++ = 0;
}

/*
 * Make a character string into a number.
 *
 * Todo:  1.  Could take random integers (12, 0x12, 012, 0b1).
 */

static int
special (register char *s)
{
  register char c;
  char b;

  switch (*s)
    {
    case '^':
      b = *++s;
      if (b == '?')
	{
	  c = b | 0x40;		/* DEL */
	}
      else
	{
	  c = b & 0x1f;
	}
      break;
    default:
      c = *s;
      break;
    }
  return c;
}

/*
 * Construct a control character sequence
 * for a special character.
 */
static const char *
control (register cc_t c)
{
  static char buf[5];
  /*
   * The only way I could get the Sun 3.5 compiler
   * to shut up about
   *      if ((unsigned int)c >= 0x80)
   * was to assign "c" to an unsigned int variable...
   * Arggg....
   */
  register unsigned int uic = (unsigned int) c;

  if (uic == 0x7f)
    return ("^?");
  if (c == (cc_t) _POSIX_VDISABLE)
    {
      return "off";
    }
  if (uic >= 0x80)
    {
      buf[0] = '\\';
      buf[1] = ((c >> 6) & 07) + '0';
      buf[2] = ((c >> 3) & 07) + '0';
      buf[3] = (c & 07) + '0';
      buf[4] = 0;
    }
  else if (uic >= 0x20)
    {
      buf[0] = c;
      buf[1] = 0;
    }
  else
    {
      buf[0] = '^';
      buf[1] = '@' + c;
      buf[2] = 0;
    }
  return (buf);
}



/*
 *	The following are data structures and routines for
 *	the "send" command.
 *
 */

struct sendlist
{
  const char *name;		/* How user refers to it (case independent) */
  const char *help;		/* Help information (0 ==> no help) */
  int needconnect;		/* Need to be connected */
  int narg;			/* Number of arguments */
  int (*handler) ();		/* Routine to perform (for special ops) */
  int nbyte;			/* Number of bytes to send this command */
  int what;			/* Character to be sent (<0 ==> special) */
};


static int
send_esc (void),
send_help (void),
send_docmd (char *),
send_dontcmd (char *), send_willcmd (char *), send_wontcmd (char *);

static struct sendlist Sendlist[] = {
  {"ao", "Send Telnet Abort output", 1, 0, 0, 2, AO},
  {"ayt", "Send Telnet 'Are You There'", 1, 0, 0, 2, AYT},
  {"brk", "Send Telnet Break", 1, 0, 0, 2, BREAK},
  {"break", 0, 1, 0, 0, 2, BREAK},
  {"ec", "Send Telnet Erase Character", 1, 0, 0, 2, EC},
  {"el", "Send Telnet Erase Line", 1, 0, 0, 2, EL},
  {"escape", "Send current escape character", 1, 0, send_esc, 1, 0},
  {"ga", "Send Telnet 'Go Ahead' sequence", 1, 0, 0, 2, GA},
  {"ip", "Send Telnet Interrupt Process", 1, 0, 0, 2, IP},
  {"intp", 0, 1, 0, 0, 2, IP},
  {"interrupt", 0, 1, 0, 0, 2, IP},
  {"intr", 0, 1, 0, 0, 2, IP},
  {"nop", "Send Telnet 'No operation'", 1, 0, 0, 2, NOP},
  {"eor", "Send Telnet 'End of Record'", 1, 0, 0, 2, EOR},
  {"abort", "Send Telnet 'Abort Process'", 1, 0, 0, 2, ABORT},
  {"susp", "Send Telnet 'Suspend Process'", 1, 0, 0, 2, SUSP},
  {"eof", "Send Telnet End of File Character", 1, 0, 0, 2, xEOF},
  {"synch", "Perform Telnet 'Synch operation'", 1, 0, dosynch, 2, 0},
  {"getstatus", "Send request for STATUS", 1, 0, get_status, 6, 0},
  {"?", "Display send options", 0, 0, send_help, 0, 0},
  {"help", 0, 0, 0, send_help, 0, 0},
  {"do", 0, 0, 1, send_docmd, 3, 0},
  {"dont", 0, 0, 1, send_dontcmd, 3, 0},
  {"will", 0, 0, 1, send_willcmd, 3, 0},
  {"wont", 0, 0, 1, send_wontcmd, 3, 0},
  {NULL, 0, 0, 0, NULL, 0, 0}
};

#define GETSEND(name) ((struct sendlist *) genget(name, (char **) Sendlist, \
				sizeof(struct sendlist)))

static int
sendcmd (int argc, char **argv)
{
  int count;			/* how many bytes we are going to need to send */
  int i;
  struct sendlist *s;		/* pointer to current command */
  int success = 0;
  int needconnect = 0;

  if (argc < 2)
    {
      printf ("need at least one argument for 'send' command\n");
      printf ("'send ?' for help\n");
      return 0;
    }
  /*
   * First, validate all the send arguments.
   * In addition, we see how much space we are going to need, and
   * whether or not we will be doing a "SYNCH" operation (which
   * flushes the network queue).
   */
  count = 0;
  for (i = 1; i < argc; i++)
    {
      s = GETSEND (argv[i]);
      if (s == 0)
	{
	  printf ("Unknown send argument '%s'\n'send ?' for help.\n",
		  argv[i]);
	  return 0;
	}
      else if (Ambiguous (s))
	{
	  printf ("Ambiguous send argument '%s'\n'send ?' for help.\n",
		  argv[i]);
	  return 0;
	}
      if (i + s->narg >= argc)
	{
	  fprintf (stderr,
		   "Need %d argument%s to 'send %s' command.  'send %s ?' for help.\n",
		   s->narg, s->narg == 1 ? "" : "s", s->name, s->name);
	  return 0;
	}
      count += s->nbyte;
      if (s->handler == send_help)
	{
	  send_help ();
	  return 0;
	}

      i += s->narg;
      needconnect += s->needconnect;
    }
  if (!connected && needconnect)
    {
      printf ("?Need to be connected first.\n");
      printf ("'send ?' for help\n");
      return 0;
    }
  /* Now, do we have enough room? */
  if (NETROOM () < count)
    {
      printf ("There is not enough room in the buffer TO the network\n");
      printf ("to process your request.  Nothing will be done.\n");
      printf ("('send synch' will throw away most data in the network\n");
      printf ("buffer, if this might help.)\n");
      return 0;
    }
  /* OK, they are all OK, now go through again and actually send */
  count = 0;
  for (i = 1; i < argc; i++)
    {
      if ((s = GETSEND (argv[i])) == 0)
	{
	  fprintf (stderr, "Telnet 'send' error - argument disappeared!\n");
	  quit ();
	}
      if (s->handler)
	{
	  count++;
	  success += (*s->handler) ((s->narg > 0) ? argv[i + 1] : 0,
				    (s->narg > 1) ? argv[i + 2] : 0);
	  i += s->narg;
	}
      else
	{
	  NET2ADD (IAC, s->what);
	  printoption ("SENT", IAC, s->what);
	}
    }
  return (count == success);
}

static int
send_esc (void)
{
  NETADD (escape);
  return 1;
}

int
send_tncmd (void (*func) (), char *cmd, char *name)
{
  char **cpp;
#if !HAVE_DECL_TELOPTS
  extern char *telopts[];
#endif
  register int val = 0;

  if (isprefix (name, "help") || isprefix (name, "?"))
    {
      register int col, len;

      printf ("Usage: send %s <value|option>\n", cmd);
      printf ("\"value\" must be from 0 to 255\n");
      printf ("Valid options are:\n\t");

      col = 8;
      for (cpp = telopts; *cpp; cpp++)
	{
	  len = strlen (*cpp) + 3;
	  if (col + len > 65)
	    {
	      printf ("\n\t");
	      col = 8;
	    }
	  printf (" \"%s\"", *cpp);
	  col += len;
	}
      printf ("\n");
      return 0;
    }
  cpp = (char **) genget (name, telopts, sizeof (char *));
  if (Ambiguous (cpp))
    {
      fprintf (stderr, "'%s': ambiguous argument ('send %s ?' for help).\n",
	       name, cmd);
      return 0;
    }
  if (cpp)
    {
#if HAVE_DECL_TELOPTS
      val = cpp - (char **) telopts;
#else /* !HAVE_DECL_TELOPTS */
      val = cpp - telopts;
#endif
    }
  else
    {
      register char *cp = name;

      while (*cp >= '0' && *cp <= '9')
	{
	  val *= 10;
	  val += *cp - '0';
	  cp++;
	}
      if (*cp != 0)
	{
	  fprintf (stderr, "'%s': unknown argument ('send %s ?' for help).\n",
		   name, cmd);
	  return 0;
	}
      else if (val < 0 || val > 255)
	{
	  fprintf (stderr, "'%s': bad value ('send %s ?' for help).\n",
		   name, cmd);
	  return 0;
	}
    }
  if (!connected)
    {
      printf ("?Need to be connected first.\n");
      return 0;
    }
  (*func) (val, 1);
  return 1;
}

static int
send_docmd (char *name)
{
  return (send_tncmd (send_do, "do", name));
}

static int
send_dontcmd (char *name)
{
  return (send_tncmd (send_dont, "dont", name));
}

static int
send_willcmd (char *name)
{
  return (send_tncmd (send_will, "will", name));
}

static int
send_wontcmd (char *name)
{
  return (send_tncmd (send_wont, "wont", name));
}

static int
send_help (void)
{
  struct sendlist *s;		/* pointer to current command */
  for (s = Sendlist; s->name; s++)
    {
      if (s->help)
	printf ("%-15s %s\n", s->name, s->help);
    }
  return (0);
}

/*
 * The following are the routines and data structures referred
 * to by the arguments to the "toggle" command.
 */

static int
lclchars (void)
{
  donelclchars = 1;
  return 1;
}

static int
togdebug (void)
{
#ifndef NOT43
  if (net > 0 && (SetSockOpt (net, SOL_SOCKET, SO_DEBUG, debug)) < 0)
    {
      perror ("setsockopt (SO_DEBUG)");
    }
#else /* NOT43 */
  if (debug)
    {
      if (net > 0 && SetSockOpt (net, SOL_SOCKET, SO_DEBUG, 0, 0) < 0)
	perror ("setsockopt (SO_DEBUG)");
    }
  else
    printf ("Cannot turn off socket debugging\n");
#endif /* NOT43 */
  return 1;
}


static int
togcrlf (void)
{
  if (crlf)
    {
      printf ("Will send carriage returns as telnet <CR><LF>.\n");
    }
  else
    {
      printf ("Will send carriage returns as telnet <CR><NUL>.\n");
    }
  return 1;
}

int binmode;

static int
togbinary (int val)
{
  donebinarytoggle = 1;

  if (val >= 0)
    {
      binmode = val;
    }
  else
    {
      if (my_want_state_is_will (TELOPT_BINARY) &&
	  my_want_state_is_do (TELOPT_BINARY))
	{
	  binmode = 1;
	}
      else if (my_want_state_is_wont (TELOPT_BINARY) &&
	       my_want_state_is_dont (TELOPT_BINARY))
	{
	  binmode = 0;
	}
      val = binmode ? 0 : 1;
    }

  if (val == 1)
    {
      if (my_want_state_is_will (TELOPT_BINARY) &&
	  my_want_state_is_do (TELOPT_BINARY))
	{
	  printf ("Already operating in binary mode with remote host.\n");
	}
      else
	{
	  printf ("Negotiating binary mode with remote host.\n");
	  tel_enter_binary (3);
	}
    }
  else
    {
      if (my_want_state_is_wont (TELOPT_BINARY) &&
	  my_want_state_is_dont (TELOPT_BINARY))
	{
	  printf ("Already in network ascii mode with remote host.\n");
	}
      else
	{
	  printf ("Negotiating network ascii mode with remote host.\n");
	  tel_leave_binary (3);
	}
    }
  return 1;
}

static int
togrbinary (int val)
{
  donebinarytoggle = 1;

  if (val == -1)
    val = my_want_state_is_do (TELOPT_BINARY) ? 0 : 1;

  if (val == 1)
    {
      if (my_want_state_is_do (TELOPT_BINARY))
	{
	  printf ("Already receiving in binary mode.\n");
	}
      else
	{
	  printf ("Negotiating binary mode on input.\n");
	  tel_enter_binary (1);
	}
    }
  else
    {
      if (my_want_state_is_dont (TELOPT_BINARY))
	{
	  printf ("Already receiving in network ascii mode.\n");
	}
      else
	{
	  printf ("Negotiating network ascii mode on input.\n");
	  tel_leave_binary (1);
	}
    }
  return 1;
}

static int
togxbinary (int val)
{
  donebinarytoggle = 1;

  if (val == -1)
    val = my_want_state_is_will (TELOPT_BINARY) ? 0 : 1;

  if (val == 1)
    {
      if (my_want_state_is_will (TELOPT_BINARY))
	{
	  printf ("Already transmitting in binary mode.\n");
	}
      else
	{
	  printf ("Negotiating binary mode on output.\n");
	  tel_enter_binary (2);
	}
    }
  else
    {
      if (my_want_state_is_wont (TELOPT_BINARY))
	{
	  printf ("Already transmitting in network ascii mode.\n");
	}
      else
	{
	  printf ("Negotiating network ascii mode on output.\n");
	  tel_leave_binary (2);
	}
    }
  return 1;
}


static int togglehelp (void);
#if defined AUTHENTICATION
extern int auth_togdebug (int);
#endif
#ifdef	ENCRYPTION
extern int EncryptAutoEnc (int);
extern int EncryptAutoDec (int);
extern int EncryptDebug (int);
extern int EncryptVerbose (int);
#endif /* ENCRYPTION */

struct togglelist
{
  const char *name;		/* name of toggle */
  const char *help;		/* help message */
  int (*handler) ();		/* routine to do actual setting */
  int *variable;
  const char *actionexplanation;
};

static struct togglelist Togglelist[] = {
  {"autoflush",
   "flushing of output when sending interrupt characters",
   0,
   &autoflush,
   "flush output when sending interrupt characters"},
  {"autosynch",
   "automatic sending of interrupt characters in urgent mode",
   0,
   &autosynch,
   "send interrupt characters in urgent mode"},
#if defined AUTHENTICATION
  {"autologin",
   "automatic sending of login and/or authentication info",
   0,
   &autologin,
   "send login name and/or authentication information"},
  {"authdebug",
   "Toggle authentication debugging",
   auth_togdebug,
   0,
   "print authentication debugging information"},
#endif
#ifdef	ENCRYPTION
  {"autoencrypt",
   "automatic encryption of data stream",
   EncryptAutoEnc,
   0,
   "automatically encrypt output"},
  {"autodecrypt",
   "automatic decryption of data stream",
   EncryptAutoDec,
   0,
   "automatically decrypt input"},
  {"verbose_encrypt",
   "Toggle verbose encryption output",
   EncryptVerbose,
   0,
   "print verbose encryption output"},
  {"encdebug",
   "Toggle encryption debugging",
   EncryptDebug,
   0,
   "print encryption debugging information"},
#endif /* ENCRYPTION */
  {"skiprc",
   "don't read ~/.telnetrc file",
   0,
   &skiprc,
   "skip reading of ~/.telnetrc file"},
  {"binary",
   "sending and receiving of binary data",
   togbinary,
   0,
   0},
  {"inbinary",
   "receiving of binary data",
   togrbinary,
   0,
   0},
  {"outbinary",
   "sending of binary data",
   togxbinary,
   0,
   0},
  {"crlf",
   "sending carriage returns as telnet <CR><LF>",
   togcrlf,
   &crlf,
   0},
  {"crmod",
   "mapping of received carriage returns",
   0,
   &crmod,
   "map carriage return on output"},
  {"localchars",
   "local recognition of certain control characters",
   lclchars,
   &localchars,
   "recognize certain control characters"},
  {" ", "", 0, NULL, NULL},			/* empty line */
#if (defined unix || defined __unix || defined __unix__) && defined TN3270
  {"apitrace",
   "(debugging) toggle tracing of API transactions",
   0,
   &apitrace,
   "trace API transactions"},
  {"cursesdata",
   "(debugging) toggle printing of hexadecimal curses data",
   0,
   &cursesdata,
   "print hexadecimal representation of curses data"},
#endif /* (unix || __unix || __unix__)) && TN3270 */
  {"debug",
   "debugging",
   togdebug,
   &debug,
   "turn on socket level debugging"},
  {"netdata",
   "printing of hexadecimal network data (debugging)",
   0,
   &netdata,
   "print hexadecimal representation of network traffic"},
  {"prettydump",
   "output of \"netdata\" to user readable format (debugging)",
   0,
   &prettydump,
   "print user readable output for \"netdata\""},
  {"options",
   "viewing of options processing (debugging)",
   0,
   &showoptions,
   "show option processing"},
  {"termdata",
   "(debugging) toggle printing of hexadecimal terminal data",
   0,
   &termdata,
   "print hexadecimal representation of terminal traffic"},
  {"?",
   0,
   togglehelp, NULL, NULL},
  {"help",
   0,
   togglehelp, NULL, NULL},
  {NULL, NULL, 0, NULL, NULL}
};

static int
togglehelp (void)
{
  struct togglelist *c;

  for (c = Togglelist; c->name; c++)
    {
      if (c->help)
	{
	  if (*c->help)
	    printf ("%-15s toggle %s\n", c->name, c->help);
	  else
	    printf ("\n");
	}
    }
  printf ("\n");
  printf ("%-15s %s\n", "?", "display help information");
  return 0;
}

static void
settogglehelp (int set)
{
  struct togglelist *c;

  for (c = Togglelist; c->name; c++)
    {
      if (c->help)
	{
	  if (*c->help)
	    printf ("%-15s %s %s\n", c->name, set ? "enable" : "disable",
		    c->help);
	  else
	    printf ("\n");
	}
    }
}

#define GETTOGGLE(name) (struct togglelist *) \
		genget(name, (char **) Togglelist, sizeof(struct togglelist))

static int
toggle (int argc, char *argv[])
{
  int retval = 1;
  char *name;
  struct togglelist *c;

  if (argc < 2)
    {
      fprintf (stderr,
	       "Need an argument to 'toggle' command.  'toggle ?' for help.\n");
      return 0;
    }
  argc--;
  argv++;
  while (argc--)
    {
      name = *argv++;
      c = GETTOGGLE (name);
      if (Ambiguous (c))
	{
	  fprintf (stderr,
		   "'%s': ambiguous argument ('toggle ?' for help).\n", name);
	  return 0;
	}
      else if (c == 0)
	{
	  fprintf (stderr, "'%s': unknown argument ('toggle ?' for help).\n",
		   name);
	  return 0;
	}
      else
	{
	  if (c->variable)
	    {
	      *c->variable = !*c->variable;	/* invert it */
	      if (c->actionexplanation)
		{
		  printf ("%s %s.\n", *c->variable ? "Will" : "Won't",
			  c->actionexplanation);
		}
	    }
	  if (c->handler)
	    {
	      retval &= (*c->handler) (-1);
	    }
	}
    }
  return retval;
}

/*
 * The following perform the "set" command.
 */

#ifdef	USE_TERMIO
struct termio new_tc = { 0 };
#endif

struct setlist
{
  const char *name;		/* name */
  const char *help;		/* help information */
  void (*handler) ();
  cc_t *charp;			/* where it is located at */
};

static struct setlist Setlist[] = {
#ifdef	KLUDGELINEMODE
  {"echo", "character to toggle local echoing on/off", 0, &echoc},
#endif
  {"escape", "character to escape back to telnet command mode", 0, &escape},
  {"rlogin", "rlogin escape character", 0, &rlogin},
  {"tracefile", "file to write trace information to", SetNetTrace,
   (cc_t *) NetTraceFile},
  {" ", "", 0, NULL},
  {" ", "The following need 'localchars' to be toggled true", 0, 0},
  {"flushoutput", "character to cause an Abort Output", 0, termFlushCharp},
  {"interrupt", "character to cause an Interrupt Process", 0, termIntCharp},
  {"quit", "character to cause an Abort process", 0, termQuitCharp},
  {"eof", "character to cause an EOF ", 0, termEofCharp},
  {" ", "", 0, NULL},
  {" ", "The following are for local editing in linemode", 0, 0},
  {"erase", "character to use to erase a character", 0, termEraseCharp},
  {"kill", "character to use to erase a line", 0, termKillCharp},
  {"lnext", "character to use for literal next", 0, termLiteralNextCharp},
  {"susp", "character to cause a Suspend Process", 0, termSuspCharp},
  {"reprint", "character to use for line reprint", 0, termRprntCharp},
  {"worderase", "character to use to erase a word", 0, termWerasCharp},
  {"start", "character to use for XON", 0, termStartCharp},
  {"stop", "character to use for XOFF", 0, termStopCharp},
  {"forw1", "alternate end of line character", 0, termForw1Charp},
  {"forw2", "alternate end of line character", 0, termForw2Charp},
  {"ayt", "alternate AYT character", 0, termAytCharp},
  {NULL, NULL, 0, NULL}
};

#if defined CRAY && !defined __STDC__
/* Work around compiler bug in pcc 4.1.5 */
void
_setlist_init ()
{
# ifndef KLUDGELINEMODE
#  define N 5
# else
#  define N 6
# endif
  Setlist[N + 0].charp = &termFlushChar;
  Setlist[N + 1].charp = &termIntChar;
  Setlist[N + 2].charp = &termQuitChar;
  Setlist[N + 3].charp = &termEofChar;
  Setlist[N + 6].charp = &termEraseChar;
  Setlist[N + 7].charp = &termKillChar;
  Setlist[N + 8].charp = &termLiteralNextChar;
  Setlist[N + 9].charp = &termSuspChar;
  Setlist[N + 10].charp = &termRprntChar;
  Setlist[N + 11].charp = &termWerasChar;
  Setlist[N + 12].charp = &termStartChar;
  Setlist[N + 13].charp = &termStopChar;
  Setlist[N + 14].charp = &termForw1Char;
  Setlist[N + 15].charp = &termForw2Char;
  Setlist[N + 16].charp = &termAytChar;
# undef	N
}
#endif /* defined(CRAY) && !defined(__STDC__) */

static struct setlist *
getset (char *name)
{
  return (struct setlist *)
    genget (name, (char **) Setlist, sizeof (struct setlist));
}

void
set_escape_char (char *s)
{
  if (rlogin != _POSIX_VDISABLE)
    {
      rlogin = (s && *s) ? special (s) : _POSIX_VDISABLE;
      printf ("Telnet rlogin escape character is '%s'.\n", control (rlogin));
    }
  else
    {
      escape = (s && *s) ? special (s) : _POSIX_VDISABLE;
      printf ("Telnet escape character is '%s'.\n", control (escape));
    }
}

static int
setcmd (int argc, char *argv[])
{
  int value;
  struct setlist *ct;
  struct togglelist *c;

  if (argc < 2 || argc > 3)
    {
      printf ("Format is 'set Name Value'\n'set ?' for help.\n");
      return 0;
    }
  if ((argc == 2) && (isprefix (argv[1], "?") || isprefix (argv[1], "help")))
    {
      for (ct = Setlist; ct->name; ct++)
	printf ("%-15s %s\n", ct->name, ct->help);
      printf ("\n");
      settogglehelp (1);
      printf ("%-15s %s\n", "?", "display help information");
      return 0;
    }

  ct = getset (argv[1]);
  if (ct == 0)
    {
      c = GETTOGGLE (argv[1]);
      if (c == 0)
	{
	  fprintf (stderr, "'%s': unknown argument ('set ?' for help).\n",
		   argv[1]);
	  return 0;
	}
      else if (Ambiguous (c))
	{
	  fprintf (stderr, "'%s': ambiguous argument ('set ?' for help).\n",
		   argv[1]);
	  return 0;
	}
      if (c->variable)
	{
	  if ((argc == 2) || (strcmp ("on", argv[2]) == 0))
	    *c->variable = 1;
	  else if (strcmp ("off", argv[2]) == 0)
	    *c->variable = 0;
	  else
	    {
	      printf
		("Format is 'set togglename [on|off]'\n'set ?' for help.\n");
	      return 0;
	    }
	  if (c->actionexplanation)
	    {
	      printf ("%s %s.\n", *c->variable ? "Will" : "Won't",
		      c->actionexplanation);
	    }
	}
      if (c->handler)
	(*c->handler) (1);
    }
  else if (argc != 3)
    {
      printf ("Format is 'set Name Value'\n'set ?' for help.\n");
      return 0;
    }
  else if (Ambiguous (ct))
    {
      fprintf (stderr, "'%s': ambiguous argument ('set ?' for help).\n",
	       argv[1]);
      return 0;
    }
  else if (ct->handler)
    {
      (*ct->handler) (argv[2]);
      printf ("%s set to \"%s\".\n", ct->name, (char *) ct->charp);
    }
  else
    {
      if (strcmp ("off", argv[2]))
	{
	  value = special (argv[2]);
	}
      else
	{
	  value = _POSIX_VDISABLE;
	}
      *(ct->charp) = (cc_t) value;
      printf ("%s character is '%s'.\n", ct->name, control (*(ct->charp)));
    }
  slc_check ();
  return 1;
}

static int
unsetcmd (int argc, char *argv[])
{
  struct setlist *ct;
  struct togglelist *c;
  register char *name;

  if (argc < 2)
    {
      fprintf (stderr,
	       "Need an argument to 'unset' command.  'unset ?' for help.\n");
      return 0;
    }
  if (isprefix (argv[1], "?") || isprefix (argv[1], "help"))
    {
      for (ct = Setlist; ct->name; ct++)
	printf ("%-15s %s\n", ct->name, ct->help);
      printf ("\n");
      settogglehelp (0);
      printf ("%-15s %s\n", "?", "display help information");
      return 0;
    }

  argc--;
  argv++;
  while (argc--)
    {
      name = *argv++;
      ct = getset (name);
      if (ct == 0)
	{
	  c = GETTOGGLE (name);
	  if (c == 0)
	    {
	      fprintf (stderr,
		       "'%s': unknown argument ('unset ?' for help).\n",
		       name);
	      return 0;
	    }
	  else if (Ambiguous (c))
	    {
	      fprintf (stderr,
		       "'%s': ambiguous argument ('unset ?' for help).\n",
		       name);
	      return 0;
	    }
	  if (c->variable)
	    {
	      *c->variable = 0;
	      if (c->actionexplanation)
		{
		  printf ("%s %s.\n", *c->variable ? "Will" : "Won't",
			  c->actionexplanation);
		}
	    }
	  if (c->handler)
	    (*c->handler) (0);
	}
      else if (Ambiguous (ct))
	{
	  fprintf (stderr, "'%s': ambiguous argument ('unset ?' for help).\n",
		   name);
	  return 0;
	}
      else if (ct->handler)
	{
	  (*ct->handler) (0);
	  printf ("%s reset to \"%s\".\n", ct->name, (char *) ct->charp);
	}
      else
	{
	  *(ct->charp) = _POSIX_VDISABLE;
	  printf ("%s character is '%s'.\n", ct->name,
		  control (*(ct->charp)));
	}
    }
  return 1;
}

/*
 * The following are the data structures and routines for the
 * 'mode' command.
 */
#ifdef	KLUDGELINEMODE
extern int kludgelinemode;

static int
dokludgemode (void)
{
  kludgelinemode = 1;
  send_wont (TELOPT_LINEMODE, 1);
  send_dont (TELOPT_SGA, 1);
  send_dont (TELOPT_ECHO, 1);
  return 0;
}
#endif

static int
dolinemode (void)
{
#ifdef	KLUDGELINEMODE
  if (kludgelinemode)
    send_dont (TELOPT_SGA, 1);
#endif
  send_will (TELOPT_LINEMODE, 1);
  send_dont (TELOPT_ECHO, 1);
  return 1;
}

static int
docharmode (void)
{
#ifdef	KLUDGELINEMODE
  if (kludgelinemode)
    send_do (TELOPT_SGA, 1);
  else
#endif
    send_wont (TELOPT_LINEMODE, 1);
  send_do (TELOPT_ECHO, 1);
  return 1;
}

static int
dolmmode (int bit, int on)
{
  unsigned char c;
  extern int linemode;

  if (my_want_state_is_wont (TELOPT_LINEMODE))
    {
      printf ("?Need to have LINEMODE option enabled first.\n");
      printf ("'mode ?' for help.\n");
      return 0;
    }

  if (on)
    c = (linemode | bit);
  else
    c = (linemode & ~bit);
  lm_mode (&c, 1, 1);
  return 1;
}

int
set_mode (int bit)
{
  return dolmmode (bit, 1);
}

int
clear_mode (int bit)
{
  return dolmmode (bit, 0);
}

struct modelist
{
  const char *name;		/* command name */
  const char *help;		/* help string */
  int (*handler) ();		/* routine which executes command */
  int needconnect;		/* Do we need to be connected to execute? */
  int arg1;
};

extern int modehelp (void);

static struct modelist ModeList[] = {
  {"character", "Disable LINEMODE option", docharmode, 1, 0},
#ifdef	KLUDGELINEMODE
  {"", "(or disable obsolete line-by-line mode)", NULL, 0, 0},
#endif
  {"line", "Enable LINEMODE option", dolinemode, 1, 0},
#ifdef	KLUDGELINEMODE
  {"", "(or enable obsolete line-by-line mode)", NULL, 0, 0},
#endif
  {"", "", NULL, 0, 0},
  {"", "These require the LINEMODE option to be enabled", NULL, 0, 0},
  {"isig", "Enable signal trapping", set_mode, 1, MODE_TRAPSIG},
  {"+isig", 0, set_mode, 1, MODE_TRAPSIG},
  {"-isig", "Disable signal trapping", clear_mode, 1, MODE_TRAPSIG},
  {"edit", "Enable character editing", set_mode, 1, MODE_EDIT},
  {"+edit", 0, set_mode, 1, MODE_EDIT},
  {"-edit", "Disable character editing", clear_mode, 1, MODE_EDIT},
  {"softtabs", "Enable tab expansion", set_mode, 1, MODE_SOFT_TAB},
  {"+softtabs", 0, set_mode, 1, MODE_SOFT_TAB},
  {"-softtabs", "Disable character editing", clear_mode, 1, MODE_SOFT_TAB},
  {"litecho", "Enable literal character echo", set_mode, 1, MODE_LIT_ECHO},
  {"+litecho", 0, set_mode, 1, MODE_LIT_ECHO},
  {"-litecho", "Disable literal character echo", clear_mode, 1,
   MODE_LIT_ECHO},
  {"help", 0, modehelp, 0, 0},
#ifdef	KLUDGELINEMODE
  {"kludgeline", 0, dokludgemode, 1, 0},
#endif
  {"", "", NULL, 0, 0},
  {"?", "Print help information", modehelp, 0, 0},
  {NULL, NULL, NULL, 0, 0},
};


int
modehelp (void)
{
  struct modelist *mt;

  printf ("format is:  'mode Mode', where 'Mode' is one of:\n\n");
  for (mt = ModeList; mt->name; mt++)
    {
      if (mt->help)
	{
	  if (*mt->help)
	    printf ("%-15s %s\n", mt->name, mt->help);
	  else
	    printf ("\n");
	}
    }
  return 0;
}

#define GETMODECMD(name) (struct modelist *) \
		genget(name, (char **) ModeList, sizeof(struct modelist))

static int
modecmd (int argc, char *argv[])
{
  struct modelist *mt;

  if (argc != 2)
    {
      printf ("'mode' command requires an argument\n");
      printf ("'mode ?' for help.\n");
    }
  else if ((mt = GETMODECMD (argv[1])) == 0)
    {
      fprintf (stderr, "Unknown mode '%s' ('mode ?' for help).\n", argv[1]);
    }
  else if (Ambiguous (mt))
    {
      fprintf (stderr, "Ambiguous mode '%s' ('mode ?' for help).\n", argv[1]);
    }
  else if (mt->needconnect && !connected)
    {
      printf ("?Need to be connected first.\n");
      printf ("'mode ?' for help.\n");
    }
  else if (mt->handler)
    {
      return (*mt->handler) (mt->arg1);
    }
  return 0;
}

/*
 * The following data structures and routines implement the
 * "display" command.
 */

static int
display (int argc, char *argv[])
{
  struct togglelist *tl;
  struct setlist *sl;

#define dotog(tl)	if (tl->variable && tl->actionexplanation) { \
			    if (*tl->variable) { \
				printf("will"); \
			    } else { \
				printf("won't"); \
			    } \
			    printf(" %s.\n", tl->actionexplanation); \
			}

#define doset(sl)   if (sl->name && *sl->name != ' ') { \
			if (sl->handler == 0) \
			    printf("%-15s [%s]\n", sl->name, control(*sl->charp)); \
			else \
			    printf("%-15s \"%s\"\n", sl->name, (char *)sl->charp); \
		    }

  if (argc == 1)
    {
      for (tl = Togglelist; tl->name; tl++)
	{
	  dotog (tl);
	}
      printf ("\n");
      for (sl = Setlist; sl->name; sl++)
	{
	  doset (sl);
	}
    }
  else
    {
      int i;

      for (i = 1; i < argc; i++)
	{
	  sl = getset (argv[i]);
	  tl = GETTOGGLE (argv[i]);
	  if (Ambiguous (sl) || Ambiguous (tl))
	    {
	      printf ("?Ambiguous argument '%s'.\n", argv[i]);
	      return 0;
	    }
	  else if (!sl && !tl)
	    {
	      printf ("?Unknown argument '%s'.\n", argv[i]);
	      return 0;
	    }
	  else
	    {
	      if (tl)
		{
		  dotog (tl);
		}
	      if (sl)
		{
		  doset (sl);
		}
	    }
	}
    }
/*@*/ optionstatus ();
#ifdef	ENCRYPTION
  EncryptStatus ();
#endif /* ENCRYPTION */
  return 1;
#undef	doset
#undef	dotog
}

/*
 * The following are the data structures, and many of the routines,
 * relating to command processing.
 */

/*
 * Set the escape character.
 */
static int
setescape (int argc, char *argv[])
{
  register char *arg;
  char buf[50];

  printf ("Deprecated usage - please use 'set escape%s%s' in the future.\n",
	  (argc > 2) ? " " : "", (argc > 2) ? argv[1] : "");
  if (argc > 2)
    arg = argv[1];
  else
    {
      printf ("new escape character: ");
      if (fgets (buf, sizeof (buf), stdin) == NULL)
	{
	  buf[0] = '\0';
	  printf ("\n");
	}
      arg = buf;
    }
  if (arg[0] != '\0')
    escape = arg[0];
  if (!In3270)
    {
      printf ("Escape character is '%s'.\n", control (escape));
    }
  fflush (stdout);
  return 1;
}

static int
togcrmod (void)
{
  crmod = !crmod;
  printf ("Deprecated usage - please use 'toggle crmod' in the future.\n");
  printf ("%s map carriage return on output.\n", crmod ? "Will" : "Won't");
  fflush (stdout);
  return 1;
}

int
suspend (void)
{
#ifdef	SIGTSTP
  setcommandmode ();
  {
    long oldrows, oldcols, newrows, newcols, err;

    err = (TerminalWindowSize (&oldrows, &oldcols) == 0) ? 1 : 0;
    kill (0, SIGTSTP);
    /*
     * If we didn't get the window size before the SUSPEND, but we
     * can get them now (?), then send the NAWS to make sure that
     * we are set up for the right window size.
     */
    if (TerminalWindowSize (&newrows, &newcols) && connected &&
	(err || ((oldrows != newrows) || (oldcols != newcols))))
      {
	sendnaws ();
      }
  }
  /* reget parameters in case they were changed */
  TerminalSaveState ();
  setconnmode (0);
#else
  printf ("Suspend is not supported.  Try the '!' command instead\n");
#endif
  return 1;
}

#if !defined TN3270
int
shell (int argc, char *argv[] _GL_UNUSED_PARAMETER)
{
  long oldrows, oldcols, newrows, newcols, err;

  setcommandmode ();

  err = (TerminalWindowSize (&oldrows, &oldcols) == 0) ? 1 : 0;
  switch (vfork ())
    {
    case -1:
      perror ("Fork failed\n");
      break;

    case 0:
      {
	/*
	 * Fire up the shell in the child.
	 */
	register char *shellp, *shellname;
# ifndef strrchr
	extern char *strrchr (const char *, int);
# endif

	shellp = getenv ("SHELL");
	if (shellp == NULL)
	  shellp = PATH_BSHELL;
	shellname = strrchr (shellp, '/');
	if (shellname == NULL)
	  shellname = shellp;
	else
	  shellname++;
	if (argc > 1)
	  execl (shellp, shellname, "-c", &saveline[1], NULL);
	else
	  execl (shellp, shellname, NULL);
	perror ("Execl");
	_exit (EXIT_FAILURE);
      }
    default:
      wait ((int *) 0);		/* Wait for the shell to complete */

      if (TerminalWindowSize (&newrows, &newcols) && connected &&
	  (err || ((oldrows != newrows) || (oldcols != newcols))))
	{
	  sendnaws ();
	}
      break;
    }
  return 1;
}
#else /* !defined(TN3270) */
extern int shell ();
#endif /* !defined(TN3270) */


/* int  argc;	 Number of arguments */
/* char *argv[]; arguments */
static int
bye (int argc, char *argv[])
{
  extern int resettermname;

  if (connected)
    {
      shutdown (net, 2);
      printf ("Connection closed.\n");
      NetClose (net);
      connected = 0;
      resettermname = 1;
#if defined AUTHENTICATION || defined ENCRYPTION
      auth_encrypt_connect (connected);
#endif /* defined(AUTHENTICATION) || defined(ENCRYPTION) */
      /* reset options */
      tninit ();
#if defined TN3270
      SetIn3270 ();		/* Get out of 3270 mode */
#endif /* defined(TN3270) */
    }
  if ((argc != 2) || (strcmp (argv[1], "fromquit") != 0))
    {
      longjmp (toplevel, 1);
    }
  return 1;
}

int
quit (void)
{
  call (bye, "bye", "fromquit", 0);
  Exit (0);
  return 0;
}

int
logoutcmd (void)
{
  send_do (TELOPT_LOGOUT, 1);
  netflush ();
  return 1;
}


/*
 * The SLC command.
 */

struct slclist
{
  const char *name;
  const char *help;
  void (*handler) ();
  int arg;
};

static void slc_help (void);

struct slclist SlcList[] = {
  {"export", "Use local special character definitions",
   slc_mode_export, 0},
  {"import", "Use remote special character definitions",
   slc_mode_import, 1},
  {"check", "Verify remote special character definitions",
   slc_mode_import, 0},
  {"help", 0, slc_help, 0},
  {"?", "Print help information", slc_help, 0},
  {NULL, NULL, NULL, 0},
};

static void
slc_help (void)
{
  struct slclist *c;

  for (c = SlcList; c->name; c++)
    {
      if (c->help)
	{
	  if (*c->help)
	    printf ("%-15s %s\n", c->name, c->help);
	  else
	    printf ("\n");
	}
    }
}

static struct slclist *
getslc (char *name)
{
  return (struct slclist *)
    genget (name, (char **) SlcList, sizeof (struct slclist));
}

static int
slccmd (int argc, char **argv)
{
  struct slclist *c;

  if (argc != 2)
    {
      fprintf (stderr,
	       "Need an argument to 'slc' command.  'slc ?' for help.\n");
      return 0;
    }
  c = getslc (argv[1]);
  if (c == 0)
    {
      fprintf (stderr, "'%s': unknown argument ('slc ?' for help).\n",
	       argv[1]);
      return 0;
    }
  if (Ambiguous (c))
    {
      fprintf (stderr, "'%s': ambiguous argument ('slc ?' for help).\n",
	       argv[1]);
      return 0;
    }
  (*c->handler) (c->arg);
  slcstate ();
  return 1;
}

/*
 * The ENVIRON command.
 */

struct envlist
{
  const char *name;
  const char *help;
  void (*handler) ();
  int narg;
};

extern struct env_lst *env_define (const char *, unsigned char *);
extern void
env_undefine (const char *),
env_export (const char *),
env_unexport (const char *), env_send (const char *),
#if defined OLD_ENVIRON && defined ENV_HACK
  env_varval (const char *),
#endif
  env_list (void);
static void env_help (void);

struct envlist EnvList[] = {
  {"define", "Define an environment variable",
   (void (*)()) env_define, 2},
  {"undefine", "Undefine an environment variable",
   env_undefine, 1},
  {"export", "Mark an environment variable for automatic export",
   env_export, 1},
  {"unexport", "Don't mark an environment variable for automatic export",
   env_unexport, 1},
  {"send", "Send an environment variable", env_send, 1},
  {"list", "List the current environment variables",
   env_list, 0},
#if defined OLD_ENVIRON && defined ENV_HACK
  {"varval", "Reverse VAR and VALUE (auto, right, wrong, status)",
   env_varval, 1},
#endif
  {"help", 0, env_help, 0},
  {"?", "Print help information", env_help, 0},
  {NULL, NULL, NULL, 0},
};

static void
env_help (void)
{
  struct envlist *c;

  for (c = EnvList; c->name; c++)
    {
      if (c->help)
	{
	  if (*c->help)
	    printf ("%-15s %s\n", c->name, c->help);
	  else
	    printf ("\n");
	}
    }
}

static struct envlist *
getenvcmd (char *name)
{
  return (struct envlist *)
    genget (name, (char **) EnvList, sizeof (struct envlist));
}

int
env_cmd (int argc, char *argv[])
{
  struct envlist *c;

  if (argc < 2)
    {
      fprintf (stderr,
	       "Need an argument to 'environ' command.  'environ ?' for help.\n");
      return 0;
    }
  c = getenvcmd (argv[1]);
  if (c == 0)
    {
      fprintf (stderr, "'%s': unknown argument ('environ ?' for help).\n",
	       argv[1]);
      return 0;
    }
  if (Ambiguous (c))
    {
      fprintf (stderr, "'%s': ambiguous argument ('environ ?' for help).\n",
	       argv[1]);
      return 0;
    }
  if (c->narg + 2 != argc)
    {
      fprintf (stderr,
	       "Need %s%d argument%s to 'environ %s' command.  'environ ?' for help.\n",
	       c->narg < argc + 2 ? "only " : "",
	       c->narg, c->narg == 1 ? "" : "s", c->name);
      return 0;
    }
  (*c->handler) (argv[2], argv[3]);
  return 1;
}

struct env_lst
{
  struct env_lst *next;		/* pointer to next structure */
  struct env_lst *prev;		/* pointer to previous structure */
  unsigned char *var;		/* pointer to variable name */
  unsigned char *value;		/* pointer to variable value */
  int export;			/* 1 -> export with default list of variables */
  int welldefined;		/* A well defined variable */
};

struct env_lst envlisthead;

struct env_lst *
env_find (const char *var)
{
  register struct env_lst *ep;

  for (ep = envlisthead.next; ep; ep = ep->next)
    {
      if (strcmp ((char *) ep->var, var) == 0)
	return (ep);
    }
  return (NULL);
}

void
env_init (void)
{
  extern char **environ;
  register char **epp, *cp;
  register struct env_lst *ep;
#ifndef strchr
  extern char *strchr (const char *, int);
#endif

  for (epp = environ; *epp; epp++)
    {
      cp = strchr (*epp, '=');
      if (cp)
	{
	  *cp = '\0';
	  ep = env_define (*epp, (unsigned char *) cp + 1);
	  ep->export = 0;
	  *cp = '=';
	}
    }
  /*
   * Special case for DISPLAY variable.  If it is ":0.0" or
   * "unix:0.0", we have to get rid of "unix" and insert our
   * hostname.
   */
  if ((ep = env_find ("DISPLAY"))
      && ((*ep->value == ':')
	  || (strncmp ((char *) ep->value, "unix:", 5) == 0)))
    {
      char *hostname = localhost ();
      char *cp2 = strchr ((char *) ep->value, ':');

      cp = xmalloc (strlen (hostname) + strlen (cp2) + 1);
      sprintf (cp, "%s%s", hostname, cp2);

      free (ep->value);
      ep->value = (unsigned char *) cp;

      free (hostname);
    }
  /*
   * If USER is not defined, but LOGNAME is, then add
   * USER with the value from LOGNAME.  By default, we
   * don't export the USER variable.
   */
  if ((env_find ("USER") == NULL) && (ep = env_find ("LOGNAME")))
    {
      env_define ("USER", ep->value);
      env_unexport ("USER");
    }
  env_export ("DISPLAY");
  env_export ("PRINTER");
}

struct env_lst *
env_define (const char *var, unsigned char *value)
{
  register struct env_lst *ep = env_find (var);
  if (ep)
    {
      free (ep->var);
      free (ep->value);
    }
  else
    {
      ep = (struct env_lst *) xmalloc (sizeof (struct env_lst));
      ep->next = envlisthead.next;
      envlisthead.next = ep;
      ep->prev = &envlisthead;
      if (ep->next)
	ep->next->prev = ep;
    }
  ep->welldefined = opt_welldefined ((char *)var);
  ep->export = 1;
  ep->var = (unsigned char *) strdup ((char *) var);
  ep->value = (unsigned char *) strdup ((char *) value);
  return (ep);
}

void
env_undefine (const char *var)
{
  register struct env_lst *ep = env_find (var);
  if (ep)
    {
      ep->prev->next = ep->next;
      if (ep->next)
	ep->next->prev = ep->prev;
      free (ep->var);
      free (ep->value);
      free (ep);
    }
}

void
env_export (const char *var)
{
  register struct env_lst *ep = env_find (var);
  if (ep)
    ep->export = 1;
}

void
env_unexport (const char *var)
{
  register struct env_lst *ep = env_find (var);
  if (ep)
    ep->export = 0;
}

void
env_send (const char *var)
{
  register struct env_lst *ep;

  if (my_state_is_wont (TELOPT_NEW_ENVIRON)
#ifdef	OLD_ENVIRON
      && my_state_is_wont (TELOPT_OLD_ENVIRON)
#endif
    )
    {
      fprintf (stderr,
	       "Cannot send '%s': Telnet ENVIRON option not enabled\n", var);
      return;
    }
  ep = env_find (var);
  if (ep == 0)
    {
      fprintf (stderr, "Cannot send '%s': variable not defined\n", var);
      return;
    }
  env_opt_start_info ();
  env_opt_add (ep->var);
  env_opt_end (0);
}

void
env_list (void)
{
  register struct env_lst *ep;

  for (ep = envlisthead.next; ep; ep = ep->next)
    {
      printf ("%c %-20s %s\n", ep->export ? '*' : ' ', ep->var, ep->value);
    }
}

unsigned char *
env_default (int init, int welldefined)
{
  static struct env_lst *nep = NULL;

  if (init)
    {
      nep = &envlisthead;
      return NULL;
    }
  if (nep)
    {
      while ((nep = nep->next))
	{
	  if (nep->export && (nep->welldefined == welldefined))
	    return (nep->var);
	}
    }
  return (NULL);
}

unsigned char *
env_getvalue (const char *var)
{
  register struct env_lst *ep  = env_find (var);
  if (ep)
    return (ep->value);
  return (NULL);
}

#if defined OLD_ENVIRON && defined ENV_HACK
void
env_varval (const char *what)
{
  extern int old_env_var, old_env_value, env_auto;
  int len = strlen (what);

  if (len == 0)
    goto unknown;

  if (strncasecmp ((char *) what, "status", len) == 0)
    {
      if (env_auto)
	printf ("%s%s", "VAR and VALUE are/will be ",
		"determined automatically\n");
      if (old_env_var == OLD_ENV_VAR)
	printf ("VAR and VALUE set to correct definitions\n");
      else
	printf ("VAR and VALUE definitions are reversed\n");
    }
  else if (strncasecmp ((char *) what, "auto", len) == 0)
    {
      env_auto = 1;
      old_env_var = OLD_ENV_VALUE;
      old_env_value = OLD_ENV_VAR;
    }
  else if (strncasecmp ((char *) what, "right", len) == 0)
    {
      env_auto = 0;
      old_env_var = OLD_ENV_VAR;
      old_env_value = OLD_ENV_VALUE;
    }
  else if (strncasecmp ((char *) what, "wrong", len) == 0)
    {
      env_auto = 0;
      old_env_var = OLD_ENV_VALUE;
      old_env_value = OLD_ENV_VAR;
    }
  else
    {
    unknown:
      printf
	("Unknown \"varval\" command. (\"auto\", \"right\", \"wrong\", \"status\")\n");
    }
}
#endif

#if defined AUTHENTICATION
/*
 * The AUTHENTICATE command.
 */

struct authlist
{
  const char *name;
  char *help;
  int (*handler) ();
  int narg;
};

extern int auth_enable (char *), auth_disable (char *), auth_status (void);
static int auth_help (void);

struct authlist AuthList[] = {
  {"status", "Display current status of authentication information",
   auth_status, 0},
  {"disable", "Disable an authentication type ('auth disable ?' for more)",
   auth_disable, 1},
  {"enable", "Enable an authentication type ('auth enable ?' for more)",
   auth_enable, 1},
  {"help", 0, auth_help, 0},
  {"?", "Print help information", auth_help, 0},
  {NULL, NULL, NULL, 0},
};

static int
auth_help ()
{
  struct authlist *c;

  for (c = AuthList; c->name; c++)
    {
      if (c->help)
	{
	  if (*c->help)
	    printf ("%-15s %s\n", c->name, c->help);
	  else
	    printf ("\n");
	}
    }
  return 0;
}

int
auth_cmd (int argc, char *argv[])
{
  struct authlist *c;

  if (argc < 2)
    {
      fprintf (stderr,
	       "Need an argument to 'auth' command.  'auth ?' for help.\n");
      return 0;
    }

  c = (struct authlist *)
    genget (argv[1], (char **) AuthList, sizeof (struct authlist));
  if (c == 0)
    {
      fprintf (stderr, "'%s': unknown argument ('auth ?' for help).\n",
	       argv[1]);
      return 0;
    }
  if (Ambiguous (c))
    {
      fprintf (stderr, "'%s': ambiguous argument ('auth ?' for help).\n",
	       argv[1]);
      return 0;
    }
  if (c->narg + 2 != argc)
    {
      fprintf (stderr,
	       "Need %s%d argument%s to 'auth %s' command.  'auth ?' for help.\n",
	       c->narg < argc + 2 ? "only " : "",
	       c->narg, c->narg == 1 ? "" : "s", c->name);
      return 0;
    }
  return ((*c->handler) (argv[2], argv[3]));
}
#endif

#ifdef	ENCRYPTION
/*
 * The ENCRYPT command.
 */

struct encryptlist
{
  const char *name;
  const char *help;
  int (*handler) ();
  int needconnect;
  int minarg;
  int maxarg;
};

extern int
EncryptEnable (char *, char *),
EncryptDisable (char *, char *),
EncryptType (char *, char *),
EncryptStart (char *),
EncryptStartInput (void),
EncryptStartOutput (void),
EncryptStop (char *),
EncryptStopInput (void), EncryptStopOutput (void), EncryptStatus (void);
static int EncryptHelp (void);

struct encryptlist EncryptList[] = {
  {"enable", "Enable encryption. ('encrypt enable ?' for more)",
   EncryptEnable, 1, 1, 2},
  {"disable", "Disable encryption. ('encrypt enable ?' for more)",
   EncryptDisable, 0, 1, 2},
  {"type", "Set encryption type. ('encrypt type ?' for more)",
   EncryptType, 0, 1, 1},
  {"start", "Start encryption. ('encrypt start ?' for more)",
   EncryptStart, 1, 0, 1},
  {"stop", "Stop encryption. ('encrypt stop ?' for more)",
   EncryptStop, 1, 0, 1},
  {"input", "Start encrypting the input stream",
   EncryptStartInput, 1, 0, 0},
  {"-input", "Stop encrypting the input stream",
   EncryptStopInput, 1, 0, 0},
  {"output", "Start encrypting the output stream",
   EncryptStartOutput, 1, 0, 0},
  {"-output", "Stop encrypting the output stream",
   EncryptStopOutput, 1, 0, 0},

  {"status", "Display current status of authentication information",
   EncryptStatus, 0, 0, 0},
  {"help", 0, EncryptHelp, 0, 0, 0},
  {"?", "Print help information", EncryptHelp, 0, 0, 0},
  {NULL, NULL, NULL, 0, 0, 0},
};

static int
EncryptHelp ()
{
  struct encryptlist *c;

  for (c = EncryptList; c->name; c++)
    {
      if (c->help)
	{
	  if (*c->help)
	    printf ("%-15s %s\n", c->name, c->help);
	  else
	    printf ("\n");
	}
    }
  return 0;
}

int
encrypt_cmd (int argc, char *argv[])
{
  struct encryptlist *c;

  if (argc < 2)
    {
      fprintf (stderr,
	       "Need an argument to 'encrypt' command.  'encrypt ?' for help.\n");
      return 0;
    }

  c = (struct encryptlist *)
    genget (argv[1], (char **) EncryptList, sizeof (struct encryptlist));
  if (c == 0)
    {
      fprintf (stderr, "'%s': unknown argument ('encrypt ?' for help).\n",
	       argv[1]);
      return 0;
    }
  if (Ambiguous (c))
    {
      fprintf (stderr, "'%s': ambiguous argument ('encrypt ?' for help).\n",
	       argv[1]);
      return 0;
    }
  argc -= 2;
  if (argc < c->minarg || argc > c->maxarg)
    {
      if (c->minarg == c->maxarg)
	{
	  fprintf (stderr, "Need %s%d argument%s ",
		   c->minarg < argc ? "only " : "", c->minarg,
		   c->minarg == 1 ? "" : "s");
	}
      else
	{
	  fprintf (stderr, "Need %s%d-%d arguments ",
		   c->maxarg < argc ? "only " : "", c->minarg, c->maxarg);
	}
      fprintf (stderr, "to 'encrypt %s' command.  'encrypt ?' for help.\n",
	       c->name);
      return 0;
    }
  if (c->needconnect && !connected)
    {
      if (!(argc && (isprefix (argv[2], "help") || isprefix (argv[2], "?"))))
	{
	  printf ("?Need to be connected first.\n");
	  return 0;
	}
    }
  return ((*c->handler) (argc > 0 ? argv[2] : 0,
			 argc > 1 ? argv[3] : 0, argc > 2 ? argv[4] : 0));
}
#endif /* ENCRYPTION */

#if (defined unix || defined __unix || defined __unix__) && defined TN3270
static void
filestuff (int fd)
{
  int res;

# ifdef	F_GETOWN
  setconnmode (0);
  res = fcntl (fd, F_GETOWN, 0);
  setcommandmode ();

  if (res == -1)
    {
      perror ("fcntl");
      return;
    }
  printf ("\tOwner is %d.\n", res);
# endif

  setconnmode (0);
  res = fcntl (fd, F_GETFL, 0);
  setcommandmode ();

  if (res == -1)
    {
      perror ("fcntl");
      return;
    }
}
#endif /* (unix || __unix || __unix__) && TN3270 */

/*
 * Print status about the connection.
 */
static int
status (int argc, char *argv[])
{
  if (connected)
    {
      printf ("Connected to %s.\n", hostname);
      if ((argc < 2) || strcmp (argv[1], "notmuch"))
	{
	  int mode = getconnmode ();

	  if (my_want_state_is_will (TELOPT_LINEMODE))
	    {
	      printf ("Operating with LINEMODE option\n");
	      printf ("%s line editing\n",
		      (mode & MODE_EDIT) ? "Local" : "No");
	      printf ("%s catching of signals\n",
		      (mode & MODE_TRAPSIG) ? "Local" : "No");
	      slcstate ();
#ifdef	KLUDGELINEMODE
	    }
	  else if (kludgelinemode && my_want_state_is_dont (TELOPT_SGA))
	    {
	      printf ("Operating in obsolete linemode\n");
#endif
	    }
	  else
	    {
	      printf ("Operating in single character mode\n");
	      if (localchars)
		printf ("Catching signals locally\n");
	    }
	  printf ("%s character echo\n",
		  (mode & MODE_ECHO) ? "Local" : "Remote");
	  if (my_want_state_is_will (TELOPT_LFLOW))
	    printf ("%s flow control\n", (mode & MODE_FLOW) ? "Local" : "No");
#ifdef	ENCRYPTION
	  encrypt_display ();
#endif /* ENCRYPTION */
	}
    }
  else
    {
      printf ("No connection.\n");
    }
#if !defined TN3270
  printf ("Escape character is '%s'.\n", control (escape));
  fflush (stdout);
#else /* !defined(TN3270) */
  if ((!In3270) && ((argc < 2) || strcmp (argv[1], "notmuch")))
    {
      printf ("Escape character is '%s'.\n", control (escape));
    }
# if defined unix || defined __unix || defined __unix__
  if ((argc >= 2) && !strcmp (argv[1], "everything"))
    {
      printf ("SIGIO received %d time%s.\n",
	      sigiocount, (sigiocount == 1) ? "" : "s");
      if (In3270)
	{
	  printf ("Process ID %d, process group %d.\n",
		  getpid (), getpgrp (getpid ()));
	  printf ("Terminal input:\n");
	  filestuff (tin);
	  printf ("Terminal output:\n");
	  filestuff (tout);
	  printf ("Network socket:\n");
	  filestuff (net);
	}
    }
  if (In3270 && transcom)
    {
      printf ("Transparent mode command is '%s'.\n", transcom);
    }
# endif /* unix || __unix || __unix__ */
  fflush (stdout);
  if (In3270)
    {
      return 0;
    }
#endif /* defined(TN3270) */
  return 1;
}

#ifdef	SIGINFO
/*
 * Function that gets called when SIGINFO is received.
 */
int
ayt_status ()
{
  call (status, "status", "notmuch", 0);
  return 1;	/* not used */
}
#endif /* SIGINFO */

static void cmdrc (char *m1, char *m2);

int
tn (int argc, char *argv[])
{
#ifdef IPV6
  struct addrinfo *result, *aip, hints;
#else
  struct hostent *host = 0;
  struct sockaddr_in sin;
  struct servent *sp = 0;
  in_addr_t temp;
#endif
  const int on = 1;
  int err;
  char *cmd, *hostp = 0, *portp = 0, *user = 0;
#if defined HAVE_IDN || defined HAVE_IDN2
  char *hosttmp = 0;
#endif

#ifdef IPV6
  memset (&hints, 0, sizeof (hints));
#else
  /* clear the socket address prior to use */
  memset ((char *) &sin, 0, sizeof (sin));
#endif

  if (connected)
    {
      printf ("?Already connected to %s\n", hostname);
      return 0;
    }
  if (argc < 2)
    {
      strcpy (line, "open ");
      printf ("(to) ");
      if (fgets (&line[strlen (line)],
		 sizeof (line) - strlen (line), stdin))
	{
	  makeargv ();
	  argc = margc;
	  argv = margv;
	}
      else
	printf ("?Name of host was not understood.\n");
    }
  cmd = *argv;
  --argc;
  ++argv;
  while (argc)
    {
      if (strcmp (*argv, "help") == 0 || isprefix (*argv, "?"))
	goto usage;
      if (strcmp (*argv, "-l") == 0)
	{
	  --argc;
	  ++argv;
	  if (argc == 0)
	    goto usage;
	  user = *argv++;
	  --argc;
	  continue;
	}
      if (strcmp (*argv, "-a") == 0)
	{
	  --argc;
	  ++argv;
	  autologin = 1;
	  continue;
	}
      if (strcmp (*argv, "-6") == 0)
	{
	  --argc;
	  ++argv;
#ifdef IPV6
	  hints.ai_family = AF_INET6;
#else
	  puts ("IPv6 isn't supported");
#endif
	  continue;
	}
      if (strcmp (*argv, "-4") == 0)
	{
	  --argc;
	  ++argv;
#ifdef IPV6
	  hints.ai_family = AF_INET;
#endif
	  continue;
	}
      if (hostp == 0)
	{
	  hostp = *argv++;
	  --argc;
	  continue;
	}
      if (portp == 0)
	{
	  portp = *argv++;
	  --argc;
	  continue;
	}
    usage:
      printf ("usage: %s [-4] [-6] [-l user] [-a] host-name [port]\n", cmd);
      return 0;
    }
  if (hostp == 0)
    goto usage;

  if (!portp)
    {
      portp = "telnet";
      telnetport = 1;
    }
  else
    {
      if (*portp == '-')
	{
	  portp++;
	  telnetport = 1;
	}
      else
	telnetport = 0;
      if (*portp >= '0' && *portp <= '9')
	{
	  long long int val;
	  char *endp;

	  val = strtoll (portp, &endp, 10);

	  if ((errno == ERANGE && (val == LLONG_MAX || val == LLONG_MIN))
	      || (*endp == '\0' && (val < 1 || val > 65535)))
	    {
	      printf ("Port number %s is out of range.\n", portp);
	      return 0;
	    }
	  else if (*endp)
	    {
	      printf ("Invalid port name '%s'.\n", portp);
	      return 0;
	    }
	}
    }

  free (hostname);
  hostname = strdup (hostp);
  if (!hostname)
    {
      printf ("Can't allocate memory to copy hostname\n");
      return 0;
    }

#if defined AUTHENTICATION || defined ENCRYPTION
  {
    /* Extract instance name of server, eliminating
     * the Kerberos principal prefix.
     */
    char *p = strchr (hostp, '/');

    if (p)
      hostp = ++p;
  }
#endif /* AUTHENTICATION || ENCRYPTION */

#if defined HAVE_IDN || defined HAVE_IDN2
  err = idna_to_ascii_lz (hostp, &hosttmp, 0);
  if (err)
    {
      printf ("Server lookup failure:  %s:%s, %s\n",
	      hostp, portp, idna_strerror (err));
      return 0;
    }
  hostp = hosttmp;
#endif /* !HAVE_IDN && !HAVE_IDN2 */

#ifdef IPV6
  hints.ai_socktype = SOCK_STREAM;
# ifdef AI_IDN
  hints.ai_flags = AI_IDN;
# endif

  err = getaddrinfo (hostp, portp, &hints, &result);
  if (err)
    {
      const char *errmsg;

      if (err == EAI_SYSTEM)
	errmsg = strerror (errno);
      else
	errmsg = gai_strerror (err);

      printf ("Server lookup failure:  %s:%s, %s\n", hostp, portp, errmsg);
      return 0;
    }

  aip = result;

  do
    {
      char buf[256];

      err = getnameinfo (aip->ai_addr, aip->ai_addrlen, buf, sizeof (buf),
			 NULL, 0, NI_NUMERICHOST);
      if (err)
	{
	  /* I don't know how thing can happen, but we just handle it.  */
	  const char *errmsg;

	  if (err == EAI_SYSTEM)
	    errmsg = strerror (errno);
	  else
	    errmsg = gai_strerror (err);

	  printf ("getnameinfo error: %s\n", errmsg);
	  return 0;
	}

      printf ("Trying %s...\n", buf);

      net = socket (aip->ai_family, SOCK_STREAM, 0);
      if (net < 0)
	{
	  perror ("telnet: socket");
	  return 0;
	}

      if (debug)
	{
	  err = setsockopt (net, SOL_SOCKET, SO_DEBUG, &on, sizeof (on));
	  if (err < 0)
	    perror ("setsockopt (SO_DEBUG)");
	}

      err = connect (net, (struct sockaddr *) aip->ai_addr, aip->ai_addrlen);
      if (err < 0)
	{
	  if (aip->ai_next)
	    {
	      perror ("Connection failed");
	      aip = aip->ai_next;
	      close (net);
	      continue;
	    }

	  perror ("telnet: Unable to connect to remote host");
	  return 0;
	}

      connected++;
# if defined AUTHENTICATION || defined ENCRYPTION
      auth_encrypt_connect (connected);
# endif	/* defined(AUTHENTICATION) || defined(ENCRYPTION) */
    }
  while (!connected);

  freeaddrinfo (result);
#else /* !IPV6 */
  temp = inet_addr (hostp);
  if (temp != (in_addr_t) - 1)
    {
      sin.sin_addr.s_addr = temp;
      sin.sin_family = AF_INET;
    }
  else
    {
      host = gethostbyname (hostp);
      if (host)
	{
	  sin.sin_family = host->h_addrtype;
	  memmove (&sin.sin_addr, host->h_addr_list[0], host->h_length);
	}
      else
	{
	  printf ("Can't lookup hostname %s\n", hostp);
	  return 0;
	}
    }

  sin.sin_port = atoi (portp);
  if (sin.sin_port == 0)
    {
      sp = getservbyname (portp, "tcp");
      if (sp == 0)
	{
	  printf ("tcp/%s: unknown service\n", portp);
	  return 0;
	}
      sin.sin_port = sp->s_port;
    }
  else
    sin.sin_port = htons (sin.sin_port);

  printf ("Trying %s...\n", inet_ntoa (sin.sin_addr));
  do
    {
      net = socket (AF_INET, SOCK_STREAM, 0);
      if (net < 0)
	{
	  perror ("telnet: socket");
	  return 0;
	}
# if defined IPPROTO_IP && defined IP_TOS
      {
#  ifdef IPTOS_LOWDELAY
	const int tos = IPTOS_LOWDELAY;
#  else
	const int tos = 0x10;
#  endif

	err = setsockopt (net, IPPROTO_IP, IP_TOS,
			  (char *) &tos, sizeof (tos));
	if (err < 0 && errno != ENOPROTOOPT)
	  perror ("telnet: setsockopt (IP_TOS) (ignored)");
      }
# endif	/* defined(IPPROTO_IP) && defined(IP_TOS) */

      if (debug
	  && setsockopt (net, SOL_SOCKET, SO_DEBUG, &on, sizeof (on)) < 0)
	perror ("setsockopt (SO_DEBUG)");

      if (connect (net, (struct sockaddr *) &sin, sizeof (sin)) < 0)
	{
	  if (host && host->h_addr_list[1])
	    {
	      int oerrno = errno;

	      fprintf (stderr, "telnet: connect to address %s: ",
		       inet_ntoa (sin.sin_addr));
	      errno = oerrno;
	      perror ((char *) 0);
	      host->h_addr_list++;
	      memmove ((caddr_t) & sin.sin_addr,
		       host->h_addr_list[0], host->h_length);
	      close (net);
	      continue;
	    }
	  perror ("telnet: Unable to connect to remote host");
	  return 0;
	}
      connected++;
# if defined AUTHENTICATION || defined ENCRYPTION
      auth_encrypt_connect (connected);
# endif	/* defined(AUTHENTICATION) || defined(ENCRYPTION) */
    }
  while (connected == 0);
#endif /* !IPV6 */

  cmdrc (hostp, hostname);
#if defined HAVE_IDN || defined HAVE_IDN2
  /* Last use of HOSTP, alias HOSTTMP.  */
  free (hosttmp);
#endif

  if (autologin && user == NULL)
    {
      struct passwd *pw;

      user = getenv ("USER");
      if (user == NULL || ((pw = getpwnam (user)) && pw->pw_uid != getuid ()))
	{
	  if ((pw = getpwuid (getuid ())))
	    user = pw->pw_name;
	  else
	    user = NULL;
	}
    }
  if (user)
    {
      env_define ("USER", (unsigned char *) user);
      env_export ("USER");
    }
  call (status, "status", "notmuch", 0);
  err = 0;
  if (setjmp (peerdied) == 0)
    telnet (user);
  else
    err = 1;

  close (net);
  ExitString ("Connection closed by foreign host.\n", err);
  /* NOT REACHED */
  return 0;
}

#define HELPINDENT (sizeof ("connect"))

static char
  openhelp[] = "connect to a site",
  closehelp[] = "close current connection",
  logouthelp[] = "forcibly logout remote user and close the connection",
  quithelp[] = "exit telnet",
  statushelp[] = "print status information",
  helphelp[] = "print help information",
  sendhelp[] = "transmit special characters ('send ?' for more)",
  sethelp[] = "set operating parameters ('set ?' for more)",
  unsethelp[] = "unset operating parameters ('unset ?' for more)",
  togglestring[] = "toggle operating parameters ('toggle ?' for more)",
  slchelp[] = "change state of special characters ('slc ?' for more)",
  displayhelp[] = "display operating parameters",
#if defined TN3270 && (defined unix || defined __unix || defined __unix__)
  transcomhelp[] = "specify Unix command for transparent mode pipe",
#endif /* TN3270 && (unix || __unix || __unix__) */
#if defined AUTHENTICATION
  authhelp[] = "turn on (off) authentication ('auth ?' for more)",
#endif
#ifdef	ENCRYPTION
  encrypthelp[] = "turn on (off) encryption ('encrypt ?' for more)",
#endif
  /* ENCRYPTION */
#if defined unix || defined __unix || defined __unix__
  zhelp[] = "suspend telnet",
#endif /* unix || __unix || __unix__ */
  shellhelp[] = "invoke a subshell",
  envhelp[] = "change environment variables ('environ ?' for more)",
  modestring[] = "try to enter line or character mode ('mode ?' for more)";

static int help (int argc, char **argv);

static Command cmdtab[] = {
  {"close", closehelp, bye, 1},
  {"logout", logouthelp, logoutcmd, 1},
  {"display", displayhelp, display, 0},
  {"mode", modestring, modecmd, 0},
  {"open", openhelp, tn, 0},
  {"quit", quithelp, quit, 0},
  {"send", sendhelp, sendcmd, 0},
  {"set", sethelp, setcmd, 0},
  {"unset", unsethelp, unsetcmd, 0},
  {"status", statushelp, status, 0},
  {"toggle", togglestring, toggle, 0},
  {"slc", slchelp, slccmd, 0},
#if defined TN3270 && (defined unix || defined __unix || defined __unix__)
  {"transcom", transcomhelp, settranscom, 0},
#endif /* TN3270 && (unix || __unix || __unix__) */
#if defined AUTHENTICATION
  {"auth", authhelp, auth_cmd, 0},
#endif
#ifdef	ENCRYPTION
  {"encrypt", encrypthelp, encrypt_cmd, 0},
#endif /* ENCRYPTION */
#if defined unix || defined __unix || defined __unix__
  {"z", zhelp, suspend, 0},
#endif /* unix || __unix || __unix__ */
#if defined TN3270
  {"!", shellhelp, shell, 1},
#else
  {"!", shellhelp, shell, 0},
#endif
  {"environ", envhelp, env_cmd, 0},
  {"?", helphelp, help, 0},
  {NULL, NULL, NULL, 0}
};

static char crmodhelp[] = "deprecated command -- use 'toggle crmod' instead";
static char escapehelp[] = "deprecated command -- use 'set escape' instead";

static Command cmdtab2[] = {
  {"help", 0, help, 0},
  {"escape", escapehelp, setescape, 0},
  {"crmod", crmodhelp, togcrmod, 0},
  {NULL, NULL, NULL, 0}
};


/*
 * Call routine with argc, argv set from args (terminated by 0).
 */

static int
call (intrtn_t routine, ...)
{
  va_list ap;
  char *args[100];
  int argno = 0;

  va_start (ap, routine);
  while ((args[argno++] = va_arg (ap, char *)) != 0)
    {
      ;
    }
  va_end (ap);
  return (*routine) (argno - 1, args);
}


static Command *
getcmd (char *name)
{
  Command *cm;

  if ((cm = (Command *) genget (name, (char **) cmdtab, sizeof (Command))))
    return cm;
  return (Command *) genget (name, (char **) cmdtab2, sizeof (Command));
}

void
command (int top, char *tbuf, int cnt)
{
  register Command *c;

  setcommandmode ();
  if (!top)
    {
      putchar ('\n');
    }
#if defined unix || defined __unix || defined __unix__
  else
    {
      signal (SIGINT, SIG_DFL);
      signal (SIGQUIT, SIG_DFL);
    }
#endif /* unix || __unix || __unix__ */
  for (;;)
    {
      if (rlogin == _POSIX_VDISABLE)
	printf ("%s> ", prompt);
      if (tbuf)
	{
	  register char *cp;
	  cp = line;
	  while (cnt > 0 && (*cp++ = *tbuf++) != '\n')
	    cnt--;
	  tbuf = 0;
	  if (cp == line || *--cp != '\n' || cp == line)
	    goto getline;
	  *cp = '\0';
	  if (rlogin == _POSIX_VDISABLE)
	    printf ("%s\n", line);
	}
      else
	{
	getline:
	  if (rlogin != _POSIX_VDISABLE)
	    printf ("%s> ", prompt);
	  if (fgets (line, sizeof (line), stdin) == NULL)
	    {
	      if (feof (stdin) || ferror (stdin))
		{
		  printf ("\n");
		  quit ();
		}
	      break;
	    }
	}
      if (line[0] == 0)
	break;
      makeargv ();
      if (margv[0] == 0)
	{
	  break;
	}
      c = getcmd (margv[0]);
      if (Ambiguous (c))
	{
	  printf ("?Ambiguous command\n");
	  continue;
	}
      if (c == 0)
	{
	  printf ("?Invalid command\n");
	  continue;
	}
      if (c->needconnect && !connected)
	{
	  printf ("?Need to be connected first.\n");
	  continue;
	}
      if ((*c->handler) (margc, margv))
	{
	  break;
	}
    }
  if (!top)
    {
      if (!connected)
	{
	  longjmp (toplevel, 1);
	}
#if defined TN3270
      if (shell_active == 0)
	{
	  setconnmode (0);
	}
#else /* defined(TN3270) */
      setconnmode (0);
#endif /* defined(TN3270) */
    }
}

/*
 * Help command.
 */
static int
help (int argc, char *argv[])
{
  register Command *c;

  if (argc == 1)
    {
      printf ("Commands may be abbreviated.  Commands are:\n\n");
      for (c = cmdtab; c->name; c++)
	if (c->help)
	  {
	    printf ("%-*s\t%s\n", (int) HELPINDENT, c->name, c->help);
	  }
      return 0;
    }
  while (--argc > 0)
    {
      register char *arg;
      arg = *++argv;
      c = getcmd (arg);
      if (Ambiguous (c))
	printf ("?Ambiguous help command %s\n", arg);
      else if (c == (Command *) 0)
	printf ("?Invalid help command %s\n", arg);
      else
	printf ("%s\n", c->help);
    }
  return 0;
}

static char *rcname = 0;

static void
cmdrc (char *m1, char *m2)
{
  register Command *c;
  FILE *rcfile;
  int gotmachine = 0;
  int l1 = strlen (m1);
  int l2 = strlen (m2);

  if (skiprc)
    return;

  if (rcname == 0)
    {
      const char *home = getenv ("HOME");
      if (home)
        rcname = xasprintf ("%s/.telnetrc", home);
      else
        rcname = xstrdup ("/.telnetrc");
    }

  if ((rcfile = fopen (rcname, "r")) == 0)
    {
      return;
    }

  for (;;)
    {
      if (fgets (line, sizeof (line), rcfile) == NULL)
	break;
      if (line[0] == 0)
	break;
      if (line[0] == '#')
	continue;
      if (gotmachine)
	{
	  if (!isspace (line[0]))
	    gotmachine = 0;
	}
      if (gotmachine == 0)
	{
	  if (isspace (line[0]))
	    continue;
	  if (strncasecmp (line, m1, l1) == 0)
	    memmove (line, &line[l1], strlen(&line[l1]) + 1);
	  else if (strncasecmp (line, m2, l2) == 0)
	    memmove (line, &line[l2], strlen(&line[l2]) + 1);
	  else if (strncasecmp (line, "DEFAULT", 7) == 0)
	    memmove (line, &line[7], strlen(&line[7]) + 1);
	  else
	    continue;
	  if (line[0] != ' ' && line[0] != '\t' && line[0] != '\n')
	    continue;
	  gotmachine = 1;
	}
      makeargv ();
      if (margv[0] == 0)
	continue;
      c = getcmd (margv[0]);
      if (Ambiguous (c))
	{
	  printf ("?Ambiguous command: %s\n", margv[0]);
	  continue;
	}
      if (c == 0)
	{
	  printf ("?Invalid command: %s\n", margv[0]);
	  continue;
	}
      /*
       * This should never happen...
       */
      if (c->needconnect && !connected)
	{
	  printf ("?Need to be connected first for %s.\n", margv[0]);
	  continue;
	}
      (*c->handler) (margc, margv);
    }
  fclose (rcfile);
}

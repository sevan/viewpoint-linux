/*
 * Copyright (c) 2011       , Peter Vrabec <pvrabec@redhat.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the copyright holders or contributors may not be used to
 *    endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#ifdef WITH_SELINUX

#include <stdio.h>
#include "defines.h"

#include <selinux/selinux.h>
#include <selinux/context.h>
#include "prototypes.h"

static bool selinux_checked = false;
static bool selinux_enabled;

/*
 * set_selinux_file_context - Set the security context before any file or
 *                            directory creation.
 *
 *	set_selinux_file_context () should be called before any creation
 *	of file, symlink, directory, ...
 *
 *	Callers may have to Reset SELinux to create files with default
 *	contexts with reset_selinux_file_context
 */
int set_selinux_file_context (const char *dst_name)
{
	/*@null@*/security_context_t scontext = NULL;

	if (!selinux_checked) {
		selinux_enabled = is_selinux_enabled () > 0;
		selinux_checked = true;
	}

	if (selinux_enabled) {
		/* Get the default security context for this file */
		if (matchpathcon (dst_name, 0, &scontext) < 0) {
			if (security_getenforce () != 0) {
				return 1;
			}
		}
		/* Set the security context for the next created file */
		if (setfscreatecon (scontext) < 0) {
			if (security_getenforce () != 0) {
				return 1;
			}
		}
		freecon (scontext);
	}
	return 0;
}

/*
 * reset_selinux_file_context - Reset the security context to the default
 *                              policy behavior
 *
 *	reset_selinux_file_context () should be called after the context
 *	was changed with set_selinux_file_context ()
 */
int reset_selinux_file_context (void)
{
	if (!selinux_checked) {
		selinux_enabled = is_selinux_enabled () > 0;
		selinux_checked = true;
	}
	if (selinux_enabled) {
		if (setfscreatecon (NULL) != 0) {
			return 1;
		}
	}
	return 0;
}

/*
 *  Log callback for libselinux internal error reporting.
 */
__attribute__((__format__ (printf, 2, 3)))
static int selinux_log_cb (int type, const char *fmt, ...) {
	va_list ap;
	char *buf;
	int r;
#ifdef WITH_AUDIT
	static int selinux_audit_fd = -2;
#endif

	va_start (ap, fmt);
	r = vasprintf (&buf, fmt, ap);
	va_end (ap);

	if (r < 0) {
		return 0;
	}

#ifdef WITH_AUDIT
	if (-2 == selinux_audit_fd) {
		selinux_audit_fd = audit_open ();

		if (-1 == selinux_audit_fd) {
			/* You get these only when the kernel doesn't have
			 * audit compiled in. */
			if (   (errno != EINVAL)
			    && (errno != EPROTONOSUPPORT)
			    && (errno != EAFNOSUPPORT)) {

			    (void) fputs (_("Cannot open audit interface.\n"),
			              stderr);
			    SYSLOG ((LOG_WARN, "Cannot open audit interface."));
			}
		}
	}

	if (-1 != selinux_audit_fd) {
		if (SELINUX_AVC == type) {
			if (audit_log_user_avc_message (selinux_audit_fd,
			    AUDIT_USER_AVC, buf, NULL, NULL,
			    NULL, 0) > 0) {
				goto skip_syslog;
			}
		} else if (SELINUX_ERROR == type) {
			if (audit_log_user_avc_message (selinux_audit_fd,
			    AUDIT_USER_SELINUX_ERR, buf, NULL, NULL,
			    NULL, 0) > 0) {
				goto skip_syslog;
			}
		}
	}
#endif

	SYSLOG ((LOG_WARN, "libselinux: %s", buf));

skip_syslog:
	free (buf);

	return 0;
}

/*
 * check_selinux_permit - Check whether SELinux grants the given
 *                        operation
 *
 *   Parameter is the SELinux permission name, e.g. rootok
 *
 *   Returns 0 when permission is granted
 *                  or something failed but running in
 *                  permissive mode
 */
int check_selinux_permit (const char *perm_name)
{
	char *user_context_str;
	int r;

	if (0 == is_selinux_enabled ()) {
		return 0;
	}

	selinux_set_callback (SELINUX_CB_LOG, (union selinux_callback) selinux_log_cb);

	if (getprevcon (&user_context_str) != 0) {
		fprintf (stderr,
		    _("%s: can not get previous SELinux process context: %s\n"),
		    Prog, strerror (errno));
		SYSLOG ((LOG_WARN,
		    "can not get previous SELinux process context: %s",
		    strerror (errno)));
		return (security_getenforce () != 0);
	}

	r = selinux_check_access (user_context_str, user_context_str, "passwd", perm_name, NULL);
	freecon (user_context_str);
	return r;
}

#else				/* !WITH_SELINUX */
extern int errno;		/* warning: ANSI C forbids an empty source file */
#endif				/* !WITH_SELINUX */

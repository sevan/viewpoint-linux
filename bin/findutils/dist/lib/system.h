/* system.h -- system-dependent definitions for findutils

   Copyright (C) 2017-2021 Free Software Foundation, Inc.

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
#if !defined SYSTEM_H
# define SYSTEM_H

# include <locale.h>

/* Take care of NLS matters.  */

# include "gettext.h"
# if ! ENABLE_NLS
#  undef textdomain
#  define textdomain(Domainname) /* empty */
#  undef bindtextdomain
#  define bindtextdomain(Domainname, Dirname) /* empty */
# endif

# define _(msgid) gettext (msgid)
# define N_(msgid) msgid

/* FALLTHROUGH
 * Since GCC7, the "-Werror=implicit-fallthrough=" option requires
 * fallthrough cases to be marked as such via:
 *     __attribute__ ((__fallthrough__))
 * Usage:
 *   switch (c)
 *     {
 *     case 1:
 *       doOne();
 *       FALLTHROUGH;
 *     case 2:
 *     ...
 *     }
 */
# ifndef FALLTHROUGH
#  if __GNUC__ < 7
#   define FALLTHROUGH ((void) 0)
#  else
#   define FALLTHROUGH __attribute__ ((__fallthrough__))
#  endif
# endif

#endif /* SYSTEM_H */

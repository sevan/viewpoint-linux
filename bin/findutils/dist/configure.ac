# -*- autoconf -*-
# Process this file with autoconf to produce a configure script.

# Copyright (C) 1996-2021 Free Software Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

dnl Written by James Youngman.

dnl Process this file with autoconf to produce a configure script.
# Make inter-release version strings look like, e.g., v4.6-131-gda920ee, which
# indicates that it is built from the 131st delta (in _some_ repository)
# following the v4.6 tag, and that da920ee is a prefix of the commit SHA1.
AC_INIT([GNU findutils],
        m4_esyscmd([build-aux/git-version-gen .tarball-version]),
        [bug-findutils@gnu.org])

dnl Set the bug-report URL
dnl
dnl The call to AC_INIT causes AC_PACKAGE_BUGREPORT to be defined
dnl and we've used an email address.  However, we would also like to
dnl specify an URL at which to report bugs (and in fact we prefer
dnl people to use that).  Se we define that here, too.
bugreport_url='https://savannah.gnu.org/bugs/?group=findutils'
dnl Ensure that PACKAGE_BUGREPORT_URL shows up in config.h so that
dnl it can be picked up by bugreport.c.
AC_DEFINE_UNQUOTED([PACKAGE_BUGREPORT_URL], ["$bugreport_url"],
                   [URL at which bugs should be reported])
dnl Also ensure that it is substituted in output files so that it
dnl turns up in locate/Makefile so that we can generate the correct
dnl content in the updatedb script.
AC_SUBST([PACKAGE_BUGREPORT_URL],[$bugreport_url])

AC_CONFIG_AUX_DIR(build-aux)
AM_INIT_AUTOMAKE([no-dist-gzip dist-xz color-tests])
AM_SILENT_RULES([yes]) # make --enable-silent-rules the default.

AC_CONFIG_SRCDIR([find/pred.c])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_HOST
AC_CONFIG_MACRO_DIR(gl/m4)

AC_SUBST(AUXDIR,$ac_aux_dir)

AC_ARG_ENABLE(leaf-optimisation,
	AS_HELP_STRING(--enable-leaf-optimisation,Enable an optimisation which saves lstat calls to identify subdirectories on filesystems having traditional Unix semantics),
	[ac_cv_leaf_optimisation=$enableval],[ac_cv_leaf_optimisation=yes])

AC_ARG_ENABLE(d_type-optimization,
	AS_HELP_STRING(--enable-d_type-optimization,Make use of the file type data returned in struct dirent.d_type by readdir()),
	[ac_cv_d_type=$enableval],[ac_cv_d_type=yes])

dnl This one has no default, because otherwise we would have to say
dnl both --enable-d_type-optimisation=no and --enable-d_type-optimization=no
dnl to turn it off.
AC_ARG_ENABLE(d_type-optimisation,
	AS_HELP_STRING(--enable-d_type-optimisation,Synonym for --enable-d_type-optimization),
	[ac_cv_d_type=$enableval],[])


AC_MSG_CHECKING([for leaf optimisation])
if test x$ac_cv_leaf_optimisation = xno; then
   AC_MSG_RESULT([no])
else
   AC_MSG_RESULT([yes])
   AC_DEFINE([LEAF_OPTIMISATION], 1, [Define if you want to use the leaf optimisation (this can still be turned off with -noleaf)])
fi

AC_ARG_VAR([DEFAULT_ARG_SIZE], [Default size of arguments to child processes
of find and xargs, 128k if unspecified])
if test -n "$DEFAULT_ARG_SIZE"; then
   AC_DEFINE_UNQUOTED([DEFAULT_ARG_SIZE], [$DEFAULT_ARG_SIZE],
     [If defined, the default argument size used in child processes])
fi



dnl Checks for programs.
AC_PROG_CC
AC_PROG_CPP

dnl for gnulib
gl_EARLY
AC_PROG_LN_S
AC_PROG_INSTALL
AC_CHECK_TOOLS([AR], [ar])
AC_CHECK_TOOLS([RANLIB], [ranlib], [:])
dnl AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
AC_SYS_LARGEFILE


gl_INIT

AC_ARG_ENABLE(compiler-warnings,
  AS_HELP_STRING(--enable-compiler-warnings,Enable many compiler warnings),
  [
  dnl Enable various GCC warnings.
  gl_MANYWARN_ALL_GCC([warnings])

  # Ensure VLAs are not used.
  # Note -Wvla is implicitly added by gl_MANYWARN_ALL_GCC
  AC_DEFINE([GNULIB_NO_VLA], [1], [Define to 1 to disable use of VLAs])

  # Set up the list of the pointless, undesired warnings.
  findutils_nw=
  findutils_nw="$findutils_nw -Wsystem-headers"	 # Don't let system headers trigger warnings
  findutils_nw="$findutils_nw -Wundef"           # All compiler preprocessors support #if UNDEF
  findutils_nw="$findutils_nw -Wtraditional"     # All compilers nowadays support ANSI C
  findutils_nw="$findutils_nw -Wconversion"      # These warnings usually don't point to mistakes.
  findutils_nw="$findutils_nw -Wsign-conversion" # Likewise.
  findutils_nw="$findutils_nw -Wc++-compat"      # malloc returns void* and a cast would be ugly.
  findutils_nw="$findutils_nw -Wswitch-default"  # A switch on an enum value needs no default.

  # Warnings we might enable in the future, but not yet (because they generate a
  # lot of noise).
  findutils_marginal=""
  findutils_marginal="$findutils_marginal -Wtraditional-conversion"
  findutils_marginal="$findutils_marginal -Wpadded"
  findutils_marginal="$findutils_marginal -Wformat-nonliteral"
  findutils_marginal="$findutils_marginal -Wunreachable-code"
  # -Wdouble-promotion generates many warnings when printing values with fprintf.
  findutils_marginal="$findutils_marginal -Wdouble-promotion"
  findutils_marginal="$findutils_marginal -Woverlength-strings"
  # Also disable some other warnings that we do in principle want, but currently lack
  # a way to portably avoid.
  findutils_tmp_nowarning=
  findutils_tmp_nowarning="$findutils_tmp_nowarning -Wsuggest-attribute=const"
  findutils_tmp_nowarning="$findutils_tmp_nowarning -Wsuggest-attribute=pure"
  findutils_tmp_nowarning="$findutils_tmp_nowarning -Wsuggest-attribute=format"
  # And some other warnings that we should fix but haven't, yet.
  findutils_tmp_nowarning="$findutils_tmp_nowarning -Wsuggest-attribute=const"

  # Enable all GCC warnings not in our list of excluded warnings.
  gl_MANYWARN_COMPLEMENT(
	[warnings], [$warnings],
	[$findutils_nw $findutils_marginal $findutils_tmp_nowarning])
  for w in $warnings
  do
    gl_WARN_ADD([$w])
  done
  # -Wextra implies -Wsign-compare, so removing -Wsign-compare from $warnings does not
  # actually eliminate this warning, as manywarnings will have included -Wextra.
  # We should actually eliminate the uses that cause this warning, but some of them are
  # tricky as they're comparisons between a type we choose and a type the implementation
  # chooses without stating whether or not it is signed (e.g. time_t).
  WARN_CFLAGS="$WARN_CFLAGS  -Wno-sign-compare"
  # Likewise for -Wunused-parameter: it is implied by -Wunused and -Wextra.
  gl_WARN_ADD([-Wno-unused-parameter])
  # Likewise for -Wformat-nonliteral: it is implied by -Wformat.
  gl_WARN_ADD([-Wno-format-nonliteral])
  ])

dnl For --enable-compiler-warnings-are-errors, any GCC compiler
dnl warning is actually an error which results in a non-zero result
dnl from the compiler (that is, the code will fail to compile).  We do
dnl this late in the configure script so that it doesn't interfere
dnl with the compilation tests run by other parts of the configure
dnl script.
AC_ARG_ENABLE(compiler-warnings-are-errors,
  AS_HELP_STRING(--enable-compiler-warnings-are-errors,Compiler warnings are errors),
  [
    AC_MSG_CHECKING([whether it is safe to use -Werror])
    if test -n "$GCC"; then
      CFLAGS="$CFLAGS -Werror"
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no, because $CC is not GCC])
    fi
  ])

dnl Older versions of gl/m4/nls.m4 provide AM_MKINSTALLDIRS.
dnl The current version of gnulib does not, but the version of
dnl po/Makefile.in.in that comes with gettext-0.14.6 expects
dnl that @MKINSTALLDIRS@ will be expanded.
AM_MKINSTALLDIRS

dnl lib/regexprops needs to be a native program, because we need to
dnl run it in order to generate the documentation about the properties
dnl of regular expressions.  See lib/Makefile.am.
AM_CONDITIONAL(CROSS_COMPILING, [[test "x$cross_compiling" = xyes]])

dnl Try to get a POSIX.1 environment.

dnl Checks for libraries.  If getpwnam() isn't present in the
dnl C library, try -lsun.
AC_CHECK_FUNC(getpwnam, [],
[AC_CHECK_LIB(sun, getpwnam)])

AC_CHECK_LIB([m],[modf],[FINDLIBS="-lm $FINDLIBS"]
  AC_DEFINE_UNQUOTED([HAVE_MODF_IN_LIBM],1,[modf is defined in -lm]))
AC_CHECK_LIB([m],[fabs],[FINDLIBS="-lm $FINDLIBS"]
  AC_DEFINE_UNQUOTED([HAVE_FABS_IN_LIBM],1,[fabs is defined in -lm]))
AC_SUBST([FINDLIBS])

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(sys/param.h mntent.h sys/mnttab.h sys/mntio.h sys/mkdev.h)
dnl fdleak.c needs sys/resource.h because it calls getrlimit(2).
AC_CHECK_FUNCS(getrlimit)
AC_HEADER_MAJOR
dnl TODO: it's possible gnulib eliminates the need for AC_HEADER_DIRENT.
AC_HEADER_DIRENT
AC_HEADER_STAT
AC_HEADER_SYS_WAIT


dnl Checks for typedefs, structures, and compiler characteristics.

AC_TYPE_UID_T
AC_TYPE_SIZE_T
AC_CHECK_TYPE(ssize_t, int)
AC_TYPE_PID_T
AC_CHECK_TYPE(ino_t, unsigned long)
AC_CHECK_TYPE(dev_t, unsigned long)
AC_TYPE_MODE_T
AC_STRUCT_ST_BLOCKS
AC_CHECK_MEMBERS([struct stat.st_rdev])

dnl fdleak.c uses PROMOTED_MODE_T, which is defined by gnulib.
gl_PROMOTED_TYPE_MODE_T


AC_STRUCT_TM
AC_STRUCT_TIMEZONE

dnl Checks for library functions that are provided by gnulib.
AC_FUNC_STRFTIME
AC_REPLACE_FUNCS(memcmp memset stpcpy strdup strstr strtol strtoul)
AC_CHECK_FUNCS(fchdir getcwd strerror endgrent endpwent setlocale)
AC_CHECK_FUNCS(getrusage)
AC_FUNC_VPRINTF
AC_FUNC_ALLOCA
AC_FUNC_CLOSEDIR_VOID

dnl We don't just use AC_FUNC_GETMNTENT directly because it
dnl will try to use -lsun on platforms which have getmntent() in the
dnl C library, for example UNICOS.
AC_CHECK_FUNC(getmntent, [], [AC_FUNC_GETMNTENT])
AC_CHECK_FUNCS(getmntent)
AC_CHECK_FUNCS(setmntent endmntent)


# Check for common but not-POSIX functions.
AC_CHECK_FUNCS(setgroups)


dnl gl_XALLOC
gl_FUNC_ALLOCA

dnl Use gl_INCLUDED_REGEX so that findutils will build on systems like
dnl Solaris, which lacks those functions in libc (see GNU Savannah bug
dnl #11710) (Sun Sep  4 20:15:11 2005: gl_INCLUDED_REGEX no longer seems
dnl to be available in gnulib CVS)
gl_REGEX

AC_PREREQ([2.69])

jy_SORTZ


AC_CHECK_MEMBER(struct dirent.d_type,,,[
#include <sys/types.h>
#include <dirent.h>])

jy_AC_ATTRIBUTE_NORETURN

dnl internationalization macros
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.3])

dnl regextype.c and regexprops.c are designed to be usable outside findutils,
dnl but findutils doesn't want to support all the regex types in gnulib,
dnl and wants to support an additional one (RE_SYNTAX_EMACS|RE_DOT_NEWLINE).
dnl Hence they need to know if they are being compiled into findutils or not.
AC_DEFINE([FINDUTILS], 1, [Define if we are compiling GNU findutils])
AC_DEFINE([ALREADY_INCLUDED_CONFIG_H], 1, [Define so that source code can verify that config.h was already included])

dnl Test support.
AM_PATH_PYTHON(,, [:])
AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON" != :])
AC_CHECK_PROGS([FAKETIME],[faketime],[:])
AM_CONDITIONAL([HAVE_FAKETIME], [test "$FAKETIME" != :])

# This is necessary so that .o files in LIBOBJS are also built via
# the ANSI2KNR-filtering rules.
#LIBOBJS=`echo $LIBOBJS|sed 's/\.o /\$U.o /g;s/\.o$/\$U.o/'`

# Note that in the list below, po/Makefile.in should appear before
# po/Makefile, so that po/Makefile can be created even if po/Makefile.in
# starts off missing.
AC_CONFIG_FILES([
Makefile
build-aux/Makefile
doc/Makefile
find/Makefile
find/testsuite/Makefile
gl/Makefile
gl/lib/Makefile
lib/Makefile
locate/Makefile
locate/testsuite/Makefile
m4/Makefile
po/Makefile.in
po/Makefile
gnulib-tests/Makefile
xargs/Makefile
xargs/testsuite/Makefile
])
AC_OUTPUT
dnl intl/Makefile

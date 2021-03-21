/* gcc-function-attribtues.h -- GCC-specific function attributes

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

/*
 Be aware that some function attributes do not work with function
 pointers.  See
 https://lists.gnu.org/r/bug-gnulib/2011-04/msg00007.html
 for details.
*/
#ifndef _GCC_FUNCTION_ATTRIBUTES_H
# define _GCC_FUNCTION_ATTRIBUTES_H

# ifndef __GNUC_PREREQ
#  if defined __GNUC__ && defined __GNUC_MINOR__
#   define __GNUC_PREREQ(maj, min) \
         ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#  else
#   define __GNUC_PREREQ(maj, min) 0
#  endif
# endif

/*
The following attributes are currently (GCC-4.4.5) defined for
functions on all targets.  Where this file provides a macro
for using it, the macro name is given in the second column.

Attribute                 Macro (if implemented in this file)
-------------------------------------------------------------------------------
alias
aligned
alloc_size		  _GL_ATTRIBUTE_ALLOC_SIZE(arg_num)
always_inline
artificial
cold
const
constructor
deprecated		  _GL_ATTRIBUTE_DEPRECATED
destructor
error
externally_visible
flatten
format			  _GL_ATTRIBUTE_FORMAT(spec)
                          _GL_ATTRIBUTE_FORMAT_PRINTF_SYSTEM(fmt,firstarg)
                          _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(fmt,firstarg)
                          _GL_ATTRIBUTE_FORMAT_SCANF_SYSTEM(fmt,firstarg)
                          _GL_ATTRIBUTE_FORMAT_SCANF(fmt,firstarg)
format_arg
gnu_inline
hot
malloc			  _GL_ATTRIBUTE_MALLOC
no_instrument_function
noinline
nonnull			  _GL_ATTRIBUTE_NONNULL(args)
                          _GL_ARG_NONNULL(args)
noreturn		  _GL_ATTRIBUTE_NORETURN
nothrow
pure			  _GL_ATTRIBUTE_PURE
returns_twice
section
sentinel		  _GL_ATTRIBUTE_SENTINEL
unused
used
warn_unused_result        _GL_ATTRIBUTE_WUR
warning
weak
*/

/*
Attributes used in gnulib, but which appear to be platform-specific
regparm
stdcall
*/

/*
Attributes used in gnulib with special arguments
Macro         Args
visibility    "default"
*/

/*
   The __attribute__ feature is available in gcc versions 2.5 and later.
   The underscored __format__ spelling of the attribute names requires 2.6.4 (we check for 2.7).
*/


# ifndef _GL_ATTRIBUTE_ALLOC_SIZE
#  if __GNUC_PREREQ(4,3)
#   define _GL_ATTRIBUTE_ALLOC_SIZE(args) __attribute__ ((__alloc_size__ args))
#  else
#   define _GL_ATTRIBUTE_ALLOC_SIZE(args) /* unsupported */
#  endif
# endif

# ifndef _GL_ATTRIBUTE_DEPRECATED
#  if __GNUC_PREREQ(3,1)
#   define _GL_ATTRIBUTE_DEPRECATED __attribute__ ((__deprecated__))
#  else
#   define _GL_ATTRIBUTE_DEPRECATED /* empty */
#  endif
# endif

# ifndef _GL_ATTRIBUTE_FORMAT
#  if __GNUC_PREREQ(2,7)
#   define _GL_ATTRIBUTE_FORMAT(spec)  __attribute__ ((__format__ spec))
#  else
#   define _GL_ATTRIBUTE_FORMAT(spec) /* unsupported */
#  endif
# endif

# ifndef _GL_ATTRIBUTE_FORMAT_PRINTF_SYSTEM
#  if __GNUC_PREREQ(2,7)
#   define _GL_ATTRIBUTE_FORMAT_PRINTF_SYSTEM(formatstring_parameter, first_argument) \
    _GL_ATTRIBUTE_FORMAT ((__printf__, formatstring_parameter, first_argument))
#  else
#   define _GL_ATTRIBUTE_FORMAT_PRINTF_SYSTEM(spec) /* unsupported */
#  endif
# endif

# ifndef _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD
#  if __GNUC_PREREQ(2,7)
#   if __GNUC_PREREQ(4,4)
#    define _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(formatstring_parameter, first_argument) \
     _GL_ATTRIBUTE_FORMAT ((__gnu_printf__, formatstring_parameter, first_argument))
#   else
#    define _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(formatstring_parameter, first_argument) \
     _GL_ATTRIBUTE_FORMAT ((__printf__, formatstring_parameter, first_argument))
#   endif
#  else
#   define _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(spec) /* unsupported */
#  endif
# endif

# ifndef _GL_ATTRIBUTE_FORMAT_SCANF_SYSTEM
#  if __GNUC_PREREQ(2,7)
#   define _GL_ATTRIBUTE_FORMAT_SCANF_SYSTEM(formatstring_parameter, first_argument) \
    _GL_ATTRIBUTE_FORMAT ((__scanf__, formatstring_parameter, first_argument))
#  else
#   define _GL_ATTRIBUTE_FORMAT_SCANF_SYSTEM(spec) /* unsupported */
#  endif
# endif

# ifndef _GL_ATTRIBUTE_FORMAT_SCANF
#  if __GNUC_PREREQ(2,7)
#   if __GNUC_PREREQ(4,4)
#    define _GL_ATTRIBUTE_FORMAT_SCANF(formatstring_parameter, first_argument) \
     _GL_ATTRIBUTE_FORMAT ((__gnu_scanf__, formatstring_parameter, first_argument))
#   else
#    define _GL_ATTRIBUTE_FORMAT_SCANF(formatstring_parameter, first_argument) \
     _GL_ATTRIBUTE_FORMAT ((__scanf__, formatstring_parameter, first_argument))
#   endif
#  else
#   define _GL_ATTRIBUTE_FORMAT_SCANF(spec) /* unsupported */
#  endif
# endif

# ifndef _GL_ATTRIBUTE_MALLOC
#  if __GNUC_PREREQ(3,0)
#   define _GL_ATTRIBUTE_MALLOC __attribute__ ((__malloc__))
#  else
#   define _GL_ATTRIBUTE_MALLOC /* unsupported */
#  endif
# endif


# ifndef _GL_ATTRIBUTE_NONNULL
#  if __GNUC_PREREQ(3,3)
#   define _GL_ATTRIBUTE_NONNULL(m) __attribute__ ((__nonnull__ (m)))
#  else
#   define _GL_ATTRIBUTE_NONNULL(m) /* unsupported */
#  endif
# endif
# ifndef _GL_ARG_NONNULL
/* alternative spelling used in gnulib's stdio.h */
#  define _GL_ARG_NONNULL(m) _GL_ATTRIBUTE_NONNULL(m)
# endif


# ifndef _GL_ATTRIBUTE_NORETURN
#  if __GNUC_PREREQ(2,8)
#   define _GL_ATTRIBUTE_NORETURN __attribute__ ((__noreturn__))
#  else
#   define _GL_ATTRIBUTE_NORETURN /* unsupported */
#  endif
# endif

# ifndef _GL_ATTRIBUTE_PURE
#  if __GNUC_PREREQ(2,96)
#   define _GL_ATTRIBUTE_PURE __attribute__ ((__pure__))
#  else
#   define _GL_ATTRIBUTE_PURE /* unsupported */
#  endif
# endif

# ifndef _GL_ATTRIBUTE_SENTINEL
#  if __GNUC_PREREQ(4,0)
  /* gnulib uses the __attribute__((__sentinel__)) variant, for which the
    argument number 0 is assumed.  Arguments are counted backwards, the last
    being 0.
  */
#   define _GL_ATTRIBUTE_SENTINEL(backward_arg_num) __attribute__ ((__sentinel__(backward_arg_num)))
#  else
#   define _GL_ATTRIBUTE_SENTINEL(backward_arg_num) /* unsupported */
#  endif
# endif

# ifndef _GL_ATTRIBUTE_WUR
#  if __GNUC_PREREQ(3,4)
#   define _GL_ATTRIBUTE_WUR __attribute__ ((__warn__unused_result__))
#  else
#   define _GL_ATTRIBUTE_WUR /* unsupported */
#  endif
# endif
# ifndef _GL_ATTRIBUTE_RETURN_CHECK
/* gnulib is inconsistent in which macro it uses; support both for now. */
#  define _GL_ATTRIBUTE_RETURN_CHECK _GL_ATTRIBUTE_WUR
# endif

#endif /* _GCC_FUNCTION_ATTRIBUTES_H */

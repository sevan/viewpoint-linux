/* A Bison parser, made by GNU Bison 3.6.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.6.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the type names.  */
#define YYSTYPE         PO_GRAM_STYPE
/* Substitute the variable and function names.  */
#define yyparse         po_gram_parse
#define yylex           po_gram_lex
#define yyerror         po_gram_error
#define yydebug         po_gram_debug
#define yynerrs         po_gram_nerrs
#define yylval          po_gram_lval
#define yychar          po_gram_char

/* First part of user prologue.  */
#line 20 "po-gram-gen.y"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Specification.  */
#include "po-gram.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str-list.h"
#include "po-lex.h"
#include "po-charset.h"
#include "error.h"
#include "xalloc.h"
#include "gettext.h"
#include "read-catalog-abstract.h"

#define _(str) gettext (str)

static long plural_counter;

#define check_obsolete(value1,value2) \
  if ((value1).obsolete != (value2).obsolete) \
    po_gram_error_at_line (&(value2).pos, _("inconsistent use of #~"));

static inline void
do_callback_message (char *msgctxt,
                     char *msgid, lex_pos_ty *msgid_pos, char *msgid_plural,
                     char *msgstr, size_t msgstr_len, lex_pos_ty *msgstr_pos,
                     char *prev_msgctxt,
                     char *prev_msgid, char *prev_msgid_plural,
                     bool obsolete)
{
  /* Test for header entry.  Ignore fuzziness of the header entry.  */
  if (msgctxt == NULL && msgid[0] == '\0' && !obsolete)
    po_lex_charset_set (msgstr, gram_pos.file_name);

  po_callback_message (msgctxt,
                       msgid, msgid_pos, msgid_plural,
                       msgstr, msgstr_len, msgstr_pos,
                       prev_msgctxt, prev_msgid, prev_msgid_plural,
                       false, obsolete);
}

#define free_message_intro(value) \
  if ((value).prev_ctxt != NULL)        \
    free ((value).prev_ctxt);           \
  if ((value).prev_id != NULL)          \
    free ((value).prev_id);             \
  if ((value).prev_id_plural != NULL)   \
    free ((value).prev_id_plural);      \
  if ((value).ctxt != NULL)             \
    free ((value).ctxt);


#line 139 "po-gram-gen.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
#ifndef YY_PO_GRAM_PO_GRAM_GEN_TAB_H_INCLUDED
# define YY_PO_GRAM_PO_GRAM_GEN_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef PO_GRAM_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define PO_GRAM_DEBUG 1
#  else
#   define PO_GRAM_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define PO_GRAM_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined PO_GRAM_DEBUG */
#if PO_GRAM_DEBUG
extern int po_gram_debug;
#endif

/* Token kinds.  */
#ifndef PO_GRAM_TOKENTYPE
# define PO_GRAM_TOKENTYPE
  enum po_gram_tokentype
  {
    PO_GRAM_EMPTY = -2,
    PO_GRAM_EOF = 0,               /* "end of file"  */
    PO_GRAM_error = 256,           /* error  */
    PO_GRAM_UNDEF = 257,           /* "invalid token"  */
    COMMENT = 258,                 /* COMMENT  */
    DOMAIN = 259,                  /* DOMAIN  */
    JUNK = 260,                    /* JUNK  */
    PREV_MSGCTXT = 261,            /* PREV_MSGCTXT  */
    PREV_MSGID = 262,              /* PREV_MSGID  */
    PREV_MSGID_PLURAL = 263,       /* PREV_MSGID_PLURAL  */
    PREV_STRING = 264,             /* PREV_STRING  */
    MSGCTXT = 265,                 /* MSGCTXT  */
    MSGID = 266,                   /* MSGID  */
    MSGID_PLURAL = 267,            /* MSGID_PLURAL  */
    MSGSTR = 268,                  /* MSGSTR  */
    NAME = 269,                    /* NAME  */
    NUMBER = 270,                  /* NUMBER  */
    STRING = 271                   /* STRING  */
  };
  typedef enum po_gram_tokentype po_gram_token_kind_t;
#endif

/* Value type.  */
#if ! defined PO_GRAM_STYPE && ! defined PO_GRAM_STYPE_IS_DECLARED
union PO_GRAM_STYPE
{
#line 103 "po-gram-gen.y"

  struct { char *string; lex_pos_ty pos; bool obsolete; } string;
  struct { string_list_ty stringlist; lex_pos_ty pos; bool obsolete; } stringlist;
  struct { long number; lex_pos_ty pos; bool obsolete; } number;
  struct { lex_pos_ty pos; bool obsolete; } pos;
  struct { char *ctxt; char *id; char *id_plural; lex_pos_ty pos; bool obsolete; } prev;
  struct { char *prev_ctxt; char *prev_id; char *prev_id_plural; char *ctxt; lex_pos_ty pos; bool obsolete; } message_intro;
  struct { struct msgstr_def rhs; lex_pos_ty pos; bool obsolete; } rhs;

#line 223 "po-gram-gen.tab.c"

};
typedef union PO_GRAM_STYPE PO_GRAM_STYPE;
# define PO_GRAM_STYPE_IS_TRIVIAL 1
# define PO_GRAM_STYPE_IS_DECLARED 1
#endif


extern PO_GRAM_STYPE po_gram_lval;

int po_gram_parse (void);

#endif /* !YY_PO_GRAM_PO_GRAM_GEN_TAB_H_INCLUDED  */
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_COMMENT = 3,                    /* COMMENT  */
  YYSYMBOL_DOMAIN = 4,                     /* DOMAIN  */
  YYSYMBOL_JUNK = 5,                       /* JUNK  */
  YYSYMBOL_PREV_MSGCTXT = 6,               /* PREV_MSGCTXT  */
  YYSYMBOL_PREV_MSGID = 7,                 /* PREV_MSGID  */
  YYSYMBOL_PREV_MSGID_PLURAL = 8,          /* PREV_MSGID_PLURAL  */
  YYSYMBOL_PREV_STRING = 9,                /* PREV_STRING  */
  YYSYMBOL_MSGCTXT = 10,                   /* MSGCTXT  */
  YYSYMBOL_MSGID = 11,                     /* MSGID  */
  YYSYMBOL_MSGID_PLURAL = 12,              /* MSGID_PLURAL  */
  YYSYMBOL_MSGSTR = 13,                    /* MSGSTR  */
  YYSYMBOL_NAME = 14,                      /* NAME  */
  YYSYMBOL_15_ = 15,                       /* '['  */
  YYSYMBOL_16_ = 16,                       /* ']'  */
  YYSYMBOL_NUMBER = 17,                    /* NUMBER  */
  YYSYMBOL_STRING = 18,                    /* STRING  */
  YYSYMBOL_YYACCEPT = 19,                  /* $accept  */
  YYSYMBOL_po_file = 20,                   /* po_file  */
  YYSYMBOL_comment = 21,                   /* comment  */
  YYSYMBOL_domain = 22,                    /* domain  */
  YYSYMBOL_message = 23,                   /* message  */
  YYSYMBOL_message_intro = 24,             /* message_intro  */
  YYSYMBOL_prev = 25,                      /* prev  */
  YYSYMBOL_msg_intro = 26,                 /* msg_intro  */
  YYSYMBOL_prev_msg_intro = 27,            /* prev_msg_intro  */
  YYSYMBOL_msgid_pluralform = 28,          /* msgid_pluralform  */
  YYSYMBOL_prev_msgid_pluralform = 29,     /* prev_msgid_pluralform  */
  YYSYMBOL_pluralform_list = 30,           /* pluralform_list  */
  YYSYMBOL_pluralform = 31,                /* pluralform  */
  YYSYMBOL_string_list = 32,               /* string_list  */
  YYSYMBOL_prev_string_list = 33           /* prev_string_list  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined PO_GRAM_STYPE_IS_TRIVIAL && PO_GRAM_STYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   40

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  19
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  15
/* YYNRULES -- Number of rules.  */
#define YYNRULES  30
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  46

#define YYMAXUTOK   271


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    15,     2,    16,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      17,    18
};

#if PO_GRAM_DEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   129,   129,   130,   131,   132,   133,   138,   146,   154,
     175,   196,   205,   214,   225,   234,   248,   257,   271,   277,
     288,   294,   306,   317,   328,   332,   347,   370,   378,   390,
     398
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if PO_GRAM_DEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "COMMENT", "DOMAIN",
  "JUNK", "PREV_MSGCTXT", "PREV_MSGID", "PREV_MSGID_PLURAL", "PREV_STRING",
  "MSGCTXT", "MSGID", "MSGID_PLURAL", "MSGSTR", "NAME", "'['", "']'",
  "NUMBER", "STRING", "$accept", "po_file", "comment", "domain", "message",
  "message_intro", "prev", "msg_intro", "prev_msg_intro",
  "msgid_pluralform", "prev_msgid_pluralform", "pluralform_list",
  "pluralform", "string_list", "prev_string_list", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,    91,    93,   270,   271
};
#endif

#define YYPACT_NINF (-26)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int8 yypact[] =
{
     -26,     2,   -26,   -26,   -26,    -8,     5,   -26,     0,   -26,
     -26,   -26,   -26,     0,    13,   -26,     5,   -26,   -26,    20,
     -26,    -7,     8,   -26,    24,   -26,   -26,   -26,   -26,     0,
       7,    15,    15,   -26,     5,   -26,    12,    17,    12,    21,
      15,   -26,    26,    22,     0,    12
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       2,     0,     1,     6,     7,     0,     0,    20,     0,    18,
       3,     4,     5,     0,     0,    14,     0,     8,    29,     0,
      27,     0,    13,    15,    16,    21,    30,    19,    28,     0,
       0,    11,    12,    24,     0,    17,    22,     0,     9,     0,
      10,    25,    23,     0,     0,    26
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -26,   -26,   -26,   -26,   -26,   -26,   -26,    23,   -26,   -26,
     -26,     9,   -25,   -13,   -15
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     1,    10,    11,    12,    13,    14,    15,    16,    31,
      35,    32,    33,    21,    19
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] =
{
      22,    24,     2,     3,    27,     4,     5,    41,     6,     7,
      17,    28,     8,     9,    18,    41,    36,    38,    20,    42,
      29,    30,    37,     8,     9,    20,    28,    25,    39,    26,
      28,    45,    34,    26,    43,    26,    37,    23,    44,     0,
      40
};

static const yytype_int8 yycheck[] =
{
      13,    16,     0,     1,    11,     3,     4,    32,     6,     7,
      18,    18,    10,    11,     9,    40,    29,    30,    18,    34,
      12,    13,    15,    10,    11,    18,    18,     7,    13,     9,
      18,    44,     8,     9,    17,     9,    15,    14,    16,    -1,
      31
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,    20,     0,     1,     3,     4,     6,     7,    10,    11,
      21,    22,    23,    24,    25,    26,    27,    18,     9,    33,
      18,    32,    32,    26,    33,     7,     9,    11,    18,    12,
      13,    28,    30,    31,     8,    29,    32,    15,    32,    13,
      30,    31,    33,    17,    16,    32
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int8 yyr1[] =
{
       0,    19,    20,    20,    20,    20,    20,    21,    22,    23,
      23,    23,    23,    23,    24,    24,    25,    25,    26,    26,
      27,    27,    28,    29,    30,    30,    31,    32,    32,    33,
      33
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     2,     2,     2,     2,     1,     2,     4,
       4,     3,     3,     2,     1,     2,     2,     3,     1,     3,
       1,     3,     2,     2,     1,     2,     5,     1,     2,     1,
       2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = PO_GRAM_EMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == PO_GRAM_EMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use PO_GRAM_error or PO_GRAM_UNDEF. */
#define YYERRCODE PO_GRAM_UNDEF


/* Enable debugging if requested.  */
#if PO_GRAM_DEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
# ifndef YY_LOCATION_PRINT
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YYUSE (yyoutput);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yykind < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yykind], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !PO_GRAM_DEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !PO_GRAM_DEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize;

    /* The state stack.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss;
    yy_state_t *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yynerrs = 0;
  yystate = 0;
  yyerrstatus = 0;

  yystacksize = YYINITDEPTH;
  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;


  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = PO_GRAM_EMPTY; /* Cause a token to be read.  */
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == PO_GRAM_EMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= PO_GRAM_EOF)
    {
      yychar = PO_GRAM_EOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == PO_GRAM_error)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = PO_GRAM_UNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = PO_GRAM_EMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 7:
#line 139 "po-gram-gen.y"
                {
                  po_callback_comment_dispatcher ((yyvsp[0].string).string);
                }
#line 1267 "po-gram-gen.tab.c"
    break;

  case 8:
#line 147 "po-gram-gen.y"
                {
                   po_callback_domain ((yyvsp[0].string).string);
                }
#line 1275 "po-gram-gen.tab.c"
    break;

  case 9:
#line 155 "po-gram-gen.y"
                {
                  char *string2 = string_list_concat_destroy (&(yyvsp[-2].stringlist).stringlist);
                  char *string4 = string_list_concat_destroy (&(yyvsp[0].stringlist).stringlist);

                  check_obsolete ((yyvsp[-3].message_intro), (yyvsp[-2].stringlist));
                  check_obsolete ((yyvsp[-3].message_intro), (yyvsp[-1].pos));
                  check_obsolete ((yyvsp[-3].message_intro), (yyvsp[0].stringlist));
                  if (!(yyvsp[-3].message_intro).obsolete || pass_obsolete_entries)
                    do_callback_message ((yyvsp[-3].message_intro).ctxt, string2, &(yyvsp[-3].message_intro).pos, NULL,
                                         string4, strlen (string4) + 1, &(yyvsp[-1].pos).pos,
                                         (yyvsp[-3].message_intro).prev_ctxt,
                                         (yyvsp[-3].message_intro).prev_id, (yyvsp[-3].message_intro).prev_id_plural,
                                         (yyvsp[-3].message_intro).obsolete);
                  else
                    {
                      free_message_intro ((yyvsp[-3].message_intro));
                      free (string2);
                      free (string4);
                    }
                }
#line 1300 "po-gram-gen.tab.c"
    break;

  case 10:
#line 176 "po-gram-gen.y"
                {
                  char *string2 = string_list_concat_destroy (&(yyvsp[-2].stringlist).stringlist);

                  check_obsolete ((yyvsp[-3].message_intro), (yyvsp[-2].stringlist));
                  check_obsolete ((yyvsp[-3].message_intro), (yyvsp[-1].string));
                  check_obsolete ((yyvsp[-3].message_intro), (yyvsp[0].rhs));
                  if (!(yyvsp[-3].message_intro).obsolete || pass_obsolete_entries)
                    do_callback_message ((yyvsp[-3].message_intro).ctxt, string2, &(yyvsp[-3].message_intro).pos, (yyvsp[-1].string).string,
                                         (yyvsp[0].rhs).rhs.msgstr, (yyvsp[0].rhs).rhs.msgstr_len, &(yyvsp[0].rhs).pos,
                                         (yyvsp[-3].message_intro).prev_ctxt,
                                         (yyvsp[-3].message_intro).prev_id, (yyvsp[-3].message_intro).prev_id_plural,
                                         (yyvsp[-3].message_intro).obsolete);
                  else
                    {
                      free_message_intro ((yyvsp[-3].message_intro));
                      free (string2);
                      free ((yyvsp[-1].string).string);
                      free ((yyvsp[0].rhs).rhs.msgstr);
                    }
                }
#line 1325 "po-gram-gen.tab.c"
    break;

  case 11:
#line 197 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-2].message_intro), (yyvsp[-1].stringlist));
                  check_obsolete ((yyvsp[-2].message_intro), (yyvsp[0].string));
                  po_gram_error_at_line (&(yyvsp[-2].message_intro).pos, _("missing 'msgstr[]' section"));
                  free_message_intro ((yyvsp[-2].message_intro));
                  string_list_destroy (&(yyvsp[-1].stringlist).stringlist);
                  free ((yyvsp[0].string).string);
                }
#line 1338 "po-gram-gen.tab.c"
    break;

  case 12:
#line 206 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-2].message_intro), (yyvsp[-1].stringlist));
                  check_obsolete ((yyvsp[-2].message_intro), (yyvsp[0].rhs));
                  po_gram_error_at_line (&(yyvsp[-2].message_intro).pos, _("missing 'msgid_plural' section"));
                  free_message_intro ((yyvsp[-2].message_intro));
                  string_list_destroy (&(yyvsp[-1].stringlist).stringlist);
                  free ((yyvsp[0].rhs).rhs.msgstr);
                }
#line 1351 "po-gram-gen.tab.c"
    break;

  case 13:
#line 215 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-1].message_intro), (yyvsp[0].stringlist));
                  po_gram_error_at_line (&(yyvsp[-1].message_intro).pos, _("missing 'msgstr' section"));
                  free_message_intro ((yyvsp[-1].message_intro));
                  string_list_destroy (&(yyvsp[0].stringlist).stringlist);
                }
#line 1362 "po-gram-gen.tab.c"
    break;

  case 14:
#line 226 "po-gram-gen.y"
                {
                  (yyval.message_intro).prev_ctxt = NULL;
                  (yyval.message_intro).prev_id = NULL;
                  (yyval.message_intro).prev_id_plural = NULL;
                  (yyval.message_intro).ctxt = (yyvsp[0].string).string;
                  (yyval.message_intro).pos = (yyvsp[0].string).pos;
                  (yyval.message_intro).obsolete = (yyvsp[0].string).obsolete;
                }
#line 1375 "po-gram-gen.tab.c"
    break;

  case 15:
#line 235 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-1].prev), (yyvsp[0].string));
                  (yyval.message_intro).prev_ctxt = (yyvsp[-1].prev).ctxt;
                  (yyval.message_intro).prev_id = (yyvsp[-1].prev).id;
                  (yyval.message_intro).prev_id_plural = (yyvsp[-1].prev).id_plural;
                  (yyval.message_intro).ctxt = (yyvsp[0].string).string;
                  (yyval.message_intro).pos = (yyvsp[0].string).pos;
                  (yyval.message_intro).obsolete = (yyvsp[0].string).obsolete;
                }
#line 1389 "po-gram-gen.tab.c"
    break;

  case 16:
#line 249 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-1].string), (yyvsp[0].stringlist));
                  (yyval.prev).ctxt = (yyvsp[-1].string).string;
                  (yyval.prev).id = string_list_concat_destroy (&(yyvsp[0].stringlist).stringlist);
                  (yyval.prev).id_plural = NULL;
                  (yyval.prev).pos = (yyvsp[-1].string).pos;
                  (yyval.prev).obsolete = (yyvsp[-1].string).obsolete;
                }
#line 1402 "po-gram-gen.tab.c"
    break;

  case 17:
#line 258 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-2].string), (yyvsp[-1].stringlist));
                  check_obsolete ((yyvsp[-2].string), (yyvsp[0].string));
                  (yyval.prev).ctxt = (yyvsp[-2].string).string;
                  (yyval.prev).id = string_list_concat_destroy (&(yyvsp[-1].stringlist).stringlist);
                  (yyval.prev).id_plural = (yyvsp[0].string).string;
                  (yyval.prev).pos = (yyvsp[-2].string).pos;
                  (yyval.prev).obsolete = (yyvsp[-2].string).obsolete;
                }
#line 1416 "po-gram-gen.tab.c"
    break;

  case 18:
#line 272 "po-gram-gen.y"
                {
                  (yyval.string).string = NULL;
                  (yyval.string).pos = (yyvsp[0].pos).pos;
                  (yyval.string).obsolete = (yyvsp[0].pos).obsolete;
                }
#line 1426 "po-gram-gen.tab.c"
    break;

  case 19:
#line 278 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-2].pos), (yyvsp[-1].stringlist));
                  check_obsolete ((yyvsp[-2].pos), (yyvsp[0].pos));
                  (yyval.string).string = string_list_concat_destroy (&(yyvsp[-1].stringlist).stringlist);
                  (yyval.string).pos = (yyvsp[0].pos).pos;
                  (yyval.string).obsolete = (yyvsp[0].pos).obsolete;
                }
#line 1438 "po-gram-gen.tab.c"
    break;

  case 20:
#line 289 "po-gram-gen.y"
                {
                  (yyval.string).string = NULL;
                  (yyval.string).pos = (yyvsp[0].pos).pos;
                  (yyval.string).obsolete = (yyvsp[0].pos).obsolete;
                }
#line 1448 "po-gram-gen.tab.c"
    break;

  case 21:
#line 295 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-2].pos), (yyvsp[-1].stringlist));
                  check_obsolete ((yyvsp[-2].pos), (yyvsp[0].pos));
                  (yyval.string).string = string_list_concat_destroy (&(yyvsp[-1].stringlist).stringlist);
                  (yyval.string).pos = (yyvsp[0].pos).pos;
                  (yyval.string).obsolete = (yyvsp[0].pos).obsolete;
                }
#line 1460 "po-gram-gen.tab.c"
    break;

  case 22:
#line 307 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-1].pos), (yyvsp[0].stringlist));
                  plural_counter = 0;
                  (yyval.string).string = string_list_concat_destroy (&(yyvsp[0].stringlist).stringlist);
                  (yyval.string).pos = (yyvsp[-1].pos).pos;
                  (yyval.string).obsolete = (yyvsp[-1].pos).obsolete;
                }
#line 1472 "po-gram-gen.tab.c"
    break;

  case 23:
#line 318 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-1].pos), (yyvsp[0].stringlist));
                  (yyval.string).string = string_list_concat_destroy (&(yyvsp[0].stringlist).stringlist);
                  (yyval.string).pos = (yyvsp[-1].pos).pos;
                  (yyval.string).obsolete = (yyvsp[-1].pos).obsolete;
                }
#line 1483 "po-gram-gen.tab.c"
    break;

  case 24:
#line 329 "po-gram-gen.y"
                {
                  (yyval.rhs) = (yyvsp[0].rhs);
                }
#line 1491 "po-gram-gen.tab.c"
    break;

  case 25:
#line 333 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-1].rhs), (yyvsp[0].rhs));
                  (yyval.rhs).rhs.msgstr = XNMALLOC ((yyvsp[-1].rhs).rhs.msgstr_len + (yyvsp[0].rhs).rhs.msgstr_len, char);
                  memcpy ((yyval.rhs).rhs.msgstr, (yyvsp[-1].rhs).rhs.msgstr, (yyvsp[-1].rhs).rhs.msgstr_len);
                  memcpy ((yyval.rhs).rhs.msgstr + (yyvsp[-1].rhs).rhs.msgstr_len, (yyvsp[0].rhs).rhs.msgstr, (yyvsp[0].rhs).rhs.msgstr_len);
                  (yyval.rhs).rhs.msgstr_len = (yyvsp[-1].rhs).rhs.msgstr_len + (yyvsp[0].rhs).rhs.msgstr_len;
                  free ((yyvsp[-1].rhs).rhs.msgstr);
                  free ((yyvsp[0].rhs).rhs.msgstr);
                  (yyval.rhs).pos = (yyvsp[-1].rhs).pos;
                  (yyval.rhs).obsolete = (yyvsp[-1].rhs).obsolete;
                }
#line 1507 "po-gram-gen.tab.c"
    break;

  case 26:
#line 348 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-4].pos), (yyvsp[-3].pos));
                  check_obsolete ((yyvsp[-4].pos), (yyvsp[-2].number));
                  check_obsolete ((yyvsp[-4].pos), (yyvsp[-1].pos));
                  check_obsolete ((yyvsp[-4].pos), (yyvsp[0].stringlist));
                  if ((yyvsp[-2].number).number != plural_counter)
                    {
                      if (plural_counter == 0)
                        po_gram_error_at_line (&(yyvsp[-4].pos).pos, _("first plural form has nonzero index"));
                      else
                        po_gram_error_at_line (&(yyvsp[-4].pos).pos, _("plural form has wrong index"));
                    }
                  plural_counter++;
                  (yyval.rhs).rhs.msgstr = string_list_concat_destroy (&(yyvsp[0].stringlist).stringlist);
                  (yyval.rhs).rhs.msgstr_len = strlen ((yyval.rhs).rhs.msgstr) + 1;
                  (yyval.rhs).pos = (yyvsp[-4].pos).pos;
                  (yyval.rhs).obsolete = (yyvsp[-4].pos).obsolete;
                }
#line 1530 "po-gram-gen.tab.c"
    break;

  case 27:
#line 371 "po-gram-gen.y"
                {
                  string_list_init (&(yyval.stringlist).stringlist);
                  string_list_append (&(yyval.stringlist).stringlist, (yyvsp[0].string).string);
                  free ((yyvsp[0].string).string);
                  (yyval.stringlist).pos = (yyvsp[0].string).pos;
                  (yyval.stringlist).obsolete = (yyvsp[0].string).obsolete;
                }
#line 1542 "po-gram-gen.tab.c"
    break;

  case 28:
#line 379 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-1].stringlist), (yyvsp[0].string));
                  (yyval.stringlist).stringlist = (yyvsp[-1].stringlist).stringlist;
                  string_list_append (&(yyval.stringlist).stringlist, (yyvsp[0].string).string);
                  free ((yyvsp[0].string).string);
                  (yyval.stringlist).pos = (yyvsp[-1].stringlist).pos;
                  (yyval.stringlist).obsolete = (yyvsp[-1].stringlist).obsolete;
                }
#line 1555 "po-gram-gen.tab.c"
    break;

  case 29:
#line 391 "po-gram-gen.y"
                {
                  string_list_init (&(yyval.stringlist).stringlist);
                  string_list_append (&(yyval.stringlist).stringlist, (yyvsp[0].string).string);
                  free ((yyvsp[0].string).string);
                  (yyval.stringlist).pos = (yyvsp[0].string).pos;
                  (yyval.stringlist).obsolete = (yyvsp[0].string).obsolete;
                }
#line 1567 "po-gram-gen.tab.c"
    break;

  case 30:
#line 399 "po-gram-gen.y"
                {
                  check_obsolete ((yyvsp[-1].stringlist), (yyvsp[0].string));
                  (yyval.stringlist).stringlist = (yyvsp[-1].stringlist).stringlist;
                  string_list_append (&(yyval.stringlist).stringlist, (yyvsp[0].string).string);
                  free ((yyvsp[0].string).string);
                  (yyval.stringlist).pos = (yyvsp[-1].stringlist).pos;
                  (yyval.stringlist).obsolete = (yyvsp[-1].stringlist).obsolete;
                }
#line 1580 "po-gram-gen.tab.c"
    break;


#line 1584 "po-gram-gen.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == PO_GRAM_EMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= PO_GRAM_EOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == PO_GRAM_EOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = PO_GRAM_EMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;


#if !defined yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif


/*-----------------------------------------------------.
| yyreturn -- parsing is finished, return the result.  |
`-----------------------------------------------------*/
yyreturn:
  if (yychar != PO_GRAM_EMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}


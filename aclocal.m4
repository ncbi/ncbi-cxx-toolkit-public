dnl ----------------------------------------------
dnl -----------------  find lib  -----------------
dnl ----------------------------------------------

AC_DEFUN(AC_FIND_LIB,[
   if eval "test \"`echo '$''{ac_cv_func_'$1'+set}'`\" = set"; then
     eval "unset '$''{'ac_cv_func_$1'}'"
   fi
   AC_CHECK_FUNCS($1)
   if eval "test \"`echo '$''{ac_cv_func_'$1'}'`\" = no"; then
     for lib in $2 ; do 
       AC_CHECK_LIB(${lib}, $1)
       ### break, if function is found in library
       eval "test \"`echo '$ac_cv_lib_'$ac_lib_var`\" = no" || break
     done
     eval "unset '$''{'ac_cv_func_$1'}'"
   fi
])

dnl ----------------------------------------------
dnl ----------------  prototypes  ----------------
dnl ----------------------------------------------

AC_DEFUN(AM_C_PROTOTYPES,
[AC_REQUIRE([AM_PROG_CC_STDC])
AC_BEFORE([$0], [AC_C_INLINE])
AC_MSG_CHECKING([for function prototypes])
if test "$am_cv_prog_cc_stdc" != no; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(PROTOTYPES)
  U= ANSI2KNR=
else
  AC_MSG_RESULT(no)
  U=_ ANSI2KNR=./ansi2knr
  # Ensure some checks needed by ansi2knr itself.
  AC_HEADER_STDC
  AC_CHECK_HEADERS(string.h)
fi
])


# serial 1

# @defmac AC_PROG_CC_STDC
# @maindex PROG_CC_STDC
# @ovindex CC
# If the C compiler in not in ANSI C mode by default, try to add an option
# to output variable @code{CC} to make it so.  This macro tries various
# options that select ANSI C on some system or another.  It considers the
# compiler to be in ANSI C mode if it defines @code{__STDC__} to 1 and
# handles function prototypes correctly.
#
# If you use this macro, you should check after calling it whether the C
# compiler has been set to accept ANSI C; if not, the shell variable
# @code{am_cv_prog_cc_stdc} is set to @samp{no}.  If you wrote your source
# code in ANSI C, you can make an un-ANSIfied copy of it by using the
# program @code{ansi2knr}, which comes with Ghostscript.
# @end defmac

AC_DEFUN(AM_PROG_CC_STDC,
[AC_REQUIRE([AC_PROG_CC])
AC_MSG_CHECKING(for ${CC-cc} option to accept ANSI C)
AC_CACHE_VAL(am_cv_prog_cc_stdc,
[am_cv_prog_cc_stdc=no
ac_save_CC="$CC"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX			-Aa -D_HPUX_SOURCE
# SVR4			-Xc -D__EXTENSIONS__
for ac_arg in "" -qlanglvl=ansi -std1 "-Aa -D_HPUX_SOURCE" "-Xc -D__EXTENSIONS__"
do
  CC="$ac_save_CC $ac_arg"
  AC_TRY_COMPILE(
[#if !defined(__STDC__) || __STDC__ != 1
choke me
#endif
/* DYNIX/ptx V4.1.3 can't compile sys/stat.h with -Xc -D__EXTENSIONS__. */
#ifdef _SEQUENT_
# include <sys/types.h>
# include <sys/stat.h>
#endif
], [
int test (int i, double x);
struct s1 {int (*f) (int a);};
struct s2 {int (*f) (double a);};],
[am_cv_prog_cc_stdc="$ac_arg"; break])
done
CC="$ac_save_CC"
])
AC_MSG_RESULT($am_cv_prog_cc_stdc)
case "x$am_cv_prog_cc_stdc" in
  x|xno) ;;
  *) CC="$CC $am_cv_prog_cc_stdc" ;;
esac
])

AC_DEFUN(AM_STRUCT_UTIMBUF,
[AC_MSG_CHECKING(for struct utimbuf)
if test x"$ac_cv_header_utime_h" = xyes; then
  AC_EGREP_CPP([struct +utimbuf],
    [#include <utime.h>],
    [AC_DEFINE(HAVE_STRUCT_UTIMBUF)
      AC_MSG_RESULT(yes)],
    AC_MSG_RESULT(no))
else
  AC_MSG_RESULT(no)
fi])


# Added a fix for C++ (see the "const charset x;" tests)
#
#
# By Denis Vakatov (vakatov@ncbi.nlm.nih.gov)

AC_DEFUN(AC_CXX_CONST,
[dnl This message is consistent in form with the other checking messages,
dnl and with the result message.
AC_CACHE_CHECK([for working const], ac_cv_c_const,
[AC_TRY_COMPILE(,
changequote(<<, >>)dnl
<<
/* Ultrix mips cc rejects this.  */
/* ...and maybe he is right(Denis Vakatov) :-) */
typedef int charset[2];
#ifdef __cplusplus
extern const charset x1;
const charset x2 = {1,2};
#else
const charset x;
#endif
/* SunOS 4.1.1 cc rejects this.  */
char const *const *ccp;
char **p;
/* NEC SVR4.0.2 mips cc rejects this.  */
struct point {int x, y;};
static struct point const zero = {0,0};
/* AIX XL C 1.02.0.0 rejects this.
   It does not let you subtract one const X* pointer from another in an arm
   of an if-expression whose if-part is not a constant expression */
const char *g = "string";
ccp = &g + (g ? g-g : 0);
/* HPUX 7.0 cc rejects these. */
++ccp;
p = (char**) ccp;
ccp = (char const *const *) p;
{ /* SCO 3.2v4 cc rejects this.  */
  char *t;
  char const *s = 0 ? (char *) 0 : (char const *) 0;

  *t++ = 0;
}
{ /* Someone thinks the Sun supposedly-ANSI compiler will reject this.  */
  int x[] = {25, 17};
  const int *foo = &x[0];
  ++foo;
}
{ /* Sun SC1.0 ANSI compiler rejects this -- but not the above. */
  typedef const int *iptr;
  iptr p = 0;
  ++p;
}
{ /* AIX XL C 1.02.0.0 rejects this saying
     "k.c", line 2.27: 1506-025 (S) Operand must be a modifiable lvalue. */
  struct s { int j; const int *ap[3]; };
  struct s *b; b->j = 5;
}
{ /* ULTRIX-32 V3.1 (Rev 9) vcc rejects this */
  const int foo = 10;
}
>>,
changequote([, ])dnl
ac_cv_c_const=yes, ac_cv_c_const=no)])
if test $ac_cv_c_const = no; then
  AC_DEFINE(const, )
fi
])


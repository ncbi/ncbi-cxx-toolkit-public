# Hacked up in various ways, since Autoconf's version doesn't quite
# suit our (unusual) conventions.  (Originally from status.m4)
m4_define([_AC_SRCPATHS],
[#ac_builddir=. # Useless!
ac_builddir=$builddir
dnl Base source directories on path to *input* file.
if test -n "$ac_file_in"; then
   ac_dir_in=`AS_DIRNAME(["$ac_file_in"])`
else
   ac_dir_in=$1
fi

if test $ac_dir_in != .; then
  ac_dir_suffix=/`echo $ac_dir_in | sed 's,^\.[[\\/]],,'`
  # A "../" for each directory in $ac_dir_suffix.
  ac_top_builddir=`echo "$ac_dir_suffix" | sed 's,/[[^\\/]]*,../,g'`
else
  ac_dir_suffix= ac_top_builddir=
fi

case $srcdir in
  .)  # No --srcdir option.  We are building in place.
    ac_srcdir=.
    if test -z "$ac_top_builddir"; then
       ac_top_srcdir=.
    else
       ac_top_srcdir=`echo $ac_top_builddir | sed 's,/$,,'`
    fi ;;
  [[\\/]]* | ?:[[\\/]]* )  # Absolute path.
    ac_srcdir=$srcdir$ac_dir_suffix;
    ac_top_srcdir=$srcdir ;;
  *) # Relative path.
    ac_srcdir=$ac_top_builddir$srcdir$ac_dir_suffix
    ac_top_srcdir=$ac_top_builddir$srcdir ;;
esac
# Do not use `cd foo && pwd` to compute absolute paths, because
# the directories may not exist.
AS_SET_CATFILE([ac_abs_builddir],   [`$smart_pwd`],     [$1])
AS_SET_CATFILE([ac_abs_top_builddir],
                                    [$ac_abs_builddir], [${ac_top_builddir}.])
AS_SET_CATFILE([ac_abs_top_srcdir], [$ac_dir_in],       [$real_srcdir])
AS_SET_CATFILE([ac_abs_srcdir],     [$ac_abs_top_srcdir], [$ac_dir_suffix])
])# _AC_SRCPATHS


# Copied from autoconf 2.59 (m4sh.m4), but rearranged to make bash a
# last resort due to issues with sourcing .bashrc.
m4_define([_AS_LINENO_PREPARE],
[_AS_LINENO_WORKS || {
  # Find who we are.  Look in the path if we contain no path at all
  # relative or not.
  case $[0] in
    *[[\\/]]* ) as_myself=$[0] ;;
    *) _AS_PATH_WALK([],
                   [test -r "$as_dir/$[0]" && as_myself=$as_dir/$[0] && break])
       ;;
  esac
  # We did not find ourselves, most probably we were run as `sh COMMAND'
  # in which case we are not to be found in the path.
  if test "x$as_myself" = x; then
    as_myself=$[0]
  fi
  if test ! -f "$as_myself"; then
    AS_ERROR([cannot find myself; rerun with an absolute path])
  fi
  case $CONFIG_SHELL in
  '')
    for as_base in sh ksh sh5 bash; do
      _AS_PATH_WALK([/bin$PATH_SEPARATOR/usr/bin$PATH_SEPARATOR$PATH],
         [case $as_dir in
         /*)
           if ("$as_dir/$as_base" -c '_AS_LINENO_WORKS') 2>/dev/null; then
             AS_UNSET(BASH_ENV)
             AS_UNSET(ENV)
             CONFIG_SHELL=$as_dir/$as_base
             export CONFIG_SHELL
             exec "$CONFIG_SHELL" "$[0]" ${1+"$[@]"}
           fi;;
         esac
       done]);;
  esac

  # Create $as_me.lineno as a copy of $as_myself, but with $LINENO
  # uniformly replaced by the line number.  The first 'sed' inserts a
  # line-number line before each line; the second 'sed' does the real
  # work.  The second script uses 'N' to pair each line-number line
  # with the numbered line, and appends trailing '-' during
  # substitution so that $LINENO is not a special case at line end.
  # (Raja R Harinath suggested sed '=', and Paul Eggert wrote the
  # second 'sed' script.  Blame Lee E. McMahon for sed's syntax.  :-)
  sed '=' <$as_myself |
    sed '
      N
      s,$,-,
      : loop
      s,^\([['$as_cr_digits']]*\)\(.*\)[[$]]LINENO\([[^'$as_cr_alnum'_]]\),\1\2\1\3,
      t loop
      s,-$,,
      s,^[['$as_cr_digits']]*\n,,
    ' >$as_me.lineno &&
  chmod +x $as_me.lineno ||
    AS_ERROR([cannot create $as_me.lineno; rerun with a POSIX shell])

  # Don't try to exec as it changes $[0], causing all sort of problems
  # (the dirname of $[0] is not the place where we might find the
  # original and so on.  Autoconf is especially sensible to this).
  . ./$as_me.lineno
  # Exit status is that of the last command.
  exit
}
])# _AS_LINENO_PREPARE


# Arguments:
# 1. library name (turned into environment/make variable)
# 2. values to check
# 3. function name
AC_DEFUN(NCBI_CHECK_LIBS,
[saved_LIBS=$LIBS
 AC_SEARCH_LIBS($3, ["[$]$1_LIBS" $2],
  [AC_DEFINE(HAVE_LIB$1, 1,
   [Define to 1 if $1 is available, either in its own library or as part
    of the standard libraries.])
   test "x$ac_cv_search_$3" = "xnone required" || $1_LIBS=$ac_cv_search_$3],
   [])
 LIBS=$saved_LIBS
])


# Arguments:
# 1. (3.) Properly-cased library name
# 2. (4.) Test code.
# 3. (5.) Extra libraries to put in $2_LIBS.
# 4. (6.) Extra libraries to require users to put in LIBS.
# 5. (7.) Extra include paths that should go into $2_INCLUDE.

AC_DEFUN(NCBI_CHECK_THIRD_PARTY_LIB,
[NCBI_CHECK_THIRD_PARTY_LIB_(m4_tolower($1), m4_toupper($1), $@)])

AC_DEFUN(NCBI_CHECK_THIRD_PARTY_LIB_,
[if test "$with_$1" != "no"; then
    case "$with_$1" in
       yes | "" ) ;;
       *        ) $2_PATH=$with_$1 ;;
    esac
    if test -d "[$]$2_PATH"; then
       in_path=" in [$]$2_PATH"
       test -d "[$]$2_PATH/include" && $2_INCLUDE="-I[$]$2_PATH/include"
       if test -d "[$]$2_PATH/lib${bit64_sfx}"; then
          $2_LIBPATH="-L[$]$2_PATH/lib${bit64_sfx} ${CONF_f_runpath}[$]$2_PATH/lib${bit64_sfx}"
       fi
       $2_LIBS="[$]$2_LIBPATH -l$3 $5"
    else
       $2_INCLUDE=""
       $2_LIBS="-l$3 $5"
       in_path=
    fi
    AC_CACHE_CHECK([for lib$3$in_path], ncbi_cv_lib_$1,
       CPPFLAGS="$7 [$]$2_INCLUDE $orig_CPPFLAGS"
       LIBS="[$]$2_LIBS $6 $orig_LIBS"
       [AC_LINK_IFELSE($4, [ncbi_cv_lib_$1=yes], [ncbi_cv_lib_$1=no])])
    if test "$ncbi_cv_lib_$1" = "no"; then
       with_$1="no"
    fi
 fi
 if test "$with_$1" = "no"; then
    $2_PATH="No_$2"
    $2_INCLUDE=
    $2_LIBS=
    WithoutPackages="$WithoutPackages $2"
 else
    WithPackages="$WithPackages $2"
    $2_INCLUDE="$7 [$]$2_INCLUDE"
    AC_DEFINE([HAVE_LIB]patsubst($2, [^LIB], []), 1,
              [Define to 1 if lib$3 is available.])
 fi
 AC_SUBST($2_INCLUDE)
 AC_SUBST($2_LIBS)
])

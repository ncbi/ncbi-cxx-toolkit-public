# autoconf 2.53's version of this breaks our (unorthodox) usage of
# CONFIG_FILES and certain substvars; substitute a tame version.
m4_define([_AC_SRCPATHS],
[#ac_builddir=. # Useless!

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
# Don't blindly perform a `cd $1/$ac_foo && pwd` since $ac_foo can be
# absolute.
ac_abs_builddir=`cd $1 && pwd`
ac_abs_top_builddir=`cd $1 && cd $ac_top_builddir && pwd`
ac_builddir=$ac_abs_top_builddir/build # Much more useful than "."!
ac_abs_srcdir=`cd $ac_dir_in && cd $ac_srcdir && pwd`
ac_abs_top_srcdir=`cd $ac_dir_in && cd $ac_top_srcdir && pwd`
])# _AC_SRCPATHS


# Copied from autoconf 2.53, but rearranged to make bash a last resort
# due to issues with sourcing .bashrc.
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
# 3. #include directives
# 4. code to compile
AC_DEFUN(NCBI_CHECK_LIBS,
[AC_MSG_CHECKING(if $1 library and functions are available)
 found=false
 for libs in "[$]$1_LIBS" $2; do
    [LIBS="$libs $orig_LIBS"]
    AC_TRY_LINK([$3], [$4], [found=true])
    if test $found = true -o -n "${$1_LIBS+set}"; then
       break
    fi
 done

case "$found" in
 false) AC_MSG_RESULT(no) ;;
 true)
    if test -n "$libs"; then
       AC_MSG_RESULT($libs)
    else
       AC_MSG_RESULT(in standard libraries)
    fi
    $1_LIBS=$libs
    AC_DEFINE(HAVE_LIB$1, 1,
              [Define to 1 if $1 is available, either in its own library
               or as part of the standard libraries.])
    ;;
esac
])

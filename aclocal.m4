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

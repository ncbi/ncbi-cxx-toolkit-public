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
  ac_dir_suffix=`echo $ac_dir_in | sed 's,^\.[[\\/]],,'`
  # A "../" for each directory in $ac_dir_suffix.
  ac_top_builddir=../`echo "$ac_dir_suffix" | sed 's,/[[^\\/]]*,../,g'`
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
    ac_srcdir=$srcdir/$ac_dir_suffix;
    ac_top_srcdir=$srcdir ;;
  *) # Relative path.
    ac_srcdir=$ac_top_builddir$srcdir/$ac_dir_suffix
    ac_top_srcdir=$ac_top_builddir$srcdir ;;
esac
# Do not use `cd foo && pwd` to compute absolute paths, because
# the directories may not exist.
AS_SET_CATFILE([ac_abs_builddir],   [$builddir],        [$1])
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


# One more hack: suppress PACKAGE_*, as we don't use them and some
# third-party libraries expose their corresponding settings, leading
# to preprocessor warnings.
m4_define([NCBI_ORIG_ACDU], m4_defn([AC_DEFINE_UNQUOTED]))
m4_define([AC_DEFINE_UNQUOTED],
   [ifelse(m4_substr([$1], 0, 8), [PACKAGE_], [],
       [NCBI_ORIG_ACDU($@)])])


AC_DEFUN(NCBI_FIX_DIR,
[ncbi_fix_dir_tmp=`if cd $[$1]; then AS_UNSET(PWD); /bin/pwd; fi`
 case "$ncbi_fix_dir_tmp" in
    /.*) ncbi_fix_dir_tmp2=`cd $[$1] && $smart_pwd 2>/dev/null`
         if test -n "$ncbi_fix_dir_tmp2" -a -d "$ncbi_fix_dir_tmp2"; then
            $1=$ncbi_fix_dir_tmp2
         else
            case "$[$1]" in
               /*) ;;
               * ) $1=$ncbi_fix_dir_tmp ;;
            esac
         fi
         ;;
    /*) $1=$ncbi_fix_dir_tmp ;;
 esac])


# Keep track of (un)available features, packages, and projects.
AC_DEFUN(NCBI_FEAT_EX,
         [m4_append_uniq([NCBI_ALL_]$1, $2, [ ])dnl
          With$1="[$]With$1[$]{With$1Sep}$2"; With$1Sep=" "])
AC_DEFUN(NCBI_FEATURE, [NCBI_FEAT_EX(Features, $1)])
AC_DEFUN(NCBI_PACKAGE, [NCBI_FEAT_EX(Packages, $1)])
AC_DEFUN(NCBI_PROJECT, [NCBI_FEAT_EX(Projects, $1)])


# Argument: question to ask user.
AC_DEFUN(NCBI_CAUTION,
[case "$with_caution" in
    yes )
       AC_MSG_ERROR([Configuration has been canceled per --with-caution.]) ;;
    no )
       AC_MSG_WARN([Proceeding without questions per --without-caution]) ;;
    * )
       echo "$1 [[y/N]]"
       read answer
       case "$answer" in
         [[Yy]]* )  AC_MSG_WARN([Proceeding at your own risk...]) ;;
         *       )  AC_MSG_ERROR([Configuration has been canceled by user.]) ;;
       esac
       ;;
 esac])


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
# 1. variable name
# 2. path(s)
# 3. suffix (optional)
dnl
dnl Naive implementation
dnl
dnl AC_DEFUN(NCBI_RPATHIFY,
dnl [$1="-L$2 ${CONF_f_runpath}$2$3"])
dnl 
dnl AC_DEFUN(NCBI_RPATHIFY_COND,
dnl [: ${$1="-L$2 ${CONF_f_runpath}$2$3"}])

AC_DEFUN(NCBI_RPATHIFY,
[ncbi_rp_L_flags=
 ncbi_rp_L_sep=$CONF_f_libpath
 if test "x${CONF_f_runpath}" = "x${CONF_f_libpath}"; then
    for x in $2; do
       ncbi_rp_L_flags="${ncbi_rp_L_flags}${ncbi_rp_L_sep}$x"
       ncbi_rp_L_sep=" $CONF_f_libpath"
    done
    $1="${ncbi_rp_L_flags}$3"
 else
    ncbi_rp_R_flags=
    ncbi_rp_R_sep=" $CONF_f_runpath"
    for x in $2; do
       ncbi_rp_L_flags="${ncbi_rp_L_flags}${ncbi_rp_L_sep}$x"
       ncbi_rp_L_sep=" $CONF_f_libpath"
       x=`echo $x | sed -e "$ncbi_rpath_sed"`
       ncbi_rp_R_flags="${ncbi_rp_R_flags}${ncbi_rp_R_sep}$x"
       ncbi_rp_R_sep=:
    done
    $1="${ncbi_rp_L_flags}${ncbi_rp_R_flags}$3"
 fi])

AC_DEFUN(NCBI_RPATHIFY_COND,
[if test -z "${$1+set}"; then
    NCBI_RPATHIFY(m4_translit($1, :), [$2], [$3])
 fi])

# Arguments:
# 1. variable name
# 2. command
# 3. extra sed code (optional)
dnl
dnl AC_DEFUN(NCBI_RPATHIFY_OUTPUT,
dnl [$1=`$2 | sed -e "$3s/-L\\([[^ ]]*\\)/-L\\1 ${CONF_f_runpath}\\1/"`])
dnl 
dnl AC_DEFUN(NCBI_RPATHIFY_OUTPUT_COND,
dnl [: ${$1=`$2 | sed -e "$3s/-L\\([[^ ]]*\\)/-L\\1 ${CONF_f_runpath}\\1/"`}])

AC_DEFUN(NCBI_RPATHIFY_OUTPUT,
[if test "x${CONF_f_runpath}" = "x${CONF_f_libpath}"; then
    $1=`$2 | sed -e "$3"`
 else
    $1=
    ncbi_rp_L_sep=
    ncbi_rp_R_flags=
    ncbi_rp_R_sep=" $CONF_f_runpath"
    for x in `$2 | sed -e "$3"`; do
       case "$x" in
          -L*)
             $1="[$]$1${ncbi_rp_L_sep}$x"
             x=`echo $x | sed -e "s/^-L//; $ncbi_rpath_sed"`
             ncbi_rp_R_flags="${ncbi_rp_R_flags}${ncbi_rp_R_sep}$x"
             ncbi_rp_R_sep=:
             ;;
          *)
             $1="[$]$1${ncbi_rp_R_flags}${ncbi_rp_L_sep}$x"
             ncbi_rp_R_flags=
             ncbi_rp_R_sep=" $CONF_f_runpath"
             ;;
       esac
       ncbi_rp_L_sep=" "
    done
    $1="[$]$1${ncbi_rp_R_flags}"
 fi])

AC_DEFUN(NCBI_RPATHIFY_OUTPUT_COND,
[if test -z "${$1+set}"; then
    NCBI_RPATHIFY_OUTPUT(m4_translit($1, :), [$2], [$3])
 fi])

# Arguments:
# 1. (3.) Properly-cased library name
# 2. (4.) Test code.
# 3. (5.) Extra libraries to put in $2_LIBS.
# 4. (6.) Extra libraries to require users to put in LIBS.
# 5. (7.) Extra include paths that should go into $2_INCLUDE.

AC_DEFUN(NCBI_CHECK_THIRD_PARTY_LIB,
[NCBI_CHECK_THIRD_PARTY_LIB_EX(m4_tolower($1), m4_toupper($1), $@)])

AC_DEFUN(NCBI_CHECK_THIRD_PARTY_LIB_EX,
[if test "$with_$1" != "no"; then
    case "$with_$1" in
       yes | "" ) ;;
       *        ) $2_PATH=$with_$1 ;;
    esac
    if test -d "[$]$2_PATH"; then
       in_path=" in [$]$2_PATH"
       if test -z "[$]$2_INCLUDE" -a -d "[$]$2_PATH/include"; then
          $2_INCLUDE="-I[$]$2_PATH/include"
       fi
       if test -n "[$]$2_LIBPATH"; then
          :
       elif test -d "[$]$2_PATH/lib${bit64_sfx}"; then
          NCBI_RPATHIFY($2_LIBPATH, [$]$2_PATH/lib${bit64_sfx}, [])
       elif test -d "[$]$2_PATH/lib"; then
          NCBI_RPATHIFY($2_LIBPATH, [$]$2_PATH/lib, [])
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
       NCBI_MISSING_PACKAGE($1)
    fi
 fi
 if test "$with_$1" = "no"; then
    $2_PATH="No_$2"
    $2_INCLUDE=
    $2_LIBS=
 else
    NCBI_PACKAGE($2)
    $2_INCLUDE="$7 [$]$2_INCLUDE"
    AC_DEFINE([HAVE_LIB]patsubst($2, [^LIB], []), 1,
              [Define to 1 if lib$3 is available.])
 fi
 AC_SUBST($2_INCLUDE)
 AC_SUBST($2_LIBS)
])

AC_DEFUN(NCBI_MISSING_PACKAGE,
   [if test "${[with_]m4_translit($1, [-], [_]):=no}" != no; then
       AC_MSG_ERROR([--with-$1 explicitly specified, but no usable version found.])
    fi])

AC_DEFUN(NCBI_CHECK_PYTHON,
[_NCBI_CHECK_PYTHON([PYTHON]patsubst($1, [\.], []), $@)])

AC_DEFUN(_NCBI_CHECK_PYTHON,
[AC_PATH_PROG($1, python$2, [],
    [$PYTHON_PATH/bin:$PATH:/usr/local/python-$2/bin])
 if test -x "[$]$1"; then
    $1_VERSION=`[$]$1 -c 'from distutils import sysconfig; print sysconfig.get_config_var("VERSION")' 2>/dev/null`
 else
    $1_VERSION=
    [ncbi_cv_lib_]m4_tolower($1)=no
 fi
 if test -n "[$]$1_VERSION"; then
    $1_INCLUDE=`[$]$1 -c 'from distutils import sysconfig; f=sysconfig.get_python_inc; print "-I%s -I%s" % (f(), f(True))'`
    $1_LIBPATH=`[$]$1 -c 'from distutils import sysconfig; print " ".join(sysconfig.get_config_vars("LIBDIR", "LIBPL"))'`
    $1_DEPS=`[$]$1 -c 'from distutils import sysconfig; print " ".join(sysconfig.get_config_vars("LIBS", "SYSLIBS"))'`
    NCBI_RPATHIFY($1_LIBS, [$]$1_LIBPATH, [ ]-lpython[$]$1_VERSION [$]$1_DEPS)
    CPPFLAGS="[$]$1_INCLUDE $orig_CPPFLAGS"
    LIBS="[$]$1_LIBS $orig_LIBS"
    AC_CACHE_CHECK([for usable Python [$]$1_VERSION libraries],
       [ncbi_cv_lib_]m4_tolower($1),
       [AC_LINK_IFELSE([AC_LANG_PROGRAM(
           [[
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <Python.h>]],
           [[Py_Initialize(); Py_Finalize();]])],
           [[ncbi_cv_lib_]m4_tolower($1)=yes],
           [[ncbi_cv_lib_]m4_tolower($1)=no])])
 else
    [ncbi_cv_lib_]m4_tolower($1)=no
 fi
 if test "[$ncbi_cv_lib_]m4_tolower($1)" = "no"; then
    $1_INCLUDE=
    $1_LIBS=
 else
    NCBI_PACKAGE($1)
    AC_DEFINE(HAVE_$1, 1, [Define to 1 if Python $2 libraries are available.])
 fi
 AC_SUBST($1_INCLUDE)
 AC_SUBST($1_LIBS)
])
 
AC_DEFUN(NCBI_LOCAL_FTDS,
[d="dbapi/driver/ftds$1/freetds"
      if test $try_local = yes -a -f "${real_srcdir}/src/$d/Makefile.in" ; then
         test "$with_ftds" = $1  &&  FTDS_PATH="<$d>"
         FTDS$1[_CTLIB_LIB]="ct_ftds$1${STATIC} tds_ftds$1${STATIC}"
         FTDS$1[_CTLIB_LIBS]='$(ICONV_LIBS) $(KRB5_LIBS)'
         FTDS$1[_CTLIB_INCLUDE]="-I\$(includedir)/$d -I\$(includedir0)/$d"
         freetds=freetds
      elif test -d "$FTDS_PATH" ; then
         FTDS$1[_CTLIB_LIB]=
         FTDS$1[_CTLIB_LIBS]=$FTDS_CTLIBS
         FTDS$1[_CTLIB_INCLUDE]=$FTDS_INCLUDE
      fi
      FTDS$1[_LIB]='$(FTDS$1[_CTLIB_LIB])'
      FTDS$1[_LIBS]='$(FTDS$1[_CTLIB_LIBS])'
      FTDS$1[_INCLUDE]='$(FTDS$1[_CTLIB_INCLUDE])'
])

AC_DEFUN(NCBI_CHECK_SUBTREE,
[if test "$with_$1" = "no" ; then
   NoConfProjects="$NoConfProjects $1"
fi

if test ! -f ${real_srcdir}/src/$1/Makefile.in  -o  \
        ! -d ${real_srcdir}/include/$1 ; then
   if test "${with_$1-no}" != "no" ; then
      AC_MSG_ERROR([--with-]$1[:  ]m4_toupper($1)[ sources are missing])
   fi
   with_$1="no"
fi
])

m4_include([ax_jni_include_dir.m4])

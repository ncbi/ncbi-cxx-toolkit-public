#############################################################################
#  $Id: configure.ac,v 1.34 2002/02/19 05:02:52 vakatov Exp $
#  Derived from configure.in version 1.173.
# ==========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ==========================================================================
#
# Authors:  Denis Vakatov, Aaron Ucko
#
# File Description:
#   Generate platform-dependent header("ncbiconf.h") for NCBI C++ libs
#   USAGE:
#    1) Process this file with "autoconf" to produce a "configure" script.
#    2) Run the resultant "configure" script to produce
#      a) "<builddir>/inc/ncbiconf.h"
#      b) "<builddir>/build/Makefile.mk"
#      c) other "Makefile"s and scripts in "<builddir>/build"
#
#############################################################################

dnl Set up lock.  The diversion magic lets this occur before AC_INIT, which
dnl already interferes with other configure processes.

AC_DIVERT_PUSH(INIT_PREPARE)
if $srcdir/scripts/get_lock.sh configure $$; then
    ac_clean_files=configure.lock
else
    exit 1
fi
cache_file=config.cache
# reconfigure.sh expects it to exist; unconditionally clobber
# command-line setting.
AC_DIVERT_POP

AC_INIT(ncbi-tools++, 0.0, cpp-core@ncbi.nlm.nih.gov)
AC_CONFIG_SRCDIR(src/Makefile.mk.in)



#############################################################################


#### Describe all "--with-*" arguments -- for the usage printout on "--help"

## Title
AC_ARG_WITH(title0,
   [===============================================================================])
AC_ARG_WITH(title1,
   [=============== NCBI C++ Toolkit specific configuration flags =================])
AC_ARG_WITH(title2,
   [===============================================================================])

## Code generation
AC_ARG_WITH(debug,
   [ --without-debug         build non-debug versions of libs and apps])
AC_ARG_WITH(optimization,
   [ --without-optimization  turn off optimization flags in non-debug mode])
AC_ARG_WITH(dll,
   [ --with-dll              build all libraries as DLLs])
AC_ARG_WITH(static,
   [ --with-static           build all libraries statically even if --with-dll])
AC_ARG_WITH(mt,
   [ --with-mt               compile in a MultiThread-safe manner])
AC_ARG_WITH(64,
   [ --with-64               compile to 64-bit code])
AC_ARG_WITH(exe,
   [ --without-exe           do not build executables])
AC_ARG_WITH(runpath,
   [ --with-runpath=         hard-code the runtime path to DLLs])

## Scripts, make features, and directory naming
AC_ARG_WITH(extra-action,
   [ --with-extra-action=    script to call after the configuration is complete])
AC_ARG_WITH(autodep,
   [ --with-autodep          automatic generation of dependencies (GNU make)])
AC_ARG_WITH(build-root,
   [ --with-build-root=DIR   specify a non-default build directory name])
AC_ARG_WITH(suffix,
   [ --without-suffix        no Release/Debug, MT or DLL sfx in the build dir name])
AC_ARG_WITH(hostspec,
   [ --with-hostspec         add full host specs to the build dir name])
AC_ARG_WITH(execopy,
   [ --without-execopy       do not copy built executables to the BIN area])
AC_ARG_WITH(bincopy,
   [ --without-bincopy       no check if libs or apps need to be copied to BIN])
AC_ARG_WITH(lib-rebuilds,
   [ --with-lib-rebuilds     ensure that apps use up-to-date libraries])
AC_ARG_WITH(lib-rebuilds2,
   [ --with-lib-rebuilds=ask ask whether to update each app's libraries])dnl'
AC_ARG_WITH(projects,
   [ --with-projects=FILE    build projects listed in FILE by default])
AC_ARG_WITH(check,
   [ --with-check=           run test suite after the build])

## NCBI packages
AC_ARG_WITH(ncbi-c,
   [ --without-ncbi-c        do not use NCBI C Toolkit])
AC_ARG_WITH(sss,
   [ --without-sss           do not use NCBI SSS libraries])
AC_ARG_WITH(sssutils,
   [ --without-utils         do not use NCBI SSS UTIL library])
AC_ARG_WITH(sssdb,
   [ --without-sssdb         do not use NCBI SSS DB library])
AC_ARG_WITH(geo,
   [ --without-geo           do not use NCBI GEO libraries])
AC_ARG_WITH(pubmed,
   [ --without-pubmed        do not use NCBI PubMed libraries])

## Third-party and system packages
AC_ARG_WITH(sybase,
   [ --without-sybase        do not use SYBASE libraries])
AC_ARG_WITH(sybase-local,
   [ --with-sybase-local=DIR use local SYBASE install (DIR is optional)])
AC_ARG_WITH(sybase-new,
   [ --with-sybase-new       use newer SYBASE install (12.5 rather than 12.0)])
AC_ARG_WITH(ftds,
   [ --without-ftds          do not use FreeTDS libraries])
AC_ARG_WITH(wxwin,
   [ --without-wxwin         do not use wxWindows])
AC_ARG_WITH(wxwin2,
   [ --with-wxwin=DIR        use wxWindows installation in DIR])
AC_ARG_WITH(fastcgi,
   [ --without-fastcgi       do not use Fast-CGI library])
AC_ARG_WITH(sp,
   [ --without-sp            do not use SP libraries])
AC_ARG_WITH(orbacus,
   [ --without-orbacus       do not use ORBacus CORBA libraries])
AC_ARG_WITH(orbacus2,
   [ --with-orbacus=DIR      use ORBacus installation in DIR])

## Optional projects
AC_ARG_WITH(local-lbsm,
   [ --without-local-lbsm    turn off support for IPC with locally running LBSMD])
AC_ARG_WITH(serial,
   [ --without-serial        do not build the serialization library and tools])
AC_ARG_WITH(objects,
   [ --with-objects          generate/build serializeable objects from ASNs])
AC_ARG_WITH(dbapi,
   [ --without-dbapi         do not build NCBI portable database libraries])
AC_ARG_WITH(app,
   [ --with-app              build standalone applications like ID1_FETCH])
AC_ARG_WITH(ctools,
   [ --without-ctools        do not build NCBI C Toolkit based projects])
AC_ARG_WITH(gui,
   [ --without-gui           do not build wxWindows based projects])
AC_ARG_WITH(internal,
   [ --with-internal         build all internal projects])

## Manuals
AC_ARG_WITH(manuals,
   [***** See also HTML documentation in ./doc/index.html *****])


#### Check the passed arguments against the list of available ones
x_with_list="\
debug optimization dll static runpath mt 64 \
exe autodep suffix hostspec execopy bincopy lib-rebuilds lib-rebuilds=ask \
ncbi-c wxwin fastcgi sss sssdb sssutils geo sp pubmed sybase sybase-local \
sybase-new ftds orbacus \
local-lbsm serial objects dbapi app ctools gui internal"

changequote(, )dnl
x_with_list=`echo "$x_with_list" | sed 's/\([^ ][^ ]*\)/--with-\1 --without-\1/g'`
changequote([, ])dnl


for x_arg in "$@" ; do
   if test -z "$x_arg" ; then
      continue
   fi

   x_okay=
   for x_desc in $x_with_list ; do
      if test "$x_arg" = "$x_desc" ; then
         x_okay="okay"
         break
      fi
   done
   if test "$x_okay" = "okay" ; then
      continue
   fi

   case "$x_arg" in
      --with-extra-action= | --exec-prefix= \
      | --cache-file= | --build= | --host= | --target= | --with-runpath= )
      AC_MSG_ERROR(["  $x_arg:  requires value;  use --help to show usage"])
      ;;

      --with-extra-action=* | --exec-prefix=* | --with-build-root=* \
      | --with-projects=* | --with-check=* \
      | --cache-file=* | --build=* | --host=* \
      | [[A-Z]*=*] \
      | --with-sybase-local=* | --with-wxwin=* | --with-orbacus=* \
      | --target=* | --with-runpath=* | --no-create | --no-recursion)
      ;;

      * )
      AC_MSG_ERROR(["  $x_arg:  unknown option;  use --help to show usage"])
      ;;
   esac
done


#### Check for special options
if test "$with_extra_action" = "yes" ; then
   AC_MSG_ERROR(["--with-extra-action must have a value after ="])
fi
if test "$with_extra_action" = "no" ; then
   AC_MSG_ERROR(["--without-extra-action is not allowed"])
fi


#### Always define this
AC_DEFINE_UNQUOTED(NCBI_CXX_TOOLKIT, 1)


#### Get the running host's properties
AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED(HOST,        "$host")
AC_DEFINE_UNQUOTED(HOST_CPU,    "$host_cpu")
AC_DEFINE_UNQUOTED(HOST_VENDOR, "$host_vendor")
AC_DEFINE_UNQUOTED(HOST_OS,     "$host_os")


#### C and C++ compilers
AC_LANG(C++)
AC_PROG_CC
AC_PROG_CXX


#### Detect KCC compiler
if test "$GCC" != "yes" ; then
   touch kcc_test.cpp
   kcc_ver="`$CXX -V -c kcc_test.cpp 2>&1 | grep '^KAI '`"
   kcc_license="`$CXX -V -c kcc_test.cpp 2>&1 | grep -v '^KAI '`"
   rm -f kcc_test.cpp kcc_test.o
   case "$kcc_ver" in 
      KAI* )  KCC="yes" ;;
   esac
fi


#### Detect ICC compiler
if test "$GCC" != "yes"  -a  "$KCC" != "yes"; then
   icc_ver="`$CXX -V 2>&1 | grep '^Intel(R) C++ Compiler'`"
   icc_license="`$CXX -c 2>&1 | grep '^icc: NOTE: The evaluation period for this product ends on '`"
   case "$icc_ver" in 
      Intel*C*Compiler* )  ICC="yes" ;;
   esac
fi


#### Detect IBM VisualAge compiler
if test "$GCC" != "yes"  -a  "$KCC" != "yes"  -a  "$ICC" != "yes"; then
   # Crude, but there doesn't seem to be a better alternative
   vac_ver="`$CXX 2>&1 | grep 'AIX Compiler.*Version'`"
   case "$vac_ver" in
      *AIX* ) VAC="yes" ;;
   esac
fi


#### Detect supported compilers
changequote(, )dnl
compiler=
compiler_ver=
ncbi_compiler="UNKNOWN"
ncbi_compiler_ver="0"

if test "$GCC" = "yes" ; then
   compiler_ver="`$CXX --version 2>&1`"
   case "$compiler_ver" in
     2.95* | 3.* )
       compiler="GCC"
       ncbi_compiler="GCC"
       ncbi_compiler_ver="$compiler_ver" ;;
   esac
elif test "$KCC" = "yes" ; then
   compiler_ver="$kcc_ver"
   compiler="KCC"
   ncbi_compiler="KCC"
   ncbi_compiler_ver="`echo $kcc_ver | sed 's%KAI C\+\+ \([0-9.]*\).*%\1%'`"
elif test "$ICC" = "yes" ; then
   compiler_ver="$icc_ver"
   compiler="ICC"
   ncbi_compiler="ICC"
   ncbi_compiler_ver="`echo $icc_ver | sed 's%.*Version \([0-9.]*\).*%\1%'`"
elif test "$VAC" = "yes" ; then
   compiler_ver="$vac_ver"
   compiler="VisualAge"
   ncbi_compiler="VISUALAGE"
   ncbi_compiler_ver="`echo $vac_ver | sed 's%.*Version \([0-9.]*\).*%\1%'`"
else
   case "$host_os" in
    solaris* )
      compiler_ver="`$CXX -V 2>&1`"
      case "$compiler_ver" in
        *WorkShop*Compilers*5\.0 )
          compiler="WorkShop5"
          ncbi_compiler="WORKSHOP"
          ncbi_compiler_ver="5.0" ;;
        *Sun*WorkShop*6*5\.1* )
          compiler="WorkShop6"
          ncbi_compiler="WORKSHOP"
          ncbi_compiler_ver="5.1" ;;
        *Sun*WorkShop*6*5\.2* )
          compiler="WorkShop52"
          ncbi_compiler="WORKSHOP"
          ncbi_compiler_ver="5.2" ;;
        *Sun*WorkShop*6*5\.3* )
          compiler="WorkShop53"
          ncbi_compiler="WORKSHOP"
          ncbi_compiler_ver="5.3" ;;
      esac ;;

    irix* )
      compiler_ver="`$CXX -version 2>&1`"
      case "$compiler_ver" in
        MIPSpro*Compilers:*Version*7\.3* )
          compiler="MIPSpro73"
          ncbi_compiler="MIPSPRO"
          ncbi_compiler_ver="7.3" ;;
      esac ;;

    cygwin32 )
      case "$CXX" in
        *cl\.exe* | cl)
          compiler="MSVC" 
          ncbi_compiler="MSVC"
          ncbi_compiler_ver="6.0" ;;
      esac ;;
   esac
fi

ncbi_compiler_ver="`echo $ncbi_compiler_ver | sed 's%\([0-9]\)\.\([0-9][0-9]*\).*%\1\2%; s%^\([0-9][0-9]\)$%\10%; s%^\([0-9]\)$%\100%'`"
compiler_version=$ncbi_compiler_ver
changequote([, ])dnl

AC_DEFINE_UNQUOTED(NCBI_COMPILER_$ncbi_compiler, 1)
AC_DEFINE_UNQUOTED(NCBI_COMPILER_VERSION, $ncbi_compiler_ver)


#### Additional check for pre-defined compilers
case "$compiler" in
  MSVC )
    if test -z "$NCBI_COMPILER" ; then
       echo
       echo "Compiler \"${compiler}\" requires a special tuning, so you"
       echo "better use a special shell script located in \"compilers/*.sh\"!"
changequote(, )dnl
       echo "Do you still want to proceed (at your own risk)? [n/y]"
changequote([, ])dnl
       read answer
       case "$answer" in
         y )  AC_MSG_WARN([Proceeding at your own risk...]) ; echo ;;
         * )  AC_MSG_WARN([Configuration has been canceled by user.]) ; exit ;;
       esac
       echo
    fi ;;
esac


#### Use full path to the compiler, if possible
changequote(, )dnl

set $CC
x_CC="`which $1  2>/dev/null`"
if test ! -x "$x_CC" ; then
  x_CC="`type $1 | sed 's/.* \([^ ]*\)$/\1/'`"
fi

if test -x "$x_CC" ; then
  shift
  CC="$x_CC $*"
  echo "adjusted C   compiler: $CC"
fi
CC_PATH="`dirname $x_CC`"
x_CC=

set $CXX
x_CXX="`which $1  2>/dev/null`"
if test ! -x "$x_CXX" ; then
  x_CXX="`type $1 | sed 's/.* \([^ ]*\)$/\1/'`"
fi

if test -x "$x_CXX" ; then
  shift
  CXX="$x_CXX $*"
  echo "adjusted C++ compiler: $CXX"
fi
CXX_PATH="`dirname $x_CXX`"
x_CXX=

### Which of these is better?
#compiler_root=`dirname CXX_PATH`
compiler_root=`echo $CXX | sed -ne 's:\(.*\)[/\\]bin[/\\].*:\1:p'`

changequote([, ])dnl


#### Multi-Thread safety
with_mt=${with_mt:="no"}
if test "$with_mt" = "yes" ; then
   case "$host_os:$compiler" in
    solaris*:WorkShop*              )  MT_FLAG=${MT_FLAG:="-mt"} ;;
    *:KCC                           )  MT_FLAG=${MT_FLAG:="--thread_safe"} ;;
    *:GCC | *:ICC | irix*:MIPSpro73 )  ;;
    * )   AC_MSG_ERROR(["Do not know how to build MT-safe with compiler $CXX $compiler_ver"]) ;;
   esac
   CPPFLAGS="$CPPFLAGS -D_MT -D_REENTRANT"
   case "$host_os" in
     solaris* )  THREAD_LIBS=${THREAD_LIBS:="-lthread -lpthread -lposix4"} ;;
     *        )  THREAD_LIBS=${THREAD_LIBS:="-lpthread"} ;;
   esac
   LIBS="$LIBS $THREAD_LIBS"
   mt_sfx="MT"
   WithFeatures="$WithFeatures MT"
else
   THREAD_LIBS=
   mt_sfx=""
fi

CFLAGS="$MT_FLAG $CFLAGS"
CXXFLAGS="$MT_FLAG $CXXFLAGS"
LDFLAGS="$MT_FLAG $LDFLAGS"


#### Provide default environment setup for known platforms/compilers
DEPFLAGS="-M"
DEPFLAGS_POST="" # Needed for VisualAge
case "$host_os:$compiler" in
 solaris2\.6:GCC )
   echo "GCC compiler is not supported for:  $host"
   compiler=
   ;;

 solaris*:GCC )
   STRIP=":"
   ;;

 linux* | osf*:GCC )
   NETWORK_LIBS=${NETWORK_LIBS:=" "}
   ;;

 irix*:GCC )
   if test "$with_debug" != "no" ; then
      LDFLAGS="-LD_LAYOUT:lgot_buffer=32 $LDFLAGS"
   fi
   ;;

 solaris*:WorkShop* )
   with_bincopy=${with_bincopy:="no"}
   CXXFLAGS="+w +w2 $CXXFLAGS"
   DEPFLAGS="-xM1"
   LDFLAGS="-xildoff $LDFLAGS"
   AR=${AR:="$CXX $MT_FLAG -xar -o"}
   STRIP=":"
   if test "$with_debug" = "no" ; then
      LDFLAGS="-s $LDFLAGS"
   fi
   RANLIB=":"
   CXX_FILTER="2>&1 | \$(top_srcdir)/compilers/cxx_filter.$compiler.sh"
   serial_ws50_rtti_kludge="rtti"
   ;;

 solaris*:KCC )
   with_bincopy=${with_bincopy:="no"}
   ;;

 irix*:MIPSpro73 )
   COMMON_FLAGS="-fullwarn -brief_diagnostics -use_readonly_const -G0 -rdata_shared"
   CFLAGS="$COMMON_FLAGS $CFLAGS"
   CXXFLAGS="$COMMON_FLAGS -ansiW -ptused -LANG:std -LANG:ansi-for-init-scope -woff 1460,1521,1429,1169,1209,1107,1424 $CXXFLAGS"
   CXXCPP=${CXXCPP:="$CXX -E -LANG:std"}
   LDFLAGS="-LANG:std $LDFLAGS"
   CPPFLAGS="-D__LONGLONG $CPPFLAGS"
   NETWORK_LIBS=" "
   ;;

 cygwin32:GCC )
   with_dll="no"
   NETWORK_LIBS=${NETWORK_LIBS:="-lwsock32"}
   CONF_exe_ext=".exe"
   ;;

 cygwin32:MSVC )
   with_dll="no"
   CFLAGS="/W4 /Od /Zi /DEBUG /D_DEBUG"
   CXXFLAGS="/W4 /Od /Zi /DEBUG /D_DEBUG"
   CPPFLAGS="/nologo $CPPFLAGS"
   LDFLAGS="/DEBUG"
   AR="lib.exe"
   RANLIB=":"
   STRIP=":"
   CONF_obj_ext=".obj"
   CONF_lib_ext=".lib"
   CONF_dll_ext=".dll"
   CONF_lib_l_ext=".lib"
   CONF_lib_pre=" "
   CONF_lib_l_pre=" "
   CONF_exe_ext=".exe"
   CONF_f_compile="/c "
   CONF_f_outobj="/Fo"
   CONF_f_outlib="/OUT:"
   CONF_f_libpath="/LIBPATH:"
   CONF_f_outexe="/OUT:"
   script_shell=""
   make_shell=""
   ;;

 aix*:VisualAge)
   CXXFLAGS="-qrtti=all $CXXFLAGS"
   DEPFLAGS="-E -M"
   DEPFLAGS_POST='>/dev/null 2>&1; (cat $*.u; rm -f $*.u)'
   NETWORK_LIBS="-lnsl"
   ;;

 aix*)
   NETWORK_LIBS="-lnsl"
   ;;
esac


#### Set makefile variables for platform-specific includes
changequote(, )dnl
COMPILER=`echo $ncbi_compiler | tr '[A-Z]' '[a-z]'`
OSTYPE=`echo $host_os | sed -e 's/[0-9.]*$//; s/-gnu$//; y/A-Z/a-z/'`
changequote([, ])dnl


#### Check if the compiler matches one of supported compilers on this platform
if test -z "$compiler" ; then
changequote(, )dnl
   cat <<EOF

  Compiler $CXX $compiler_ver is not fully supported.
  Consult doc/config.html#ref_Compilers for the list of
  fully supported platforms/compilers.
  Do you still want to proceed (at your own risk)? [n/y]
EOF
changequote([, ])dnl
   read answer
   case "$answer" in
      y )  AC_MSG_WARN([Proceeding at your own risk...]) ; echo ;;
      * )  AC_MSG_WARN([Configuration has been canceled by user.]) ; exit ;;
   esac
   compiler="UNKNOWN"
fi


#### Set compiler prefix for non-native compilers
case "$host_os:$compiler" in
   linux*:GCC )
      ;;
   *:GCC | *:KCC | *:ICC )
      compiler_pfx="${compiler}-" ;;
esac


#### 32/64-bit compilation, and extra LIBS
with_64=${with_64:="no"}
if test "$with_64" = "yes" ; then
   bit64_sfx="64"
   case "$host:$compiler" in
    sparc-sun-solaris*:WorkShop5 | sparc-sun-solaris*:KCC )
      ARCH_CFLAGS="-xtarget=ultra -xarch=v9"
      ;;
    sparc-sun-solaris*:WorkShop6 )
      ARCH_CFLAGS="-xtarget=ultra -xarch=v9"
      LIBS="-Bstatic -L$CXX_PATH/../lib -lCrun -Bdynamic $LIBS"
      ;;
changequote(, )dnl
    sparc-sun-solaris*:WorkShop5[23] )
      ARCH_CFLAGS="-xtarget=ultra -xarch=v9"
      ;;
changequote([, ])dnl
    mips-sgi-irix*:MIPSpro73 )
      ARCH_CFLAGS="-64"
      CPPFLAGS="$ARCH_CFLAGS $CPPFLAGS"
      AR=${AR:="$CXX $ARCH_CFLAGS -ar -WR,-v -o"}
      ;;
    mips-sgi-irix*:KCC )
      ARCH_CFLAGS="-64"
      ;;
    * )
      AC_MSG_ERROR(["Do not know how to compile 64-bit with compiler $CXX $compiler_ver $host:$compiler"])
      ;;
   esac
else
   bit64_sfx=
   ARCH_CFLAGS=
   case "$host_os:$compiler" in
    solaris*:WorkShop5 )
      LIBS="-Bstatic -L$CXX_PATH/../SC5.0/lib -lm -Bdynamic $LIBS"
      MATH_LIBS=" "
      ;;
    solaris*:WorkShop6 )
      LIBS="-Bstatic -L$CXX_PATH/../lib -lCrun -lm -Bdynamic $LIBS"
      MATH_LIBS=" "
      ;;
changequote(, )dnl
    solaris*:WorkShop5[23] )
      MATH_LIBS=" "
      ;;
changequote([, ])dnl
    irix*:MIPSpro73 | irix*:KCC )
      ARCH_CFLAGS="-n32"
      ;;
   esac
fi

CFLAGS="$ARCH_CFLAGS $CFLAGS"
CXXFLAGS="$ARCH_CFLAGS $CXXFLAGS"
LDFLAGS="$ARCH_CFLAGS $LDFLAGS"


#### Flags to enable (potentially unsafe) extra optimization.
if test -z "$DEF_FAST_FLAGS"  -o  -z "$FAST_CXXFLAGS" ; then
   case "$compiler" in
      GCC )
        DEF_FAST_FLAGS="-O9 -fomit-frame-pointer -finline-functions"
        DEF_FAST_FLAGS="$DEF_FAST_FLAGS -ffast-math -fstrict-aliasing"
        ;;
      WorkShop* )
        # Include ARCH_CFLAGS here because -fast implies -xtarget=native.
        DEF_FAST_FLAGS="-fast"
        ;;
      KCC )
        DEF_FAST_FLAGS="+K3"
        case "$host_os" in
           solaris* )  DEF_FAST_FLAGS="$DEF_FAST_FLAGS -fast" ;;
        esac
        ;;
      ICC )
        DEF_FAST_FLAGS="-O3 -axW -ip"
        ;;
      MIPSpro* )
        DEF_FAST_FLAGS="-O3 -Ofast"
        ;;
      * )
        DEF_FAST_FLAGS="-O"
        ;;
   esac
else
   DEF_FAST_FLAGS="-O"
fi

: ${FAST_CFLAGS="$DEF_FAST_FLAGS"}
: ${FAST_CXXFLAGS="$DEF_FAST_FLAGS"}


#### DLLs

# Generate position-independent code (PIC)
if test -z "$CFLAGS_DLL"  -a  -z "$CXXFLAGS_DLL" ; then
   case "$host_os:$compiler" in
    *:GCC | linux*:KCC )
      CFLAGS_DLL="-fPIC"
      CXXFLAGS_DLL="-fPIC"
      ;;
    solaris*:WorkShop* | irix*:MIPSpro73 | \
    solaris*:KCC | irix*:KCC | linux*:ICC)
      CFLAGS_DLL="-KPIC"
      CXXFLAGS_DLL="-KPIC"
      ;;
   esac
fi

# Provide runpath to the C++ Toolkit (or as specified by --with-runpath=) DLLs
if test -z "$CONF_f_runpath" ; then
   case "$host_os:$compiler" in
    solaris*:WorkShop* | solaris*:KCC )
      CONF_f_runpath="-R"
      ;;
    solaris*:GCC )
      CONF_f_runpath="-Xlinker -R -Xlinker "
      ;;
    linux*:GCC | linux*:ICC )
      CONF_f_runpath="-Xlinker -rpath -Xlinker "
      ;;
    irix*:* | linux*:KCC )
      CONF_f_runpath="-rpath "
      ;;
    * )
      CONF_f_runpath=${CONF_f_runpath:=""}
      ;;
   esac
fi

if   test -z "$CONF_f_runpath"  -o  "$with_runpath" = "no" ; then
   runpath=
   pure_runpath=
elif test -n "$with_runpath"  -a  "$with_runpath" != "yes" ; then
   runpath="${CONF_f_runpath}${with_runpath}"
   pure_runpath=$with_runpath
elif test "$with_runpath" = "yes"  -o  "$with_dll" != "no" ; then
   runpath="${CONF_f_runpath}\$(libdir)"
   pure_runpath='${build_root}/lib'
else
   runpath=
   pure_runpath=
fi


# Whether to build DLLs or static (and whether by default, or forcibly)
LIB_OR_DLL="lib"
if test "$with_dll" = "yes" ; then
   case "$host_os:$compiler" in
    linux*:GCC | irix*:* | solaris*:WorkShop* | *:KCC | linux*:ICC \
    | solaris*:GCC )
      ;;
    * )
      echo
      echo "DLLs may not be buildable by $CXX $compiler_ver on $host!"
changequote(, )dnl
      echo "Do you still want to try build DLLs (at your own risk)? [y/n]"
changequote([, ])dnl
      read answer
      case "$answer" in
         y | Y | yes | YES ) ;;
         * )  AC_MSG_WARN([Configuration has been canceled by user.]) ; exit ;;
      esac
      echo
      ;;
   esac
fi

case "$with_dll:$with_static" in
   yes:yes) LIB_OR_DLL="both"; dll_sfx="DLL+static" ;;
   yes:*)   LIB_OR_DLL="dll";  dll_sfx="DLL" ;;
   *:no)    AC_MSG_ERROR([--without-static requires --with-dll]) ;;
   *)       LIB_OR_DLL="lib";  dll_sfx=""  ;;
esac
  
if test "$with_dll" = "no"; then # no DLLs whatsoever!
   FORCE_STATIC_LIB="LIB_OR_DLL = lib"
else
   FORCE_STATIC_LIB=
fi

if test "$LIB_OR_DLL" = "both"; then
   STATIC="-static"
else
   STATIC=
fi
AC_SUBST(STATIC)

# Linker command-line to build DLL
case "$compiler" in
 GCC | MIPSpro73 | ICC )
   case "$host_os" in
      solaris* )
        # See http://realm.progsoc.uts.edu.au/~subtle/solaris28_link.html
        LINK_DLL="$CXX $ARCH_CFLAGS -shared -mimpure-text -o"
        ;;
      * )
        LINK_DLL="$CXX $ARCH_CFLAGS -shared -o"
        ;;
   esac
   ;;
 WorkShop* )
   LINK_DLL="$CXX $ARCH_CFLAGS $MT_FLAG -G -h \$@ -o"
   ;;
 KCC )
   LINK_DLL="$CXX $ARCH_CFLAGS $MT_FLAG --soname \$@ -o"
   ;;
 * )
   if test "$with_dll" = "yes"  &&  test -z "$LINK_DLL" ; then
      AC_MSG_ERROR(["Do not know how to build DLLs with compiler $CXX $compiler_ver"])
   fi
   ;;
esac


#### GNU compiler::  common flags and definitions
if test "$compiler" = "GCC" ; then
   CFLAGS="-Wall $CFLAGS"
   CXXFLAGS="-Wall $CXXFLAGS"
   case "$host_os" in
      solaris* )  CPPFLAGS="-D__EXTENSIONS__ $CPPFLAGS" ;;
      irix*    )  CXXFLAGS="$CXXFLAGS -D_LANGUAGE_C_PLUS_PLUS" ;;
   esac
fi


#### KAI compiler::  common flags and definitions
if test "$compiler" = "KCC" ; then
   AR=${AR:="$CXX $MT_FLAG -o"}
   CFLAGS="--display_error_number --c $CFLAGS"
   CXXFLAGS="--display_error_number --one_instantiation_per_object $CXXFLAGS"
   case "$host_os" in
      solaris* )  CPPFLAGS="-D__EXTENSIONS__ -D__STDC__=0 $CPPFLAGS" ;;
   esac
   if test -n "$kcc_license" ; then
      kcc_CC="$CC"
      kcc_CXX="$CXX"
      CC="./compilers/cxx_filter.KCC.sh $CC"
      CXX="./compilers/cxx_filter.KCC.sh $CXX"
   fi
fi


#### Intel compiler::  common flags and definitions
if test "$compiler" = "ICC" ; then
   if test -n "$icc_license" ; then
      icc_CC="$CC"
      icc_CXX="$CXX"
      CC="./compilers/cxx_filter.ICC.sh $CC"
      CXX="./compilers/cxx_filter.ICC.sh $CXX"
   fi
fi


#### Automatic generation of dependencies for/by the "make" utility
case "$host_os" in
   solaris* )  KeepStateTarget=".KEEP_STATE:" ;;
esac

if test "$with_autodep" = "yes" ; then
   Rules="rules_with_autodep"
elif test "$with_autodep" = "no" ; then
   Rules="rules"
elif make -v 2>/dev/null | grep 'GNU Make' ; then
   Rules="rules_with_autodep"
else
   Rules="rules"
fi


#### Tools
AC_PROG_LN_S
AC_PROG_RANLIB
AC_CHECK_PROG(AR, ar, ar cr, ErrorCannotFind_AR)
AC_CHECK_PROG(STRIP, strip, strip, :)
case "$host_os" in
   irix* )
     # lorder and tsort exist but don't work.
     LORDER=echo
     TSORT=cat
     ;;
   * )
     AC_CHECK_PROG(LORDER, lorder, lorder, echo)
     AC_CHECK_PROG(TSORT, tsort, tsort, cat)
     if test "$LORDER" = echo -o "$TSORT" = cat ; then
       # We need a matched pair...either lorder:tsort or echo:cat.
       LORDER=echo
       TSORT=cat
     fi
     ;;
esac


#### Check if "${build_root}" is defined;  provide a default one
if test -n "${with_build_root}" ; then
   build_root=${with_build_root}
elif test -n "${exec_prefix}"  &&  test "${exec_prefix}" != "NONE" ; then
   build_root=${exec_prefix}
fi

if test -z "${build_root}" ; then
   build_root="${srcdir}/${compiler}"
else
   with_suffix=${with_suffix:="no"}
fi


#### Release/Debug
if test "$with_debug" = "no" ; then
   release_debug="Release"
   D_SFX=
else
   release_debug="Debug"
   D_SFX=d
fi
DEBUG_SFX=${release_debug}


#### Add "Release"/"Debug" and "MT", "DLL", "64" suffixes to "${build_root}"
if test "$with_suffix" != "no" ; then
   build_root="${build_root}-${release_debug}${mt_sfx}${dll_sfx}${bit64_sfx}"
fi


#### Add full host specs to "${build_root}"
if test "$with_hostspec" = "yes" ; then
   build_root="${build_root}--${host}"
fi


#### Overall configuration mode summary
signature="${compiler}-${release_debug}${mt_sfx}${dll_sfx}${bit64_sfx}--${host}"


#### Check if there is "${build_root}" dir
test -d "${build_root}"  ||  mkdir -p "${build_root}"


#### Make "build_root" absolute
case "${build_root}" in
   /* ) ;;
   . )
      CDPATH=
      if test "${PWD}" != "" && test "`(cd ${PWD} ; sh -c pwd)`" = "`pwd`"  ;
      then
         build_root="$PWD"
      else
         build_root="`(cd ${build_root}; pwd)`"
      fi
   ;;
   *  ) build_root="`(cd ${build_root}; pwd)`" ;;
esac


#### Make "srcdir" absolute
case "${srcdir}" in
   /* ) ;;
   . )
      CDPATH=
      if test "${PWD}" != "" && test "`(cd ${PWD} ; sh -c pwd)`" = "`pwd`"  ;
      then
        srcdir="$PWD"
      else
        srcdir="`(cd ${srcdir}; pwd)`"
      fi
   ;;
   * ) srcdir="`(cd ${srcdir}; pwd)`" ;;
esac


#### Source, include, and build directories
ncbiconf="$build_root/inc/ncbiconf.h"
if test "$build_root" = "$srcdir" ; then
   builddir="$srcdir/src"
else
   builddir="$build_root/build"
fi
status_dir="$build_root/status"


#### Create build directories
mkdir -p "$builddir" "$build_root/lib" "$build_root/bin"


#### Check if the target directory already has a configured system in it
if test "$no_create" != "yes"  -a  -f "${ncbiconf}" ; then
   echo
   echo "There is already a built \"${ncbiconf}\"!"
changequote(, )dnl
   echo "Do you want to re-configure in \"${builddir}\"? [y/n]"
changequote([, ])dnl
   read answer
   case "$answer" in
      y | Y | yes | YES ) ;;
      * )  AC_MSG_ERROR([Re-configuration has been canceled by user.]) ;;
   esac
   echo
fi


#### Use right command shell in scripts and makefiles
script_shell="#! ${CONFIG_SHELL:=/bin/sh}"
make_shell="SHELL=${CONFIG_SHELL:=/bin/sh}"


#### Turn on compilation warnings and adjust
#### some compiler-specific flags and features

if test "$CONF_obj_ext"   = " " ;  then obj_ext="" ; else obj_ext="${CONF_obj_ext:=.o}" ; fi
if test "$CONF_lib_pre"   = " " ;  then lib_pre="" ; else lib_pre="${CONF_lib_pre:=lib}" ; fi
if test "$CONF_lib_l_pre" = " " ;  then lib_l_pre="" ; else lib_l_pre="${CONF_lib_l_pre:=-l}" ; fi
if test "$CONF_lib_ext"   = " " ;  then lib_ext="" ; else lib_ext="${CONF_lib_ext:=.a}" ; fi
if test "$CONF_dll_ext"   = " " ;  then dll_ext="" ; else dll_ext="${CONF_dll_ext:=.so}" ; fi
if test "$CONF_lib_l_ext" = " " ;  then lib_l_ext="" ; else lib_l_ext="${CONF_lib_l_ext}" ; fi
if test "$CONF_exe_ext"   = " " ;  then exe_ext="" ; else exe_ext="${CONF_exe_ext}" ; fi
if test "$CONF_f_compile" = " " ;  then f_compile="" ; else f_compile="${CONF_f_compile:=-c }" ; fi
if test "$CONF_f_outobj"  = " " ;  then f_outobj="" ; else f_outobj="${CONF_f_outobj:=-o }" ; fi
if test "$CONF_f_outlib"  = " " ;  then f_outlib="" ; else f_outlib="${CONF_f_outlib}" ; fi
if test "$CONF_f_libpath" = " " ;  then f_libpath="" ; else f_libpath="${CONF_f_libpath:=-L}" ; fi
if test "$CONF_f_runpath" = " " ;  then f_runpath="" ; else f_runpath="${CONF_f_runpath:=$CONF_f_libpath}" ; fi
if test "$CONF_f_outexe"  = " " ;  then f_outexe="" ; else f_outexe="${CONF_f_outexe:=-o }" ; fi


#### For some compilers(e.g. SunPro 5.0), there is no sense to compare the
#### old object/lib/binary file with the new one -- they will always be
#### different because of some timestamp or smth. else ("--without-bincopy")
BINCOPY="${CONFIG_SHELL-/bin/sh} \$(top_srcdir)/scripts/if_diff.sh \"cp -p\""
if test "$with_bincopy" = "no" ; then
   BINCOPY="cp -p"
fi


#### Manage the debug-vs.-optimization flags ("--*-debug", "--*-optimization")
changequote(, )dnl
if test "$compiler" != "MSVC" ; then
   if test "$with_debug" = "no" ; then
      with_optimization=${with_optimization:="yes"}
      CFLAGS=`  echo $CFLAGS    | sed 's/-g[0-9]*//g'`
      CXXFLAGS=`echo $CXXFLAGS  | sed 's/-g[0-9]*//g'`
      LDFLAGS=` echo $LDFLAGS   | sed 's/-g[0-9]*//g'`
   else
      with_optimization=${with_optimization:="no"}
      if echo "$CFLAGS"   |grep -v >/dev/null '\-g' ; then
         CFLAGS="$CFLAGS -g"     ; fi
      if echo "$CXXFLAGS" |grep -v >/dev/null '\-g' ; then
         CXXFLAGS="$CXXFLAGS -g" ; fi
      if test "$with_optimization" = "no" ; then
         CPPFLAGS="-D_DEBUG $CPPFLAGS" ; fi
      if echo "$LDFLAGS"  |grep -v >/dev/null '\-g' ;
         then LDFLAGS="$LDFLAGS -g" ; fi
      STRIP=":"
   fi

   if test "$with_optimization" = "no" ; then
      CFLAGS=`  echo $CFLAGS    | sed 's/-O[0-9]*//g'`
      CXXFLAGS=`echo $CXXFLAGS  | sed 's/-O[0-9]*//g'`
      LDFLAGS=` echo $LDFLAGS   | sed 's/-O[0-9]*//g'`
      FAST_CFLAGS="$CFLAGS" 
      FAST_CXXFLAGS="$CXXFLAGS" 
   else
      FAST_CFLAGS="`  echo $CFLAGS   | sed 's/-O[0-9]*//g'` $FAST_CFLAGS"
      FAST_CXXFLAGS="`echo $CXXFLAGS | sed 's/-O[0-9]*//g'` $FAST_CXXFLAGS"
      if echo "$CFLAGS"   |grep -v >/dev/null '\-O' ; then
         CFLAGS="$CFLAGS -O" ; fi
      if echo "$CXXFLAGS" |grep -v >/dev/null '\-O' ; then
         CXXFLAGS="$CXXFLAGS -O" ; fi
      CPPFLAGS="-DNDEBUG $CPPFLAGS"
      if echo "$LDFLAGS"  |grep -v >/dev/null '\-O' ; then
         LDFLAGS="$LDFLAGS -O" ; fi
   fi

   if test "$with_debug" = "no"  &&  test "$with_optimization" = "no" ; then
      CPPFLAGS="-DNDEBUG $CPPFLAGS"
   fi
fi
changequote([, ])dnl



### ------------------------------------------------------------------
### ------------------------------------------------------------------

### Check for header files.
AC_CHECK_HEADERS(iostream  iostream.h)
AC_CHECK_HEADERS(fstream   fstream.h)
AC_CHECK_HEADERS(strstream strstream.h strstrea.h)


### Check for the platform (very roughly)
AC_CHECK_HEADERS(unistd.h,  x_have_unistd="yes")
AC_CHECK_HEADERS(windows.h, x_have_windows="yes")

if test "$x_have_windows" = "yes" ; then
   AC_DEFINE_UNQUOTED(NCBI_OS_MSWIN, 1)
   AC_DEFINE_UNQUOTED(NCBI_OS, "MSWIN")
elif test "$x_have_unistd" = "yes" ; then
   AC_DEFINE_UNQUOTED(NCBI_OS_UNIX, 1)
   AC_DEFINE_UNQUOTED(NCBI_OS, "UNIX")

   UNIX_SRC='$(UNIX_SRC)'
   UNIX_USR_PROJ='$(UNIX_USR_PROJ)'
   WithFeatures="$WithFeatures unix"

   if test "$OSTYPE" = "irix" ; then
      AC_DEFINE_UNQUOTED(NCBI_OS_IRIX, 1)
   fi
else
   AC_DEFINE_UNQUOTED(NCBI_OS_MAC, 1)
   AC_DEFINE_UNQUOTED(NCBI_OS, "MAC")
fi


### typedef socklen_t
AC_MSG_CHECKING(for type socklen_t)
AC_TRY_COMPILE(
   [#include <sys/socket.h>],
   [
     socklen_t addrlen = 0;
     (void) accept(0, (struct sockaddr*) 0, &addrlen);
   ],
   AC_DEFINE_UNQUOTED(HAVE_SOCKLEN_T, 1)
)
AC_MSG_RESULT()


### <sys/sockio.h>
AC_MSG_CHECKING(for sys/sockio.h)
AC_TRY_COMPILE(
   [#include <sys/sockio.h>],
   ,
   AC_DEFINE_UNQUOTED(HAVE_SYS_SOCKIO_H, 1)
)
AC_MSG_RESULT()


### Check for some reentrant library functions (only needed in MT mode)

if test "$with_mt" = "yes" ; then

## gethostbyname_r() -- 2 different variants

AC_MSG_CHECKING(for gethostbyname_r)
x_gethostbyname_r="no"
AC_TRY_COMPILE(
   [#include <netdb.h>],
   [
     void*           dummy = (void*) &gethostbyname_r;
     char            buf[1024];
     int             err;
     struct hostent* hp = 0;
     hp = gethostbyname_r("", hp, buf, sizeof(buf), &err);
   ],
   [
     x_gethostbyname_r="5"
   ]
)
if test "$x_gethostbyname_r" = "no" ; then
AC_TRY_COMPILE(
   [#include <netdb.h>],
   [
     void*           dummy = (void*) &gethostbyname_r;
     char            buf[1024];
     int             err;
     struct hostent* hp = 0;
     err = gethostbyname_r("", hp, buf, sizeof(buf), &hp, &err);
   ],
   [
     x_gethostbyname_r="6"
   ]
)
fi

if test "$x_gethostbyname_r" != "no" ; then
   AC_DEFINE_UNQUOTED(HAVE_GETHOSTBYNAME_R, $x_gethostbyname_r)
fi
AC_MSG_RESULT($x_gethostbyname_r)



## gethostbyaddr_r() -- 2 different variants

AC_MSG_CHECKING(for gethostbyaddr_r)
x_gethostbyaddr_r="no"
AC_TRY_COMPILE(
   [
     #include <netdb.h>
     #include <sys/types.h>
     #include <sys/socket.h>
   ],
   [
     void* dummy = (void*) &gethostbyaddr_r;
     char            buf[1024];
     int             err;
     unsigned int    host;
     struct hostent* hp = 0;
     hp = gethostbyaddr_r((char *) &host, sizeof(host), AF_INET,
                          hp, buf, sizeof(buf), &err);
   ],
   [
     x_gethostbyaddr_r="7"
   ]
)
if test "$x_gethostbyaddr_r" = "no" ; then
AC_TRY_COMPILE(
   [
     #include <netdb.h>
     #include <sys/types.h>
     #include <sys/socket.h>
   ],
   [
     void* dummy = (void*) &gethostbyaddr_r;
     char            buf[1024];
     int             err;
     unsigned int    host;
     struct hostent* hp = 0;
     int res = gethostbyaddr_r((char *) &host, sizeof(host), AF_INET,
                               hp, buf, sizeof(buf), &hp, &err);
   ],
   [
     x_gethostbyaddr_r="8"
   ]
)
fi

if test "$x_gethostbyaddr_r" != "no" ; then
   AC_DEFINE_UNQUOTED(HAVE_GETHOSTBYADDR_R, $x_gethostbyaddr_r)
fi
AC_MSG_RESULT($x_gethostbyaddr_r)


AC_MSG_CHECKING(for localtime_r)
AC_TRY_COMPILE(
   [#include <time.h>],
   [
     void* dummy = (void*) &localtime_r;
     time_t    timer;
     struct tm result;
     (void) localtime_r(&timer, &result);
   ],
   AC_DEFINE_UNQUOTED(HAVE_LOCALTIME_R, 1)
)
AC_MSG_RESULT()


AC_MSG_CHECKING(for pthread_setconcurrency)
AC_TRY_COMPILE(
   [#include <pthread.h>],
   [
     void* dummy = (void*) &pthread_setconcurrency;
     int ret_value = pthread_setconcurrency(5);
   ],
   AC_DEFINE_UNQUOTED(HAVE_PTHREAD_SETCONCURRENCY, 1)
)
AC_MSG_RESULT()

fi
### ...reentrant versions of some standard library functions, MT functions


### Check for other standard library functions

AC_MSG_CHECKING(for strdup)
AC_TRY_COMPILE(
   [#include <string.h>],
   [
     void* dummy = (void*) &strdup;
     const char s[] = "abc";
     char* s1 = strdup(s);
   ],
   AC_DEFINE_UNQUOTED(HAVE_STRDUP, 1)
)
AC_MSG_RESULT()


AC_MSG_CHECKING(for Linux-like sysinfo with 1 arg)
AC_TRY_COMPILE(
   [#include <sys/sysinfo.h>],
   [
     void* dummy = (void*) &sysinfo;
     struct sysinfo* info = 0;
     int retval = sysinfo(info);
   ],
   AC_DEFINE_UNQUOTED(HAVE_SYSINFO_1, 1)
)
AC_MSG_RESULT()


AC_MSG_CHECKING(for getloadavg)
AC_TRY_COMPILE(
   [#include <sys/loadavg.h>],
   [
     void* dummy = (void*) &getloadavg;
     double loadavg[3];
     int i = getloadavg(loadavg, 3);
   ],
   AC_DEFINE_UNQUOTED(HAVE_GETLOADAVG, 1)
)
AC_MSG_RESULT()


AC_MSG_CHECKING(for sysmp)
AC_TRY_COMPILE(
   [
     #include <sys/sysinfo.h>
     #include <sys/sysmp.h>
   ],
   [ 
     void* dummy = (void*) &sysmp;
     struct sysinfo si;
     sysmp(MP_SAGET, MPSA_SINFO, &si, sizeof(si));
   ],
   AC_DEFINE_UNQUOTED(HAVE_SYSMP, 1)
)
AC_MSG_RESULT()


AC_MSG_CHECKING(for strcasecmp)
AC_TRY_COMPILE(
   [#include <string.h>],
   [
     void* dummy = (void*) &strcasecmp;
     const char* s1 = 0;
     const char* s2 = 0;
     int i = strcasecmp(s1, s2);
   ],
   AC_DEFINE_UNQUOTED(HAVE_STRCASECMP, 1)
)
AC_MSG_RESULT()


### Check for typedefs, structures, and compiler features.
AC_CXX_CONST
AC_TYPE_SIZE_T

AC_MSG_CHECKING(if in-class template functions are supported)
AC_TRY_COMPILE(
   [class C { template<class T> void operator<<(const T& t) {} };],
   ,
   ,
   AC_DEFINE_UNQUOTED(NO_INCLASS_TMPL, 1)
)
AC_MSG_RESULT()


AC_MSG_CHECKING(if can use exception specifications throw(...) in func.proto)
AC_TRY_RUN(
   [template <class T> inline int test_throw_spec(const T&) throw() {
      return 0;
   }
   int main(void) {
      return test_throw_spec(123);
   }],
   AC_DEFINE_UNQUOTED(NCBI_USE_THROW_SPEC, 1)
)
AC_MSG_RESULT()


AC_MSG_CHECKING(if have an obsolete basic_string::compare())
AC_TRY_COMPILE(
   [#include <string>],
   [std::string s;  int i = 0;  i = s.compare("aaa", 0, 2); ],
   AC_DEFINE_UNQUOTED(NCBI_OBSOLETE_STR_COMPARE, 1)
)
AC_MSG_RESULT()


AC_MSG_CHECKING(if the auto_ptr template class is poor or not implemented)
AC_TRY_COMPILE(
   [#include <memory>],
   [int *ip = 0;  std::auto_ptr<int> ap(ip);  *ip = *ap;  ap.reset(ip); ],
   ,
   AC_DEFINE_UNQUOTED(HAVE_NO_AUTO_PTR, 1)
)
AC_MSG_RESULT()


AC_MSG_CHECKING(if min/max templates are not implemented)
AC_TRY_COMPILE(
   [#include <algorithm>
    #ifdef min
    #undef min
    #endif
    #ifdef max
    #undef max
    #endif
   ],
   [int a=0, b=1;  a = std::min(a, b);  b = std::max(a, b); ],
   ,
   AC_DEFINE_UNQUOTED(HAVE_NO_MINMAX_TEMPLATE, 1)
)
AC_MSG_RESULT()


AC_MSG_CHECKING(if new C++ streams do not have ios_base::)
AC_TRY_COMPILE(
   [#include <iostream>],
   [(void)(std::ios_base::out != std::ios_base::beg); ],
   ,
   AC_DEFINE_UNQUOTED(HAVE_NO_IOS_BASE, 1)
)
AC_MSG_RESULT()


AC_MSG_CHECKING(for std::char_traits::)
AC_TRY_COMPILE(
   [#include <iostream>],
   [using namespace std;
   char_traits<char>::int_type  i = char_traits<char>::eof();
   i = char_traits<char>::not_eof(i);
   char_traits<char>::char_type c = '\0';
   char_traits<char>::pos_type  p = 0;
   char_traits<char>::off_type  o = 1;
   ],
   ,
   AC_DEFINE_UNQUOTED(HAVE_NO_CHAR_TRAITS, 1)
)
AC_MSG_RESULT()


AC_MSG_CHECKING(for union semun)
AC_TRY_COMPILE(
   [#include <sys/sem.h>
   union semun { double a;  void* b; };],
   ,
   ,
   AC_DEFINE_UNQUOTED(HAVE_SEMUN, 1)
)
AC_MSG_RESULT()


AC_MSG_CHECKING(which way the stack grows)
AC_TRY_RUN(
    [
     int f(int* p)
     {
         int i;
         return (&i < p) ? 1 : 0;
     }
     int main(void)
     {
         int p;
         return f(&p);
     }
    ],
    [AC_DEFINE(STACK_GROWS_UP)
      stack_dir=up
    ],
    [AC_DEFINE(STACK_GROWS_DOWN)
     stack_dir=down],
    [stack_dir="unknown (cross-compiling)"])
AC_MSG_RESULT($stack_dir)

### Check for the availability of other packages
### --------------------------------------------


### Common-use system and miscellaneous libs
NETWORK_LIBS=${NETWORK_LIBS:="-lsocket -lnsl"}
MATH_LIBS=${MATH_LIBS:="-lm"}


### Math lib to be always linked in
LIBS="$MATH_LIBS $LIBS"


### Save original compiler/linker flags
orig_CPPFLAGS="$CPPFLAGS"
orig_LIBS="$LIBS"


NCBI_CHECK_LIBS(KSTAT, -lkstat,
   [#include <kstat.h>],
   [kstat_ctl_t* kc = kstat_open(); ])

NCBI_CHECK_LIBS(RPCSVC, -lrpcsvc,
   [#include <rpc/rpc.h>
    #include <rpcsvc/rstat.h>],
   [enum clnt_stat eee = rstat((char*) 0, (struct statstime*) 0); ])

NCBI_CHECK_LIBS(CRYPT, -lcrypt -lcrypt_i,
   [#include <stdlib.h>],
   [setkey("my_key"); ])

NCBI_CHECK_LIBS(DL, -ldl,
   [#include <dlfcn.h>],
   [dlopen("foo.so", 0); ])


## SYBASE libraries
AC_MSG_CHECKING(if SYBASE libraries are available)
if test -z "$with_sybase_local" ; then
   with_sybase_local="no"
fi
if test "$with_sybase" != "no" ; then
   sybase_lib=lib$bit64_sfx
   # deduce the path to Sybase package
   if test "$with_sybase_local" != "no" ; then
      if test "$with_sybase_local" = "yes" ; then
         SYBASE_PATH=${SYBASE_PATH:="/export/home/sybase/clients/current"}
      else
         SYBASE_PATH=$with_sybase_local
      fi
   elif test "$with_sybase_new" = "yes" ; then
      sybase_lib=lib
      if test -n "$bit64_sfx"; then
         : ${SYBASE_PATH="/netopt/Sybase/clients/12.5.64-EBF9728"}
         : ${SYBASE_LCL_PATH="/export/home/sybase/clients/12.5.64"}
      else
         : ${SYBASE_PATH="/netopt/Sybase/clients/12.5-EBF9723"}
         : ${SYBASE_LCL_PATH="/export/home/sybase/clients/12.5"}
      fi
   else
      SYBASE_PATH=${SYBASE_PATH:="/netopt/Sybase/clients/current"}
      SYBASE_LCL_PATH=${SYBASE_LCL_PATH:="/export/home/sybase/clients/current"}
   fi

   # compose a list of commonly used Sybase libraries
   case "$host_os" in
     linux* )
       SYBASE_LIBLIST="-lblk -lct -lcs -lsybtcl -lcomn -lintl -ldl"
       SYBASE_DLLLIST=""
       sybase_r="no"
       ;;
     solaris* | * )
       SYBASE_LIBLIST="-lblk -lct -lcs -ltcl -lcomn -lintl"
       SYBASE_DLLLIST="-ltli"
       ;;
   esac
   SYBASE_DBLIBS="-lsybdb"

   # multi-threading and 64-bit specific adjustments
   changequote(<<, >>)dnl
   if test "$with_mt" = "yes"  -a  "$sybase_r" != "no" ; then
      SYBASE_LIBLIST=`echo $SYBASE_LIBLIST | sed 's/\(-l[a-z][a-z_]*\)/\1_r/g'`
      SYBASE_DLLLIST=`echo $SYBASE_DLLLIST | sed 's/\(-l[a-z][a-z_]*\)/\1_r/g'`
   fi
   if test -n "$bit64_sfx" ; then
      SYBASE_INCLUDE="-DSYB_LP64"
      SYBASE_LIBLIST=`echo $SYBASE_LIBLIST | sed 's/\(-l[a-z][a-z_]*\)/\1'${bit64_sfx}'/g'`
      SYBASE_DLLLIST=`echo $SYBASE_DLLLIST | sed 's/\(-l[a-z][a-z_]*\)/\1'${bit64_sfx}'/g'`
      SYBASE_DBLIBS="${SYBASE_DBLIBS}${bit64_sfx}"
   fi
   changequote([, ])dnl

   # deduce conf.macro: SYBASE_INCLUDE, SYBASE_LIBS, SYBASE_DLLS, SYBASE_DBLIBS
   SYBASE_LIBPATH="$SYBASE_PATH/${sybase_lib}"
   SYBASE_INCLUDE="-I$SYBASE_PATH/include $SYBASE_INCLUDE"
   if test "$with_sybase_local" != "no" ; then
      SYBASE_LPATH="-L$SYBASE_LIBPATH ${CONF_f_runpath}$SYBASE_LIBPATH"
   else
      SYBASE_LCL_LIBPATH="$SYBASE_LCL_PATH/${sybase_lib}"
      SYBASE_LPATH="-L$SYBASE_LCL_LIBPATH -L$SYBASE_LIBPATH ${CONF_f_runpath}$SYBASE_LCL_LIBPATH:$SYBASE_LIBPATH"
      SYBASE_INCLUDE="-I$SYBASE_LCL_PATH/include $SYBASE_INCLUDE"
   fi
   SYBASE_LIBS="$SYBASE_LPATH $SYBASE_LIBLIST"
   SYBASE_DBLIBS="$SYBASE_LPATH $SYBASE_DBLIBS"
   SYBASE_DLLS="$SYBASE_DLLLIST"

   # test it
   CPPFLAGS="$SYBASE_INCLUDE $orig_CPPFLAGS"
   LIBS="$SYBASE_LIBS $SYBASE_DLLS $NETWORK_LIBS $orig_LIBS"
   AC_TRY_LINK(
      [#include <ctpublic.h>],
      [CS_RETCODE i = ct_init(0,0);  i = cs_ctx_global(0,0); ],
      [AC_DEFINE_UNQUOTED(HAVE_LIBSYBASE, 1)],
      [with_sybase="no"]
   )
fi

if test "$with_sybase" = "no" ; then
   SYBASE_PATH="No_Sybase"
   SYBASE_INCLUDE=
   SYBASE_LIBS=
   SYBASE_DLLS=
   SYBASE_DBLIBS=
   WithoutPackages="$WithoutPackages Sybase"
   AC_MSG_RESULT([no])
else
   WithPackages="$WithPackages Sybase"
   if test "$with_sybase_local" != "no" ; then
      AC_MSG_RESULT([$SYBASE_PATH])
   else
      AC_MSG_RESULT([$SYBASE_LCL_PATH:$SYBASE_PATH])
   fi
   if test "$with_mt" = "yes"  -a  "$sybase_r" = "no" ; then
       AC_MSG_WARN(["Using non-reentrant Sybase libraries"])
   fi
fi

SYBASE_LPATH=
SYBASE_LCL_LIBPATH=
SYBASE_LIBLIST=
SYBASE_DLLLIST=
SYBASE_LIBPATH=

if test -n "$SYBASE_DBLIBS" ; then
   AC_MSG_CHECKING(if SYBASE includes DBLib)
   CPPFLAGS="$SYBASE_INCLUDE $prig_CPPFLAGS"
   LIBS="$SYBASE_DBLIBS $SYBASE_LIBS $SYBASE_DLLS $NETWORK_LIBS $orig_LIBS"   
   AC_TRY_LINK(
      [
       #include <sybfront.h>
       #include <sybdb.h>
      ],
      [
       LOGINREC*  x_login     = 0;
       DBPROCESS* x_dbproc    = dbopen(x_login, (char*) "SRV_NAME");
      ],
      [AC_DEFINE_UNQUOTED(HAVE_LIBSYBDB, 1)],
      [SYBASE_DBLIBS=]
   )
   if test -n "$SYBASE_DBLIBS" ; then
      WithPackages="$WithPackages DBLib"
      AC_MSG_RESULT(yes)
   else
      WithoutPackages="$WithoutPackages DBLib"
      AC_MSG_RESULT(no)
   fi
fi


## Free TDS libraries
AC_MSG_CHECKING(if FreeTDS libraries are available)
if test "$with_ftds" != "no" ; then
   FTDS_PATH=${FTDS_PATH:="/netopt/Sybase/clients-mssql/current"}
   FTDS_LIBS=${FTDS_LIBS:="-lsybdb -ltds"}
   FTDS_LIBS="-L$FTDS_PATH/lib ${CONF_f_runpath}$FTDS_PATH/lib $FTDS_LIBS"
   FTDS_INCLUDE="-I$FTDS_PATH/include"

   CPPFLAGS="$FTDS_INCLUDE $orig_CPPFLAGS"
   LIBS="$FTDS_LIBS $NETWORK_LIBS $orig_LIBS"
   AC_TRY_LINK(
      [
       #include <sybdb.h>
       #include <tds.h>
      ],
      [
       LOGINREC*  x_login     = 0;
       DBPROCESS* x_dbproc    = dbopen(x_login, (char*) "SRV_NAME");
       TDSLOGIN*  x_tds_login = 0;
       TDSSOCKET* x_sock      = tds_connect(x_tds_login);
      ],
      [AC_DEFINE_UNQUOTED(HAVE_LIBFTDS, 1)],
      [with_ftds="no"]
   )
fi
if test "$with_ftds" = "no" ; then
   FTDS_PATH=
   FTDS_INCLUDE=
   FTDS_LIBS=
   WithoutPackages="$WithoutPackages FreeTDS"
   AC_MSG_RESULT([no])
else
   WithPackages="$WithPackages FreeTDS"
   AC_MSG_RESULT([$FTDS_PATH])
fi


## NCBI C Toolkit
AC_MSG_CHECKING(if NCBI C Toolkit is available)
if test "$with_ncbi_c" != "no" ; then
   NCBI_C_PATH=${NCBI_C_PATH:="$NCBI"}
   NCBI_C_INCLUDE="-I$NCBI_C_PATH/include${bit64_sfx}"
   if test "$with_debug" = "no" ; then
     NCBI_C_LIBPATH="$NCBI_C_PATH/lib${bit64_sfx}"
   else
     NCBI_C_LIBPATH="$NCBI_C_PATH/altlib${bit64_sfx}"
   fi
   if test "$with_mt" = "yes" ; then
     NCBI_C_LIBPATH="$NCBI_C_LIBPATH $NCBI_C_LIBPATH/ncbithr.o"
   fi
   NCBI_C_LIBPATH="-L$NCBI_C_LIBPATH"
   CPPFLAGS="$NCBI_C_INCLUDE $orig_CPPFLAGS"
   LIBS="$NCBI_C_LIBPATH -lncbi $NETWORK_LIBS $orig_LIBS"
   AC_TRY_LINK(
      [#include <ncbi.h>],
      [ Nlm_Boolean b = Nlm_GetArgs("fake", -1, 0); ],
      [AC_DEFINE_UNQUOTED(HAVE_NCBI_C, 1)],
      [with_ncbi_c="no"]
   )
fi
if test "$with_ncbi_c" = "no" ; then
   NCBI_C_PATH="No_NCBI_CToolkit"
   NCBI_C_INCLUDE=
   NCBI_C_LIBPATH=
   NCBI_C_ncbi=
   WithoutPackages="$WithoutPackages C-Toolkit"
else
   WithPackages="$WithPackages C-Toolkit"
   NCBI_C_ncbi=${NCBI_C_ncbi:="-lncbi"}
fi
AC_MSG_RESULT([$NCBI_C_PATH])


## wxWindows
AC_MSG_CHECKING(if wxWindows is available)
if test "$with_wxwin" != "no" ; then
   if test -z "$WXWIN_PATH"; then
      if test -d "$with_wxwin"; then
         WXWIN_PATH=$with_wxwin
      else
         WXWIN_PATH="$NCBI/wxwin"
      fi
   fi
   if test -d "$WXWIN_PATH/${compiler_pfx}${release_debug}${mt_sfx}/lib" \
        -a -z "$WXWIN_ARCH_PATH"; then
      WXWIN_ARCH_PATH="$WXWIN_PATH/${compiler_pfx}${release_debug}${mt_sfx}"
   else
      WXWIN_ARCH_PATH="$WXWIN_PATH"
   fi
   WXWIN_BINPATH=${WXWIN_BINPATH:="$WXWIN_ARCH_PATH/bin"}
   WXWIN_LIBPATH=${WXWIN_LIBPATH:="$WXWIN_ARCH_PATH/lib"}
#    if test -n "$WXWIN_GTK"; then
#    WXWIN_GTK=wx_gtk
#    if test "$with_debug" != "no"; then
#       if test -f "$WXWIN_LIBPATH/libwx_gtkd.a"
#       -o -f "$WXWIN_LIBPATH/libwx_gtkd.so"; then
#          WXWIN_GTK=wx_gtkd
#       fi
#    fi
#    fi    

   if test -x "$WXWIN_BINPATH/wx-config" ; then
      WXWIN_INCLUDE=`$WXWIN_BINPATH/wx-config --cflags`
#       WXWIN_LIBS="-L$WXWIN_LIBPATH ${CONF_f_runpath}$WXWIN_LIBPATH -l${WXWIN_GTK} -lgtk -lgdk -lgmodule -lglib"
      WXWIN_LIBS=`$WXWIN_BINPATH/wx-config --libs`

      CPPFLAGS="$WXWIN_INCLUDE $orig_CPPFLAGS"
      LIBS="$WXWIN_LIBS $orig_LIBS"
      AC_TRY_LINK(
         [#include <wx/wx.h>],
         [ wxExit(); ],
         [AC_DEFINE_UNQUOTED(HAVE_WXWINDOWS, 1)],
         [with_wxwin="no"]
      )
   else
      with_wxwin="no"
   fi
fi

if test "$with_wxwin" = "no" ; then
   WXWIN_LIBPATH="No_wxWindows"
   WXWIN_INCLUDE=
   WXWIN_LIBS=
   WithoutPackages="$WithoutPackages wxWindows"
else
   WithPackages="$WithPackages wxWindows"
fi
AC_MSG_RESULT([$WXWIN_LIBPATH])


## In-house Fast-CGI library
AC_MSG_CHECKING(if Fast-CGI library is available)
if test "$with_fastcgi" != "no" ; then
   FASTCGI_PATH=${FASTCGI_PATH:="$NCBI/fcgi-current"}
   FASTCGI_INCLUDE="-I$FASTCGI_PATH/include${bit64_sfx}"
   if test "$with_debug" = "no" ; then
      FASTCGI_LIBS="-L$FASTCGI_PATH/lib${bit64_sfx} -lfcgi $NETWORK_LIBS"
   else
      FASTCGI_LIBS="-L$FASTCGI_PATH/altlib${bit64_sfx} -lfcgi $NETWORK_LIBS"
   fi
   FASTCGI_OBJS="fcgibuf"
   CPPFLAGS="$FASTCGI_INCLUDE $orig_CPPFLAGS"
   LIBS="$FASTCGI_LIBS $orig_LIBS"
   AC_TRY_LINK(
      [#include <fcgiapp.h>],
      [ (void) FCGX_IsCGI(); ],
      [AC_DEFINE_UNQUOTED(HAVE_LIBFASTCGI, 1)],
      [with_fastcgi="no"]
   )
fi
if test "$with_fastcgi" = "no" ; then
   FASTCGI_PATH="No_FastCgi"
   FASTCGI_INCLUDE=
   FASTCGI_LIBS=
   FASTCGI_OBJS=
   WithoutPackages="$WithoutPackages Fast-CGI"
else
   WithPackages="$WithPackages Fast-CGI"
fi
AC_MSG_RESULT([$FASTCGI_PATH])


## NCBI SSS libraries
with_sss=${with_sss:="yes"}
with_sssutils=${with_sssutils:="yes"}
with_sssdb=${with_sssdb:="yes"}

if test "$with_sybase" = "no" ; then
   with_sssdb="no"
fi

if test "$with_sss" != "no" ; then
   NCBI_SSS_PATH=${NCBI_SSS_PATH:="$NCBI/sss/BUILD"}
   NCBI_SSS_INCLUDE=${NCBI_SSS_INCLUDE:="$NCBI_SSS_PATH/include"}
   if test -z "$NCBI_SSS_LIBPATH" ; then
      NCBI_SSS_LIBPATH="${NCBI_SSS_PATH}/lib/${release_debug}"
      if test "$compiler" = "GCC" ; then
         NCBI_SSS_LIBPATH="${NCBI_SSS_LIBPATH}GNU"
      elif test "$compiler" = "KCC" ; then
         NCBI_SSS_LIBPATH="${NCBI_SSS_LIBPATH}KAI"
      elif test "$compiler" = "ICC" ; then
         NCBI_SSS_LIBPATH="${NCBI_SSS_LIBPATH}ICC"
      fi
      NCBI_SSS_LIBPATH="${NCBI_SSS_LIBPATH}${bit64_sfx}"
      if test "$with_mt" = "yes" ; then
         NCBI_SSS_LIBPATH="${NCBI_SSS_LIBPATH}mt"
      fi
   fi
   if test ! -d "${NCBI_SSS_LIBPATH}"  -o  ! -d "${NCBI_SSS_INCLUDE}" ; then
      AC_MSG_CHECKING([for NCBI SSS directories in $NCBI_SSS_PATH])
      AC_MSG_RESULT([no])
      with_sss="no"
   fi
fi

if test "$with_sss" = "no" ; then
   with_sssutils="no"
   with_sssdb="no"
   NCBI_SSS_INCLUDE=
   NCBI_SSS_LIBPATH=
   LIBSSSUTILS=
   LIBSSSDB=
   WithoutPackages="$WithoutPackages SSS"
else
   NCBI_SSS_LIBPATH="-L${NCBI_SSS_LIBPATH}"
   NCBI_SSS_INCLUDE="-I${NCBI_SSS_INCLUDE}"

   # SSS UTILS
   AC_MSG_CHECKING(if NCBI SSS UTILS library is available)
   if test "$with_sssutils" != "no" ; then
      CPPFLAGS="$NCBI_SSS_INCLUDE $SYBASE_INCLUDE $orig_CPPFLAGS"
      LIBSSSUTILS="-lsssutils"
      LIBS="$NCBI_SSS_LIBPATH $LIBSSSUTILS $SYBASE_LIBS $NETWORK_LIBS $orig_LIBS"
      AC_TRY_LINK(
         [#include <hashtab.hpp>],
         [ (new C_HashTab(123))->Clear(); ],
         [
           AC_DEFINE_UNQUOTED(HAVE_LIBSSSUTILS, 1)
           WithPackages="$WithPackages SSSUTILS"
         ],
         [
           with_sssutils="no"
           LIBSSSUTILS=""
           WithoutPackages="$WithoutPackages SSSUTILS"
         ]
      )
   fi
   AC_MSG_RESULT([$with_sssutils])

   # SSS DB
   AC_MSG_CHECKING(if NCBI SSS DB library is available)
   if test "$with_sssdb" != "no" ; then
      CPPFLAGS="$NCBI_SSS_INCLUDE $SYBASE_INCLUDE $orig_CPPFLAGS"
      LIBSSSDB="-lsssdb"
      LIBS="$NCBI_SSS_LIBPATH $LIBSSSDB $SYBASE_LIBS $NETWORK_LIBS $orig_LIBS"
      AC_TRY_LINK(
         [#include <cslink.hpp>],
         [ (new CSLink)->connect2server("srv", "user", "pwd", "db"); ],
         [
           AC_DEFINE_UNQUOTED(HAVE_LIBSSSDB, 1)
           WithPackages="$WithPackages SSSDB"
         ],
         [
           with_sssdb="no"
           LIBSSSDB=""
           WithoutPackages="$WithoutPackages SSSDB"
         ]
      )
   fi
   AC_MSG_RESULT([$with_sssdb])
fi


# GEO
with_geo=${with_geo:="yes"}

if test "$with_sssdb" = "no"  -o  "$with_sssutils" = "no" ; then
   with_geo="no"
fi

if test "$with_geo" != "no" ; then
   NCBI_GEO_PATH=${NCBI_GEO_PATH:="$NCBI/geo"}
   NCBI_GEO_INCLUDE=${NCBI_GEO_INCLUDE:="$NCBI_GEO_PATH/include"}
   if test -z "$NCBI_GEO_LIBPATH" ; then
      NCBI_GEO_LIBPATH="${NCBI_GEO_PATH}/lib/${compiler_pfx}${release_debug}${bit64_sfx}"
   fi
   if test ! -d "${NCBI_GEO_LIBPATH}"  -o  ! -d "${NCBI_GEO_INCLUDE}" ; then
      AC_MSG_CHECKING([for NCBI GEO directories in $NCBI_GEO_PATH])
      AC_MSG_RESULT([no])
      with_geo="no"
   fi
fi

if test "$with_geo" = "no" ; then
   NCBI_GEO_INCLUDE=
   NCBI_GEO_LIBPATH=
   LIBGEODB=
else
   NCBI_GEO_LIBPATH="-L${NCBI_GEO_LIBPATH}"
   NCBI_GEO_INCLUDE="-I${NCBI_GEO_INCLUDE}"
   LIBGEODB="-lgeodb"

   AC_MSG_CHECKING(if NCBI GEO DB library is available)
   AC_DEFINE_UNQUOTED(HAVE_LIBGEODB, 1)
#     Cannot check it -- because GEO headers include C++ Toolkit headers!
#
#     CPPFLAGS="$NCBI_GEO_INCLUDE $NCBI_SSS_INCLUDE $SYBASE_INCLUDE $orig_CPPFLAGS"
#     LIBS="$NCBI_GEO_LIBPATH $LIBGEODB $NCBI_SSS_LIBPATH $LIBSSSDB $LIBSSSUTILS $SYBASE_LIBS $NETWORK_LIBS $orig_LIBS"
#     AC_TRY_LINK(
#        [#include <geo_api.hpp>],
#        [ (void) GEO_API::Submitter::contributors(); ],
#        [AC_DEFINE_UNQUOTED(HAVE_LIBGEODB, 1)],
#        [with_geo="no" ; LIBGEODB="" ]
#     )
   AC_MSG_RESULT([$with_geo])
fi

if test "$with_geo" != "no" ; then
   WithPackages="$WithPackages GEO"
else
   WithoutPackages="$WithoutPackages GEO"
fi


# SP
with_sp=${with_sp:="yes"}

if test "$with_sp" != "no" ; then
   SP_PATH=${SP_PATH:="$NCBI/SP"}
   if test -n "$SP_INCLUDE"; then
      SP_GENERIC="$SP_INCLUDE/../generic"
   else
      SP_INCLUDE="$SP_PATH/include"
      SP_GENERIC="$SP_PATH/generic"
   fi
   if test -z "$SP_LIBPATH" ; then
      SP_LIBPATH="${SP_PATH}/${compiler_pfx}${release_debug}${mt_sfx}${bit64_sfx}"
   fi
   if test ! -d "${SP_LIBPATH}"  -o  ! -d "${SP_INCLUDE}" ; then
      AC_MSG_CHECKING([for SP directories in $SP_PATH])
      AC_MSG_RESULT([no])
      with_sp="no"
   fi
fi

if test "$with_sp" != "no" ; then
   AC_MSG_CHECKING(if NCBI SP library is available)
   if test -d "$SP_GENERIC"; then
      SP_INCLUDE="-I${SP_INCLUDE} -I${SP_GENERIC}"
   else
      SP_INCLUDE="-I${SP_INCLUDE}"
   fi
   SP_LIBS="-L$SP_LIBPATH -lsp"
   CPPFLAGS="$SP_INCLUDE $orig_CPPFLAGS"
   LIBS="$SP_LIBS $orig_LIBS"
   AC_TRY_LINK(
      [#include <SGMLApplication.h>],
      [ SP_API::SGMLApplication app; ],
      [AC_DEFINE_UNQUOTED(HAVE_LIBSP, 1)],
      [with_sp="no"]
   )
   AC_MSG_RESULT([$with_sp])
fi

if test "$with_sp" = "no" ; then
   SP_INCLUDE=
   SP_LIBS=
   WithoutPackages="$WithoutPackages SP"
else
   WithPackages="$WithPackages SP"
fi


## NCBI PubMed
AC_MSG_CHECKING(if NCBI PubMed libraries are available)
if test "$with_ncbi_c" = "no" ; then
   with_pubmed="no"
fi

if test "$with_pubmed" != "no" ; then
   NCBI_PM_PATH=${NCBI_PM_PATH:="$NCBI/pubmed"}${bit64_sfx}

   if test ! -d "${NCBI_PM_PATH}" ; then
      with_pubmed="no"
   else
      CPPFLAGS="-I$NCBI_PM_PATH/pmentrez/include/Ncbi $NCBI_C_INCLUDE $orig_CPPFLAGS"
      LIBS="-L$NCBI_PM_PATH/pmentrez/libs -lpmncbi $NCBI_C_LIBPATH -lncbi $orig_LIBS"
      AC_TRY_LINK(
         [#include <ncbistr.hpp>],
         [ CNcbiString("abc"); ],
         [AC_DEFINE_UNQUOTED(HAVE_LIBPUBMED, 1)],
         [with_pubmed="no"]
      )
   fi
fi

if test "$with_pubmed" = "no" ; then
   NCBI_PM_PATH="No_NCBI_PubMed"
   WithoutPackages="$WithoutPackages PubMed"
else
   WithPackages="$WithPackages PubMed"
fi

AC_MSG_RESULT([$NCBI_PM_PATH])


## ORBacus CORBA
if test "${with_orbacus:-yes}" = yes; then
    : ${ORBACUS_PATH=$NCBI/corba/OB-4.0.1}
elif test "$with_orbacus" != no; then
    ORBACUS_PATH=$with_orbacus
fi
AC_MSG_CHECKING([for ORBacus])
if test "$with_orbacus" != no; then
   fullpath=${ORBACUS_PATH}/${release_debug}${mt_sfx}${bit64_sfx}
   LIBOB="-lOB"
   if test "$with_mt" = "yes" ; then
      LIBOB="$LIBOB -lJTC"
      LIBIMR="-lIMR"
   else
      LIBIMR=
   fi
   if test -f ${fullpath}/inc/OB/Config.h ; then
      : ${ORBACUS_INCLUDE="-I$ORBACUS_PATH/include -I$fullpath/inc"}
      : ${ORBACUS_LIBPATH="-L$fullpath/lib ${CONF_f_runpath}${fullpath}/lib"}
   elif test -f ${ORBACUS_PATH}/include/OB/Config.h ; then
     : ${ORBACUS_INCLUDE="-I$ORBACUS_PATH/include"}
     : ${ORBACUS_LIBPATH="-L$ORBACUS_PATH/lib ${CONF_f_runpath}${ORBACUS_PATH}/lib"}
   fi
   CPPFLAGS="$ORBACUS_INCLUDE $orig_CPPFLAGS"
   LIBS="$ORBACUS_LIBPATH $LIBOB $NETWORK_LIBS $orig_LIBS"
   AC_TRY_LINK(
      [#include <OB/CORBA.h>],
      [
        int c = 0;
        char* v[] = { NULL };
        CORBA::ORB_var orb = CORBA::ORB_init(c, v);
        if (!CORBA::is_nil(orb)) orb->destroy();
      ],
      [AC_DEFINE(HAVE_ORBACUS)],
      [with_orbacus="no"])
fi
if test "$with_orbacus" = no; then
   WithoutPackages="$WithoutPackages ORBacus"
   ORBACUS_PATH=No_ORBacus
   ORBACUS_INCLUDE=
   ORBACUS_LIBPATH=
   LIBOB=
   LIBIMR=
else
   WithPackages="$WithPackages ORBacus"
fi
AC_MSG_RESULT([$ORBACUS_PATH])
# Sigh... without this, the linker pulls in the contents of the cache
# and then gripes about missing JTC stuff.
test -d SunWS_cache  &&  rm -r SunWS_cache


### Restore original compiler/linker flags
LIBS="$orig_LIBS"
CPPFLAGS="$orig_CPPFLAGS"



### Optional projects
### --------------------------------------------


## `local_lbsm' support

local_lbsm="ncbi_lbsm ncbi_lbsm_ipc ncbi_service_lbsmd"

for fff in $local_lbsm ; do
   if test ! -f ${srcdir}/src/connect/${fff}.c ; then
      if test "$with_local_lbsm" = "yes" ; then
         AC_MSG_ERROR(["--with-local-lbsm:  src/connect/${fff}.c is  missing"])
      fi
      with_local_lbsm="no"
      break
   fi
done

if test "$with_local_lbsm" = "no" ; then
   WithoutProjects="$WithoutProjects local_lbsm"
   local_lbsm="ncbi_service_lbsmd_stub"
else
   WithProjects="$WithProjects local_lbsm"
fi


## `serial' projects

if test "$with_serial" = "no" ; then
   NoConfProjects="$NoConfProjects serial"
fi

if test ! -f ${srcdir}/src/serial/Makefile.in  -o  \
        ! -d ${srcdir}/include/serial ; then
   if test "$with_serial" = "yes" ; then
      AC_MSG_ERROR(["--with-serial:  SERIAL sources are missing"])
   fi
   with_serial="no"
fi

if test "$with_serial" = "no" ; then
   WithoutProjects="$WithoutProjects serial"
   serial=
else
   WithProjects="$WithProjects serial"
   serial="serial"
fi


## `objects' projects

if test "$with_objects" = "no"  || \
   test "$with_objects" = ""  -a  "$with_app" != "yes" ; then
   NoConfProjects="$NoConfProjects objects"
fi

if test ! -f ${srcdir}/src/objects/Makefile.in  -o  \
        ! -d ${srcdir}/include/objects ; then
   if test "$with_objects" = "yes" ; then
      AC_MSG_ERROR(["--with-objects:  OBJECTS sources are missing"])
   fi
   with_objects="no"
fi

if test "$with_objects" = "yes"  -a  -n "$serial" ; then
   WithProjects="$WithProjects objects"
   objects="objects"
else
   WithoutProjects="$WithoutProjects objects"
   objects=
fi


## `dbapi' projects

if test "$with_dbapi" = "no" ; then
   NoConfProjects="$NoConfProjects dbapi"
fi

if test ! -f ${srcdir}/src/dbapi/Makefile.in  -o  \
        ! -d ${srcdir}/include/dbapi ; then
   if test "$with_dbapi" = "yes" ; then
      AC_MSG_ERROR(["--with-dbapi:  DBAPI sources are missing"])
   fi
   with_dbapi="no"
fi

if test "$with_dbapi" = "no" ; then
   WithoutProjects="$WithoutProjects dbapi"
   dbapi=
else
   WithProjects="$WithProjects dbapi"
   dbapi="dbapi"
fi


## `app' projects

if test "$with_app" = "no" ; then
   NoConfProjects="$NoConfProjects app"
fi

if test ! -f ${srcdir}/src/app/Makefile.in  -o  \
        ! -d ${srcdir}/include/app ; then
   if test "$with_app" = "yes" ; then
      AC_MSG_ERROR(["--with-app:  APP sources are missing"])
   fi
   with_app="no"
fi

if test "$with_app" = "yes" ; then
   WithProjects="$WithProjects app"
   app="app"
else
   WithoutProjects="$WithoutProjects app"
   app=
fi


## `ctools' project cluster

if test "$with_ctools" = "no" ; then
   NoConfProjects="$NoConfProjects ctools"
fi

if test ! -f ${srcdir}/src/ctools/Makefile.in  -o  \
        ! -d ${srcdir}/include/ctools  -o  "$with_ncbi_c" = "no"
then
   if test "$with_ctools" = "yes" ; then
      if test "$with_ncbi_c" = "no" ; then
         AC_MSG_ERROR(["--with-ctools:  NCBI C Toolkit is missing"])
      else
         AC_MSG_ERROR(["--with-ctools:  CTOOLS sources are missing"])
      fi
   fi
   with_ctools="no"
fi

if test "$with_ctools" = "no" ; then
   WithoutProjects="$WithoutProjects ctools"
else
   OPT_GROUPS="$OPT_GROUPS ctools"
fi


## `gui' project cluster

if test "$with_gui" = "no" ; then
   NoConfProjects="$NoConfProjects gui"
fi

if test ! -f ${srcdir}/src/gui/Makefile.in  -o  \
        ! -d ${srcdir}/include/gui  -o  "$with_wxwin" = "no"
then
   if test "$with_gui" = "yes" ; then
      if test "$with_wxwin" = "no" ; then
         AC_MSG_ERROR(["--with-gui:  wxWindows package is missing"])
      else
         AC_MSG_ERROR(["--with-gui:  GUI sources are missing"])
      fi
   fi
   with_gui="no"
fi

if test "$with_gui" = "no" ; then
   WithoutProjects="$WithoutProjects gui"
else
   OPT_GROUPS="$OPT_GROUPS gui"
fi


## `internal' project cluster

if test "$with_internal" = "no" ; then
   NoConfProjects="$NoConfProjects internal"
fi

if test "$with_internal" = "yes" ; then
   OPT_GROUPS="$OPT_GROUPS internal"
   if test "$with_sybase" = "no"  -o  "$with_ncbi_c" = "no"  -o  \
           "$with_sss" = "no"  -o  "$with_pubmed" = "no"  \
           "$with_sp" = "no"  -o  "$with_geo" = "no"  -o  "$with_sp" = "no"
   then
      AC_MSG_ERROR(["--with-internal:  Cannot build INTERNAL projects"])
   fi
else
   WithoutProjects="$WithoutProjects internal"
fi



### Miscellanea
### --------------------------------------------

if test "$with_execopy" = "no" ; then
   APP_NOCOPY="BINCOPY=@:"
else
   APP_NOCOPY=
fi

if test "$with_exe" = "no" ; then
   APP_NOBUILD="APP_PROJ="
else
   APP_NOBUILD=
fi

IF_REBUILDING_LIBS="#"
IF_REBUILDING_CONDITIONALLY="#"
case "$with_lib_rebuilds" in
  yes) IF_REBUILDING_LIBS="" ;;
  ask) IF_REBUILDING_CONDITIONALLY="" ;;
esac

case "$with_projects" in
 /* | "" ) AC_SUBST(PROJECTS, $with_projects) ;;
 *       ) AC_SUBST(PROJECTS, "\$(top_srcdir)/$with_projects") ;;
esac


# Run the test suite after the build?

if test -n "$with_check"  -a  "$with_check" != "no" ; then
   check="check"
   test "$with_check" != "yes"  &&  CHECK_ARG="$with_check"
fi



### Check for C standard types and sizes
### --------------------------------------------

AC_C_BIGENDIAN
AC_C_CHAR_UNSIGNED
AC_CHECK_SIZEOF(void*)
AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(__int64)
AC_CHECK_SIZEOF(double)
AC_CHECK_SIZEOF(long double)
AC_CHECK_SIZEOF(size_t)
bits=`expr 8 \* $ac_cv_sizeof_size_t`
AC_DEFINE_UNQUOTED(NCBI_PLATFORM_BITS, $bits)



### Cleanup
### ---------------------------------

case "$compiler" in
 WorkShop* )
   test -d SunWS_cache  &&  rm -r SunWS_cache
   ;;
 MIPSpro73 )
   test -d ii_files     &&  rm -r ii_files
   ;;
 KCC )
   test -d ti_files     &&  rm -r ti_files
   ;;
esac


### Get rid of the KCC "anti-license-warning" filter
if test "$KCC" = "yes"  -a  -n "$kcc_license" ; then
   CC="$kcc_CC"
   CXXCPP="$kcc_CXX -E"
   CXX="$kcc_CXX"
fi

### Get rid of the ICC "anti-license-warning" filter
if test "$ICC" = "yes"  -a  -n "$icc_license" ; then
   CC="$icc_CC"
   CXXCPP="$icc_CXX -E"
   CXX="$icc_CXX"
fi


#############################################################################
### Compose a list of configurables("*.in" -- makefiles, shell scripts, etc.)
### in subdirs to configure
### Compose their target counterparts
### Configure

excl_file="`pwd`/config.tmp.$$"
changequote(<<, >>)dnl
echo "NoConfProjects $NoConfProjects" | sed 's% \([^ ][^ ]*\)%\
^\\./\1/%g' > $excl_file
changequote([, ])dnl
configurables=`cd "${srcdir}/src"  &&  find . -name '*.in' -print | egrep -v -f $excl_file | sed "s%\.\(.*\)\.in%$builddir\1:\./src\1\.in%g"`
rm $excl_file

if test -z "$configurables" ; then
  AC_MSG_ERROR([Cannot find any configurables below \"${srcdir}\"!])
fi

eval ncbi_runpath=`echo $pure_runpath`
AC_DEFINE_UNQUOTED(NCBI_RUNPATH, "$ncbi_runpath")

### Template substitutions
AC_SUBST(signature)

AC_SUBST(build_root)

AC_SUBST(top_srcdir)
AC_SUBST(srcdir)
AC_SUBST(includedir)
AC_SUBST(status_dir)
AC_SUBST(builddir)
AC_SUBST(runpath)

AC_SUBST(CC)
AC_SUBST(CPP)
AC_SUBST(CXX)
AC_SUBST(AR)
AC_SUBST(RANLIB)
AC_SUBST(STRIP)

AC_SUBST(CFLAGS)
AC_SUBST(FAST_CFLAGS)
AC_SUBST(CXXFLAGS)
AC_SUBST(FAST_CXXFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(DEPFLAGS)
AC_SUBST(DEPFLAGS_POST)
AC_SUBST(LDFLAGS)
AC_SUBST(LIBS)

AC_SUBST(D_SFX)
AC_SUBST(DEBUG_SFX)

AC_SUBST(LIB_OR_DLL)
AC_SUBST(FORCE_STATIC_LIB)
AC_SUBST(LINK_DLL)
AC_SUBST(CFLAGS_DLL)
AC_SUBST(CXXFLAGS_DLL)

AC_SUBST(OPT_GROUPS)
AC_SUBST(local_lbsm)
AC_SUBST(serial)
AC_SUBST(objects)
AC_SUBST(dbapi)
AC_SUBST(app)

AC_SUBST(check)
AC_SUBST(CHECK_ARG)

AC_SUBST(script_shell)
AC_SUBST(make_shell)
AC_SUBST(obj_ext)
AC_SUBST(lib_pre)
AC_SUBST(lib_l_pre)
AC_SUBST(lib_ext)
AC_SUBST(dll_ext)
AC_SUBST(lib_l_ext)
AC_SUBST(exe_ext)
AC_SUBST(f_compile)
AC_SUBST(f_outobj)
AC_SUBST(f_outlib)
AC_SUBST(f_libpath)
AC_SUBST(f_runpath)
AC_SUBST(f_outexe)

AC_SUBST(THREAD_LIBS)
AC_SUBST(NETWORK_LIBS)
AC_SUBST(MATH_LIBS)
AC_SUBST(KSTAT_LIBS)
AC_SUBST(RPCSVC_LIBS)
AC_SUBST(CRYPT_LIBS)
AC_SUBST(DL_LIBS)
AC_SUBST(SYBASE_INCLUDE)
AC_SUBST(SYBASE_LIBS)
AC_SUBST(SYBASE_DLLS)
AC_SUBST(SYBASE_DBLIBS)
AC_SUBST(FTDS_INCLUDE)
AC_SUBST(FTDS_LIBS)
AC_SUBST(NCBI_C_INCLUDE)
AC_SUBST(NCBI_C_LIBPATH)
AC_SUBST(WXWIN_INCLUDE)
AC_SUBST(WXWIN_LIBS)
AC_SUBST(FASTCGI_INCLUDE)
AC_SUBST(FASTCGI_LIBS)
AC_SUBST(FASTCGI_OBJS)
AC_SUBST(NCBI_SSS_INCLUDE)
AC_SUBST(NCBI_SSS_LIBPATH)
AC_SUBST(LIBSSSUTILS)
AC_SUBST(LIBSSSDB)
AC_SUBST(NCBI_GEO_INCLUDE)
AC_SUBST(NCBI_GEO_LIBPATH)
AC_SUBST(LIBGEODB)
AC_SUBST(SP_INCLUDE)
AC_SUBST(SP_LIBS)
AC_SUBST(NCBI_PM_PATH)
AC_SUBST(ORBACUS_INCLUDE)
AC_SUBST(ORBACUS_LIBPATH)
AC_SUBST(LIBOB)
AC_SUBST(LIBIMR)

AC_SUBST(NCBI_C_ncbi)

AC_SUBST(BINCOPY)
AC_SUBST(APP_NOCOPY)
AC_SUBST(APP_NOBUILD)
AC_SUBST(IF_REBUILDING_LIBS)
AC_SUBST(IF_REBUILDING_CONDITIONALLY)
AC_SUBST(CC_FILTER)
AC_SUBST(CXX_FILTER)

AC_SUBST(KeepStateTarget)
AC_SUBST(Rules)
AC_SUBST(serial_ws50_rtti_kludge)

AC_SUBST(UNIX_SRC)
AC_SUBST(UNIX_USR_PROJ)

AC_SUBST(compiler)
AC_SUBST(compiler_root)
AC_SUBST(compiler_version)

AC_SUBST(COMPILER)
AC_SUBST(OSTYPE)


#############################################################################
### Create output files and do some post-configuration

## Compose a list of target files
changequote(<<, >>)dnl
x_configurables=`echo "$configurables" | sed 's/:[^:][^:]*\.in//g'`
changequote([, ])dnl

## Deploy the target directory tree
echo "$x_configurables" | while read x_file ; do
   x_dir=`dirname $x_file`
   test -d "$x_dir"  ||  mkdir -p "$x_dir"
done

## Configure the compiler/flag-dependent header
AC_CONFIG_HEADER(${ncbiconf}:config.h.in)

##
## Configure makefiles, shell scripts, etc.
##
AC_CONFIG_FILES($configurables)

AC_CONFIG_COMMANDS([default], [
status_dir="$SAVE_status_dir"
builddir="$SAVE_builddir"
build_root="$SAVE_build_root"
x_configurables="$SAVE_x_configurables"
ncbiconf="$SAVE_ncbiconf"
CFLAGS="$SAVE_CFLAGS"
CXXFLAGS="$SAVE_CXXFLAGS"
CPPFLAGS="$SAVE_CPPFLAGS"
LDFLAGS="$SAVE_LDFLAGS"
LIB_OR_DLL="$SAVE_LIB_OR_DLL"
with_dll="$SAVE_with_dll"
WithFeatures="$SAVE_WithFeatures"
WithPackages="$SAVE_WithPackages"
WithoutPackages="$SAVE_WithoutPackages"
WithProjects="$SAVE_WithProjects"
WithoutProjects="$SAVE_WithoutProjects"
with_extra_action="$SAVE_with_extra_action"
with_projects="$SAVE_with_projects"

dnl adjust "/$builddir" to "/src"
echo "Please wait while the makefiles are being adjusted..."
echo "$x_configurables" | while read x_file ; do
   mv "$x_file" "$x_file.tmp"
   cat "$x_file.tmp" | sed "s%/$builddir%/src%g" > "$x_file"
   rm "$x_file.tmp"
done

dnl make all shell scripts *.sh be executable
find "$build_root/." -name '*.sh' -print | while read x_file ; do
   chmod a+x "$x_file"
done

dnl save status, cache, and log files to the build tree dir "status/"
if test -f config.status -a -f config.cache -a -f config.log ; then
   test -d  "$status_dir"  ||  mkdir -p "$status_dir"
   mv config.status config.cache config.log "$status_dir"/
fi

dnl Update list of enabled packages
rm -f "$status_dir"/*.enabled
for p in $WithPackages $WithProjects $WithFeatures ; do
   touch "$status_dir"/$p.enabled
done

dnl execute extra-action
if test -n "$with_extra_action" ; then
   with_extra_action=`echo "$with_extra_action" | sed "s%{}%$build_root%g"`
   echo
   echo "executing:  $with_extra_action..."
   eval $with_extra_action
   exit_code="$?"
   if test $exit_code -ne 0 ; then
      echo "WARNING! non-zero exit code ($exit_code):  $with_extra_action"
   fi
   echo
fi

dnl print out additional configuration options
case "$LIB_OR_DLL" in
both) DLL_USAGE="LIBRARIES:   build both static and dynamic by default" ;;
dll)  DLL_USAGE="LIBRARIES:   build as dynamic by default" ;;
lib) 
   if test "$with_dll" = "no" ; then
      DLL_USAGE="LIBRARIES:   build as static only"
   else
      DLL_USAGE="LIBRARIES:   build as static by default"
   fi
   ;;
esac

if test -n "$with_projects"; then
    build_proj="To build selected projects:  cd $builddir && make all_p"
fi

cat << EOCONF
===============================================================================
NCBI C++ Toolkit documentation:  doc/index.html,  doc/config.html#ref_Running
For the available configuration flags run:  ./configure --help

CFLAGS   = $CFLAGS
CXXFLAGS = $CXXFLAGS
CPPFLAGS = $CPPFLAGS
LDFLAGS  = $LDFLAGS

$DLL_USAGE
FEATURES:   $WithFeatures
PACKAGES:
  enabled:  $WithPackages
  disabled: $WithoutPackages
PROJECTS:
  enabled:  $WithProjects
  disabled: $WithoutProjects

Tools / flags / paths:  $builddir/Makefile.mk
Configuration  header:  $ncbiconf

To build everything:  cd $builddir && make all_r
$build_proj
******* CONFIGURATION SUCCESSFUL *******
EOCONF
], [
SAVE_status_dir="$status_dir"
SAVE_builddir="$builddir"
SAVE_build_root="$build_root"
SAVE_x_configurables="$x_configurables"
SAVE_ncbiconf="$ncbiconf"
SAVE_CFLAGS="$CFLAGS"
SAVE_CXXFLAGS="$CXXFLAGS"
SAVE_CPPFLAGS="$CPPFLAGS"
SAVE_LDFLAGS="$LDFLAGS"
SAVE_LIB_OR_DLL="$LIB_OR_DLL"
SAVE_with_dll="$with_dll"
SAVE_WithFeatures="$WithFeatures"
SAVE_WithPackages="$WithPackages"
SAVE_WithoutPackages="$WithoutPackages"
SAVE_WithProjects="$WithProjects $OPT_GROUPS"
SAVE_WithoutProjects="$WithoutProjects"
SAVE_with_extra_action="$with_extra_action"
SAVE_with_projects="$with_projects"
])

AC_OUTPUT

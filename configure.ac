#############################################################################
#  $Id: configure.ac,v 1.354 2005/03/08 20:50:30 ucko Exp $
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
#   Setup NCBI C++ Toolkit build tree - tuned for your platform, compiler, etc.
#   USAGE:
#    1) Process this file with "autoconf" to produce a "configure" script.
#    2) Run the resultant "configure" script to produce:
#      a) "<builddir>/inc/ncbiconf.h"
#      b) "<builddir>/build/Makefile.mk"
#      c) other Makefile's and scripts in "<builddir>/build" and below
#
#############################################################################

AC_PREREQ(2.59)

dnl Set up lock.  The diversion magic lets this occur before AC_INIT, which
dnl already interferes with other configure processes.

AC_DIVERT_PUSH(INIT_PREPARE)
if $srcdir/scripts/get_lock.sh configure $$; then
    ac_clean_files=configure.lock
else
    exit 1
fi
cache_file=config.cache
# reconfigure.sh expects config.cache to exist; unconditionally clobber
# any command-line setting.
if test -z "$CONFIG_SITE" -a -r $srcdir/config.site; then
    CONFIG_SITE=$srcdir/config.site
fi
AC_DIVERT_POP

AC_INIT([ncbi-tools++],[0.0],[cpp-core@ncbi.nlm.nih.gov])
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
AC_ARG_WITH(profiling,
   [ --with-profiling        build profiled versions of libs and apps])
AC_ARG_WITH(dll,
   [ --with-dll              build all libraries as DLLs])
AC_ARG_WITH(static,
   [ --with-static           build all libraries statically even if --with-dll])
AC_ARG_WITH(static-exe,
   [ --with-static-exe       build all executables as statically as possible])
AC_ARG_WITH(mt,
   [ --with-mt               compile in a MultiThread-safe manner])
AC_ARG_WITH(64,
   [ --with-64               compile to 64-bit code])
AC_ARG_WITH(exe,
   [ --without-exe           do not build executables])
AC_ARG_WITH(runpath,
   [ --with-runpath=         hard-code the runtime path to DLLs])
AC_ARG_WITH(lfs,
   [ --with-lfs              enable large file support to the extent possible])

## Scripts, make features, and directory naming
AC_ARG_WITH(extra-action,
   [ --with-extra-action=    script to call after the configuration is complete])
AC_ARG_WITH(autodep,
   [ --with-autodep          automatic generation of dependencies (GNU make)])
AC_ARG_WITH(build-root,
   [ --with-build-root=DIR   specify a non-default build directory name])
AC_ARG_WITH(fake-root,
   [ --with-fake-root=DIR    appear to have been built under DIR])
AC_ARG_WITH(suffix,
   [ --without-suffix        no Release/Debug, MT or DLL sfx in the build dir name])
AC_ARG_WITH(hostspec,
   [ --with-hostspec         add full host specs to the build dir name])
AC_ARG_WITH(version,
   [ --without-version       don't always include the cplr ver in the bd name])
AC_ARG_WITH(execopy,
   [ --without-execopy       do not copy built executables to the BIN area])
AC_ARG_WITH(bincopy,
   [ --with-bincopy          populate lib and bin with copies, not hard links])
AC_ARG_WITH(lib-rebuilds,
   [ --with-lib-rebuilds     ensure that apps use up-to-date libraries])
AC_ARG_WITH(lib-rebuilds2,
   [ --with-lib-rebuilds=ask ask whether to update each app's libraries])dnl'
AC_ARG_WITH(projects,
   [ --with-projects=FILE    build projects listed in FILE by default])
AC_ARG_WITH(check,
   [ --with-check=           run test suite after the build])
AC_ARG_WITH(ncbi-public,
   [ --with-ncbi-public      ensure compatibility for all in-house platforms])
AC_ARG_WITH(strip,
   [ --with-strip            strip binaries at build time])
AC_ARG_WITH(pch,
   [ --with-pch              use precompiled headers if possible])

## NCBI packages
AC_ARG_WITH(ncbi-c,
   [ --without-ncbi-c        do not use NCBI C Toolkit])
AC_ARG_WITH(sss,
   [ --without-sss           do not use NCBI SSS libraries])
AC_ARG_WITH(sssutils,
   [ --without-utils         do not use NCBI SSS UTIL library])
AC_ARG_WITH(sssdb,
   [ --without-sssdb         do not use NCBI SSS DB library])
AC_ARG_WITH(included-sss,
   [ --with-included-sss     use the in-tree copy of SSS])
AC_ARG_WITH(geo,
   [ --without-geo           do not use NCBI GEO libraries])
AC_ARG_WITH(included-geo,
   [ --with-included-geo     use the in-tree copy of GEO])
AC_ARG_WITH(pubmed,
   [ --without-pubmed        do not use NCBI PubMed libraries])

## Third-party and system packages
AC_ARG_WITH(z,
   [ --with-z=DIR            use zlib installation in DIR])
AC_ARG_WITH(z2,
   [ --without-z             use internal copy of zlib])
AC_ARG_WITH(bz2,
   [ --with-bz2=DIR          use bzlib installation in DIR])
AC_ARG_WITH(bz2b,
   [ --without-bz2           use internal copy of bzlib])
AC_ARG_WITH(pcre,
   [ --with-pcre=DIR         use PCRE installation in DIR])
AC_ARG_WITH(pcre2,
   [ --without-pcre          use internal copy of PCRE])
AC_ARG_WITH(sybase,
   [ --without-sybase        do not use SYBASE libraries])
AC_ARG_WITH(sybase-local,
   [ --with-sybase-local=DIR use local SYBASE install (DIR is optional)])
AC_ARG_WITH(sybase-new,
   [ --with-sybase-new       use newer SYBASE install (12.5 rather than 12.0)])
AC_ARG_WITH(ftds,
   [ --without-ftds          do not use FreeTDS libraries])
AC_ARG_WITH(ftds2,
   [ --with-ftds=DIR         use FreeTDS installation in DIR])
AC_ARG_WITH(ftds-renamed,
   [ --without-ftds-renamed  do not rename Sybase DBLIB symbols in built-in FTDS])
AC_ARG_WITH(mysql,
   [ --without-mysql         do not use MySQL])
AC_ARG_WITH(mysql2,
   [ --with-mysql=DIR        use MySQL installation in DIR])
AC_ARG_WITH(fltk,
   [ --without-fltk          do not use FLTK])
AC_ARG_WITH(fltk2,
   [ --with-fltk=DIR         use FLTK installation in DIR])
AC_ARG_WITH(opengl,
   [ --without-opengl        do not use OpenGL])
AC_ARG_WITH(opengl2,
   [ --with-opengl=DIR       use OpenGL installation in DIR])
AC_ARG_WITH(mesa,
   [ --without-mesa          do not use MESA off-screen OpenGL])
AC_ARG_WITH(mesa2,
   [ --with-mesa=DIR         use MESA installation in DIR])
AC_ARG_WITH(glut,
   [ --without-glut          do not use GLUT])
AC_ARG_WITH(glut2,
   [ --with-glut=DIR         use GLUT installation in DIR])
AC_ARG_WITH(wxwin,
   [ --without-wxwin         do not use wxWindows])
AC_ARG_WITH(wxwin2,
   [ --with-wxwin=DIR        use wxWindows installation in DIR])
AC_ARG_WITH(fastcgi,
   [ --without-fastcgi       do not use Fast-CGI library])
AC_ARG_WITH(fastcgi2,
   [ --with-fastcgi=VERSION  use Fast-CGI installation in \$NCBI/fcgi-VERSION])
AC_ARG_WITH(fastcgi3,
   [ --with-fastcgi=DIR      use Fast-CGI installation in DIR])
AC_ARG_WITH(bdb,
   [ --without-bdb           do not use Berkeley DB library])
AC_ARG_WITH(bdb,
   [ --with-bdb=VERSION      use DB installation in \$NCBI/BerkeleyDB-VERSION])
AC_ARG_WITH(bdb,
   [ --with-bdb=DIR          use Berkeley DB installation in DIR])
AC_ARG_WITH(sp,
   [ --without-sp            do not use SP libraries])
AC_ARG_WITH(orbacus,
   [ --without-orbacus       do not use ORBacus CORBA libraries])
AC_ARG_WITH(orbacus2,
   [ --with-orbacus=DIR      use ORBacus installation in DIR])
AC_ARG_WITH(odbc,
   [ --with-odbc=DIR         use ODBC installation in DIR])
AC_ARG_WITH(odbc2,
   [ --without-odbc          do not use ODBC])
AC_ARG_WITH(python,
   [ --with-python=DIR       use Python installation in DIR])
AC_ARG_WITH(python2,
   [ --without-python        do not use Python])
AC_ARG_WITH(cppunit,
   [ --with-cppunit=DIR      use CPPUNIT installation in DIR])
AC_ARG_WITH(cppunit2,
   [ --without-cppunit       do not use CPPUNIT])
AC_ARG_WITH(sqlite,
   [ --with-sqlite=DIR       use SQLite installation in DIR])
AC_ARG_WITH(sqlite2,
   [ --without-sqlite,       do not use SQLite])
AC_ARG_WITH(expat,
   [ --with-expat=DIR        use Expat installation in DIR])
AC_ARG_WITH(expat2,
   [ --without-expat,        do not use Expat])
AC_ARG_WITH(sablot,
   [ --with-sablot=DIR       use Sablotron installation in DIR])
AC_ARG_WITH(sablot2,
   [ --without-sablot,       do not use Sablotron])
AC_ARG_WITH(libxml,
   [ --with-libxml=DIR       use libxml2 installation in DIR])
AC_ARG_WITH(libxml_,
   [ --without-libxml        do not use libxml2])
AC_ARG_WITH(oechem,
   [ --with-oechem=DIR       use OpenEye OEChem installation in DIR])
AC_ARG_WITH(oechem2,
   [ --without-oechem        do not use OEChem])
AC_ARG_WITH(gif,
   [ --with-gif=DIR          use lib(un)gif installation in DIR])
AC_ARG_WITH(gif2,
   [ --without-gif           do not use lib(un)gif])
AC_ARG_WITH(jpeg,
   [ --with-jpeg=DIR         use libjpeg installation in DIR])
AC_ARG_WITH(jpeg2,
   [ --without-jpeg          do not use libjpeg])
AC_ARG_WITH(png,
   [ --with-png=DIR          use libpng installation in DIR])
AC_ARG_WITH(png2,
   [ --without-png           do not use libpng])
AC_ARG_WITH(tiff,
   [ --with-tiff=DIR         use libtiff installation in DIR])
AC_ARG_WITH(tiff2,
   [ --without-tiff          do not use libtiff])
AC_ARG_WITH(xpm,
   [ --with-xpm=DIR          use libXpm installation in DIR])
AC_ARG_WITH(xpm2,
   [ --without-xpm           do not use libXpm])

## Optional projects
AC_ARG_WITH(local-lbsm,
   [ --without-local-lbsm    turn off support for IPC with locally running LBSMD])
AC_ARG_WITH(serial,
   [ --without-serial        do not build the serialization library and tools])
AC_ARG_WITH(objects,
   [ --without-objects       do not generate/build serializeable objects from ASNs])
AC_ARG_WITH(dbapi,
   [ --without-dbapi         do not build database connectivity libraries])
AC_ARG_WITH(app,
   [ --without-app           do not build standalone applications like ID1_FETCH])
AC_ARG_WITH(ctools,
   [ --without-ctools        do not build NCBI C Toolkit based projects])
AC_ARG_WITH(gui,
   [ --without-gui           do not build FLTK-based projects])
AC_ARG_WITH(algo,
   [ --without-algo          do not build CPU-intensive algorithms])
AC_ARG_WITH(internal,
   [ --without-internal      do not build internal projects])

## Manuals
AC_ARG_WITH(manuals,
   [***** See also HTML documentation in ./doc/index.html *****])


#### Check the passed arguments against the list of available ones
x_with_list="\
debug optimization profiling dll static runpath mt 64 projects ncbi-public \
exe autodep suffix hostspec execopy bincopy lib-rebuilds lib-rebuilds=ask \
check static-exe strip version lfs pch \
ncbi-c wxwin fastcgi sss sssdb sssutils included-sss geo included-geo \
sp pubmed \
z bz2 pcre sybase sybase-local sybase-new ftds mysql orbacus \
fltk opengl mesa glut bdb odbc python sqlite cppunit \
expat sablot libxml oechem gif jpeg tiff png xpm \
local-lbsm serial objects dbapi app ctools gui algo internal"

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
      --with-extra-action= | --exec-prefix= | --with-projects= \
      | --cache-file= | --build= | --host= | --target= | --with-runpath= )
      AC_MSG_ERROR(["  $x_arg:  requires value;  use --help to show usage"])
      ;;

      --with-extra-action=* | --with-build-root=* | --with-fake-root=* \
      | --with-projects=* | --with-check=* \
      | --cache-file=* | --build=* | --host=* | --prefix=* | --exec-prefix=* \
      | --libdir=* | --bindir=* | --includedir=* \
      | [[A-Z]*=*] \
      | --with-z=* | --with-bz2=* | --with-pcre=* \
      | --with-sybase-local=* | --with-wxwin=* | --with-orbacus=* \
      | --with-ftds=* | --with-mysql=* | --with-fltk=* | --with-fastcgi=* \
      | --with-sqlite=* | --with-expat=* | --with-sablot=* | --with-libxml=* \
      | --with-bdb=* | --with-odbc=* | --with-python=* | --with-gif=* | --with-jpeg=* \
      | --with-png=* | --with-tiff=* | --with-xpm=* | --with-opengl=* \
      | --with-mesa=* | --with-glut=* | --with-oechem=* | --with-cppunit=* \
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
AC_DEFINE(NCBI_CXX_TOOLKIT, 1, [This is the NCBI C++ Toolkit.])


#### Get the running host's properties
AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED(HOST,        "$host",        [Full GNU-style system type])
AC_DEFINE_UNQUOTED(HOST_CPU,    "$host_cpu",    [CPU type only])
AC_DEFINE_UNQUOTED(HOST_VENDOR, "$host_vendor", [System vendor only])
AC_DEFINE_UNQUOTED(HOST_OS,     "$host_os",     [System OS only])


#### Make some provisions for traditional operation
AC_PROG_INSTALL


#### C and C++ compilers
AC_LANG(C++)
AC_PROG_CC
AC_PROG_CXX


#### Detect Insure++, and attempt to find the real compiler.
case "$CC" in
   insure* | */insure*)
      psrcdump=`echo "$CC" | sed -e 's/insure.*/psrcdump/'`
      real_CC=`$psrcdump -t insure++ | sed -ne 's/^Compiler \(.*\)/\1/p'`
      real_dir=`$psrcdump -t insure++ | sed -ne 's/^Compilerinstalldir \(.*\)/\1/p'`
      test -n "$real_dir" && test -x "$real_dir/$real_CC" && \
        real_CC="$real_dir/$real_CC"
      case "$real_CC" in
          *gcc* ) LIBS="$LIBS -lstdc++"
      esac
      ;;
   *)
      real_CC=$CC
      ;;
esac


#### Detect KCC compiler
if test "$GCC" != "yes" ; then
   touch kcc_test.cpp
   kcc_ver="`$real_CC -V -c kcc_test.cpp 2>&1 | grep '^KAI '`"
   kcc_license="`$real_CC -V -c kcc_test.cpp 2>&1 | grep -v '^KAI '`"
   rm -f kcc_test.cpp kcc_test.o
   case "$kcc_ver" in
      KAI* )  KCC="yes" ;;
   esac
fi


#### Detect ICC compiler (newer versions of which configure can mis-ID as GCC)
if test "$KCC" != "yes"; then
   icc_ver="`$real_CC -V 2>&1 | grep '^Intel(R) C'`"
   icc_license="`$real_CC -c 2>&1 | grep '^icc: NOTE: The evaluation period for this product ends on '`"
   case "$icc_ver" in
      Intel*C*Compiler* )  ICC="yes"; GCC="no" ;;
   esac
fi


#### Detect IBM VisualAge compiler
if test "$GCC" != "yes"  -a  "$KCC" != "yes"  -a  "$ICC" != "yes"; then
   # Crude, but there doesn't seem to be a better alternative
   vac_ver="`$real_CC 2>&1 | grep 'AIX Compiler.*Version'`"
   case "$vac_ver" in
      *AIX* ) VAC="yes" ;;
   esac
fi


#### Detect Compaq compiler
if test "$GCC" != "yes"  -a  "$KCC" != "yes"  -a  "$ICC" != "yes" \
  -a  "$VAC" != "yes"; then
   # Must use CXX rather than real_CC to ensure correct version number.
   ccc_ver="`$CXX -V 2>&1 | grep '^Compaq C++ V'`"
   case "$ccc_ver" in
      *Compaq* ) CCC="yes" ;;
   esac
fi


#### Detect supported compilers
changequote(, )dnl
compiler=
compiler_ver=
ncbi_compiler="UNKNOWN"
ncbi_compiler_ver="0"

if test "$GCC" = "yes" ; then
   compiler_ver="`$real_CC -dumpversion 2>&1`"
   case "$compiler_ver" in
     2.95* | 2.96* | 3.* )
       compiler="GCC"
       ncbi_compiler="GCC"
       ncbi_compiler_ver="$compiler_ver" ;;
   esac
elif test "$KCC" = "yes" ; then
   compiler_ver="$kcc_ver"
   compiler="KCC"
   ncbi_compiler="KCC"
   ncbi_compiler_ver="`echo $kcc_ver | sed 's%^KAI .*C[+][+] \([0-9.]*\).*%\1%'`"
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
elif test "$CCC" = "yes" ; then
   compiler_ver="$ccc_ver"
   compiler=Compaq
   ncbi_compiler="COMPAQ"
   ncbi_compiler_ver="`echo $ccc_ver | sed 's%.*C[+][+] V\([0-9.]*\).*%\1%'`"
else
   case "$host_os" in
    solaris* )
      compiler_ver="`$real_CC -V 2>&1`"
      case "$compiler_ver" in
        *WorkShop*Compilers*5\.0 )
          compiler="WorkShop5"
          ncbi_compiler="WORKSHOP"
          ncbi_compiler_ver="5.0" ;;
        *Sun*WorkShop*6*5\.1* )
          compiler="WorkShop51"
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
        *Forte*7*5\.4* )
          compiler="WorkShop54"
          ncbi_compiler="WORKSHOP"
          ncbi_compiler_ver="5.4" ;;
      esac ;;

    irix* )
      compiler_ver="`$real_CC -version 2>&1`"
      case "$compiler_ver" in
        MIPSpro*Compilers:*Version*7\.3* )
          compiler="MIPSpro73"
          ncbi_compiler="MIPSPRO"
          ncbi_compiler_ver="7.3" ;;
      esac ;;

    cygwin* )
      case "$real_CC" in
        *cl\.exe* | cl)
          compiler="MSVC"
          ncbi_compiler="MSVC"
          ncbi_compiler_ver="6.0" ;;
      esac ;;
   esac
fi

ncbi_compiler_ver="`echo $ncbi_compiler_ver | sed 's%\([0-9]\)\.\([0-9]\)\.\([0-9]\).*%\1\2\3%; s%\([0-9]\)\.\([0-9][0-9]*\).*%\1\2%; s%^\([0-9][0-9]\)$%\10%; s%^\([0-9]\)$%\100%'`"
compiler_version=$ncbi_compiler_ver
changequote([, ])dnl

# Possible values of NCBI_COMPILER_$ncbi_compiler, since acconfig.h is obsolete
if false; then
   AC_DEFINE(NCBI_COMPILER_GCC,       1, [Compiler name])
   AC_DEFINE(NCBI_COMPILER_KCC,       1, [Compiler name])
   AC_DEFINE(NCBI_COMPILER_ICC,       1, [Compiler name])
   AC_DEFINE(NCBI_COMPILER_WORKSHOP,  1, [Compiler name])
   AC_DEFINE(NCBI_COMPILER_MIPSPRO,   1, [Compiler name])
   AC_DEFINE(NCBI_COMPILER_VISUALAGE, 1, [Compiler name])
   AC_DEFINE(NCBI_COMPILER_COMPAQ,    1, [Compiler name])
   AC_DEFINE(NCBI_COMPILER_MSVC,      1, [Compiler name])
   AC_DEFINE(NCBI_COMPILER_UNKNOWN,   1, [Compiler name])
fi

AC_DEFINE_UNQUOTED(NCBI_COMPILER_$ncbi_compiler, 1, [Compiler name])
AC_DEFINE_UNQUOTED(NCBI_COMPILER_VERSION, $ncbi_compiler_ver,
                   [Compiler version as three-digit integer])


#### Additional check for pre-defined compilers
case "$compiler:$compiler_version" in
  MSVC:* )
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
         * )  AC_MSG_WARN([Configuration has been canceled by user.]) ; exit 1;;
       esac
       echo
    fi ;;
esac


#### Use full path to the compiler, if possible
changequote(, )dnl

set $CC
x_CC="`type $1 | sed 's/.* \([^ ]*\)$/\1/'`"
if test ! -x "$x_CC" ; then
  x_CC="`which $1  2>/dev/null`"
fi

if test -x "$x_CC" ; then
  shift
  CC="$x_CC $*"
  echo "adjusted C   compiler: $CC"
fi
CC_PATH="`dirname $x_CC`"
x_CC=

set $CXX
x_CXX="`type $1 | sed 's/.* \([^ ]*\)$/\1/'`"
if test ! -x "$x_CXX" ; then
  x_CXX="`which $1  2>/dev/null`"
fi

if test -x "$x_CXX" ; then
  shift
  CXX="$x_CXX $*"
  echo "adjusted C++ compiler: $CXX"
fi
CXX_PATH="`dirname $x_CXX`"
x_CXX=

if test "$with_static_exe" = "yes"; then
   C_LINK='$(top_srcdir)/scripts/favor-static $(CC)'
   LINK='$(top_srcdir)/scripts/favor-static $(CXX)'
else
   C_LINK='$(CC)'
   LINK='$(CXX)'
fi

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
    *:Compaq | freebsd*:GCC         )  MT_FLAG=${MT_FLAG:="-pthread"} ;;
    *:VisualAge                     )  : ${MT_FLAG:="-qthreaded"} ;;
    *:GCC | *:ICC | irix*:MIPSpro73 )  ;;
    * )   AC_MSG_ERROR(["Do not know how to build MT-safe with compiler $CXX $compiler_ver"]) ;;
   esac
   CPPFLAGS="$CPPFLAGS -D_MT -D_REENTRANT -D_THREAD_SAFE"
   case "$host_os" in
     solaris* )  THREAD_LIBS=${THREAD_LIBS:="-lpthread -lposix4"} ;;
     freebsd* )  ;; # -pthread already substitutes libc_r for libc
     *        )  THREAD_LIBS=${THREAD_LIBS:="-lpthread"} ;;
   esac
   LIBS="$LIBS $THREAD_LIBS"
   mt_sfx="MT"
   WithFeatures="$WithFeatures MT"
else
   MT_FLAG=
   THREAD_LIBS=
   mt_sfx=""
fi
MT_SFX="${mt_sfx}"
AC_SUBST(MT_SFX)

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
   STRIP="@:"
   ;;

 linux*:GCC )
   LDFLAGS="-Wl,-E $LDFLAGS"
   # May be needed for pthread_{set,get}concurrency
   CPPFLAGS="-D_GNU_SOURCE $CPPFLAGS"
   ;;

 linux*:ICC )
   LDFLAGS="-Wl,-E $LDFLAGS"
   # "incomplete type is not allowed" should be an error, not a warning!
   CFLAGS="-we70 $CFLAGS"
   CXXFLAGS="-we70 $CXXFLAGS"
   # May be needed for pthread_{set,get}concurrency
   CPPFLAGS="-D_GNU_SOURCE $CPPFLAGS"
   ;;

 linux*)
   # May be needed for pthread_{set,get}concurrency
   CPPFLAGS="-D_GNU_SOURCE $CPPFLAGS"
   ;;

 freebsd*:GCC )
   LDFLAGS="-Wl,-E $LDFLAGS"
   ;;

 irix*:GCC )
   if test "$with_debug" != "no" ; then
      LDFLAGS="-LD_LAYOUT:lgot_buffer=32 $LDFLAGS"
   fi
   ;;

 solaris*:WorkShop* )
   CPPFLAGS="-D__EXTENSIONS__ $CPPFLAGS"
   CXXFLAGS="+w +w2 $CXXFLAGS"
   DEPFLAGS="-xM1"
   LDFLAGS="-xildoff $LDFLAGS"
   AR=${AR:="$CXX $MT_FLAG -xar -o"}
   STRIP="@:"
   if test "$with_debug" = "no" -a "$with_profiling" != "yes" \
      -a "$with_strip" = "yes" ; then
      LDFLAGS="-s $LDFLAGS"
   fi
   CXX_FILTER="2>&1 | \$(top_srcdir)/compilers/cxx_filter.$compiler.sh"
   LINK_FILTER="$CXX_FILTER"
   AR_FILTER="$CXX_FILTER"
   serial_ws50_rtti_kludge="rtti"
   ncbicntr="ncbicntr_workshop"
   ;;

 irix*:MIPSpro73 )
   COMMON_FLAGS="-fullwarn -brief_diagnostics -use_readonly_const -G0 -rdata_shared"
   # Warnings we disable (messages courtesy of "gettxt cc.cat.m:N")
   # 1107: A signed bit field has a length of 1 bit.
   # 1169: External/internal linkage conflicts with a previous declaration.
   # 1209: The controlling expression is constant. [while (0) et al.]
   # 1424: The %n1 is not used in declaring the argument types of %n2.
   # 1429: The type "long long" is nonstandard.
   # 1460: Function %n is redeclared "inline" after being called.
   # 1521: A nonstandard preprocessing directive is used.
   CFLAGS="$COMMON_FLAGS -woff 1209 $CFLAGS"
   CXXFLAGS="$COMMON_FLAGS -ansiW -ptused -LANG:std -LANG:ansi-for-init-scope -woff 1460,1521,1429,1169,1209,1107,1424 $CXXFLAGS"
   CXXCPP=${CXXCPP:="$CXX -E -LANG:std"}
   LDFLAGS="-LANG:std -G0 $LDFLAGS"
   CPPFLAGS="-D__LONGLONG $CPPFLAGS"
   CXX_FILTER="2>&1 | \$(top_srcdir)/compilers/cxx_filter.$compiler.sh"
   ;;

 cygwin*:GCC )
   with_dll="no"
   NETWORK_LIBS=${NETWORK_LIBS:="-lwsock32"}
   CONF_exe_ext=".exe"
   ;;

 cygwin*:MSVC )
   with_dll="no"
   CFLAGS="/W4 /Od /Zi /DEBUG /D_DEBUG"
   CXXFLAGS="/W4 /Od /Zi /DEBUG /D_DEBUG"
   CPPFLAGS="/nologo $CPPFLAGS"
   LDFLAGS="/DEBUG"
   AR="lib.exe"
   RANLIB=":"
   STRIP="@:"
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

 *:VisualAge)
   CFLAGS="-qlanglvl=stdc99 $CFLAGS"
   CXXFLAGS="-qrtti=all $CXXFLAGS"
   DEPFLAGS="-E -M"
   DEPFLAGS_POST='>/dev/null 2>&1; (cat $*.u; rm -f $*.u)'
   ;;

 *:Compaq)
   CPPFLAGS="-D__USE_STD_IOSTREAM $CPPFLAGS"
   CXXFLAGS="-tweak -nocompress -distinguish_nested_enums $CXXFLAGS"
   DEPFLAGS="$DEPFLAGS -distinguish_nested_enums"
   case "$host_os" in
      osf*)
        # Suppress link warnings; otherwise, we get "weak symbol multiply
        # defined" all over the place.
        LDFLAGS="-Wl,-S $LDFLAGS"
        # The linker sometimes needs a LOT of memory.
        LINK="ulimit -d \`ulimit -H -d\` && $LINK"
        ;;
   esac
   ;;

 darwin*:GCC)
   # There's a distinction between shared libraries (dylibs) and
   # loadable objects (bundles).
   # See <http://fink.sourceforge.net/doc/porting/shared.php>.
   CONF_dll_ext=".dylib"
   CONF_loadable_ext=".so"
   if $CXX -v 2>&1 | grep -q Apple; then
      # Suppress warnings about potential changes in the size of long double;
      # our ABI isn't too stable yet anyway.
      CFLAGS="-Wno-long-double $CFLAGS"
      CXXFLAGS="-Wno-long-double -fno-permissive $CXXFLAGS"
   fi
   if test -f /usr/include/dlfcn.h; then
      # may be able to do without /sw, so ignore it by default
      :
   elif test -f /sw/include/dlfcn.h; then
      CPPFLAGS="-I/sw/include $CPPFLAGS"
      LDFLAGS="-L/sw/lib $LDFLAGS"
      test -f /sw/lib/libdb-4.1.dylib  &&  BERKELEYDB_PATH="/sw"
   else
      AC_MSG_WARN([No dlfcn.h in /usr/include or /sw/include.])
   fi
   case "$CPPFLAGS $LIBS" in
      *sw/*) ;;
      *)
         if test -d "$NCBI/safe-sw"; then
             CPPFLAGS="-I$NCBI/safe-sw/include $CPPFLAGS"
             LDFLAGS="-L$NCBI/safe-sw/lib $LDFLAGS"
	 fi
	 ;;
   esac

   CPPFLAGS="-F/System/Library/Frameworks/CoreServices.framework/Frameworks $CPPFLAGS"
   LIBS="-Wl,-framework,ApplicationServices $LIBS"
   ;;
esac


#### Set makefile variables for platform-specific includes
changequote(, )dnl
COMPILER=`echo $ncbi_compiler | tr '[A-Z]' '[a-z]'`
OSTYPE=`echo $host_os | sed -e 's/[0-9.]*$//; s/-gnu[^-]*$//; y/A-Z/a-z/'`
changequote([, ])dnl


#### Check if the compiler matches one of supported compilers on this platform
if test -z "$compiler" ; then
changequote(, )dnl
   cat <<EOF

  Compiler $CXX $compiler_ver is not fully supported.
  Consult doc/config.html\#ref_Compilers for the list of
  fully supported platforms/compilers.
  Do you still want to proceed (at your own risk)? [n/y]
EOF
changequote([, ])dnl
   read answer
   case "$answer" in
      y )  AC_MSG_WARN([Proceeding at your own risk...]) ; echo ;;
      * )  AC_MSG_WARN([Configuration has been canceled by user.]) ; exit 1 ;;
   esac
   compiler="UNKNOWN"
fi


#### Set compiler prefix for non-native compilers
case "$host_os:$compiler" in
   *:GCC | *:KCC | *:ICC )
      compiler_pfx="${compiler}-"
      compiler_vpfx=`echo "${compiler}${compiler_ver}00-" | \
         sed -e 's/\.//g; s/[\([0-9][0-9][0-9]\)[0-9]*]/\1/'`
      ;;
esac


C_LIBS=$LIBS


#### 32/64-bit compilation, and extra C++ LIBS
if test "$with_64" = "yes" ; then
   bit64_sfx="64"
   case "$host:$compiler" in
    sparc-sun-solaris*:WorkShop5 | sparc-sun-solaris*:KCC )
      ARCH_CFLAGS="-xtarget=ultra -xarch=v9"
      ;;
    sparc-sun-solaris*:WorkShop51 )
      ARCH_CFLAGS="-xtarget=ultra -xarch=v9"
      LIBS="-Bstatic -L$CXX_PATH/../lib -lCstd -lCrun -Bdynamic $LIBS"
      ;;
changequote(, )dnl
    sparc-sun-solaris*:WorkShop5[234] )
      ARCH_CFLAGS="-xtarget=ultra -xarch=v9"
      # Redundant for programs, but necessary for dlopen-able shared libs,
      # at least in the x86 ReleaseMT configuration.
      LIBS="-lCstd $LIBS"
      ;;
changequote([, ])dnl
    mips*:GCC )
      ARCH_CFLAGS="-mips64"
      ;;
    *:GCC )
      # May not work prior to GCC 3.1.
      ARCH_CFLAGS="-m64"
      ;;

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
   if test "$with_64" = "no" ; then
      case "$host:$compiler" in
       sparc-sun-solaris*:WorkShop* | sparc-sun-solaris*:KCC )
         ARCH_CFLAGS="-xtarget=native -xarch=v9"
         ;;
       mips*:GCC )
         ARCH_CFLAGS="-mipsn32"
         ;;
       *:GCC )
         # May not work prior to GCC 3.1.
         ARCH_CFLAGS="-m32"
         ;;

       mips-sgi-irix*:MIPSpro73 )
         ARCH_CFLAGS="-n32"
         CPPFLAGS="$ARCH_CFLAGS $CPPFLAGS"
         AR=${AR:="$CXX $ARCH_CFLAGS -ar -WR,-v -o"}
         ;;
       mips-sgi-irix*:KCC )
         ARCH_CFLAGS="-n32"
         ;;
       * )
         # Just a warning, since 32-bit mode is typically the default anyway...
         AC_MSG_WARN(["Do not know how to compile 32-bit with compiler $CXX $compiler_ver $host:$compiler"])
         ;;
      esac
   fi
   case "$host:$compiler" in
    *solaris*:WorkShop5 )
      LIBS="-Bstatic -L$CXX_PATH/../SC5.0/lib -lm -Bdynamic $LIBS"
      MATH_LIBS=" "
      ;;
    *solaris*:WorkShop51 )
      LIBS="-Bstatic -L$CXX_PATH/../lib -lCstd -lCrun -lm -Bdynamic $LIBS"
      MATH_LIBS=" "
      ;;
    sparc-sun-solaris*:WorkShop54 )
      # Using this (still very conservative) non-generic XARCH flavor allows
      # compiler to use the extended instruction set which reportedly speeds
      # up the locking of at least some STL classes by allowing them to use
      # atomic increment instead of mutex;  also seems to fix something that
      # caused misterios basic_string<> related crashes in the
      # WorkShop54 -xarch=v8 -mt configuration).
      ARCH_CFLAGS="-xarch=v8plus"
      ;;
changequote(, )dnl
    *solaris*:WorkShop5[234] )
      # MATH_LIBS=" "
      # Redundant for programs, but necessary for dlopen-able shared libs,
      # at least in the x86 ReleaseMT configuration.
      LIBS="-lCstd $LIBS"
      ;;
changequote([, ])dnl
    *irix*:MIPSpro73 | *irix*:KCC )
      ARCH_CFLAGS="-n32"
      ;;
   esac
fi

case "$host_os:$host_cpu" in
   linux*:alpha*)
      ARCH_CFLAGS="-mieee $ARCH_CFLAGS"
      ;;
esac

CFLAGS="$ARCH_CFLAGS $CFLAGS"
CXXFLAGS="$ARCH_CFLAGS $CXXFLAGS"
LDFLAGS="$ARCH_CFLAGS $LDFLAGS"

if test -n "$with_64"; then
   AC_CACHE_CHECK([whether this system supports --with(out)-64],
      ncbi_cv_sys_with64_ok,
      [AC_LINK_IFELSE([
          AC_LANG_PROGRAM([#include <string>],
                          [std::string* sp = new std::string("foo");])],
          ncbi_cv_sys_with64_ok=yes, ncbi_cv_sys_with64_ok=no)])
   if test "$ncbi_cv_sys_with64_ok" = no; then
      AC_MSG_ERROR([cannot continue; please try different options])
   fi
fi

### large file support (switch to AC_SYS_LARGEFILE?)
case "$host_os:$compiler:$compiler_version" in
    *:GCC:3[[4-9]]*) : ${with_lfs=yes} ;;
esac

if test "$with_lfs" = "yes" ; then
   LFSFLAGS="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"
   CPPFLAGS="$LFSFLAGS $CPPFLAGS"
fi


#### Flags to enable (potentially unsafe) extra optimization.
if test -z "$DEF_FAST_FLAGS"  -o  -z "$FAST_CXXFLAGS" ; then
   case "$compiler:$compiler_version" in
      GCC:2* )
        # GCC 2.9x sometimes experiences internal errors at high optimization.
        DEF_FAST_FLAGS="-O2"
	;;
      GCC:* )
        DEF_FAST_FLAGS="-O9 -finline-functions -ffast-math -fstrict-aliasing"
        if test "$with_profiling" != "yes"; then
           # incompatible with -pg
           DEF_FAST_FLAGS="$DEF_FAST_FLAGS -fomit-frame-pointer"
        fi
        ;;
      WorkShop* )
        DEF_FAST_FLAGS="-fast"
        case "$with_ncbi_public:$host_cpu" in
           yes:sparc* )
              DEF_FAST_FLAGS="$DEF_FAST_FLAGS -xtarget=ultra"
              ;;
           *:i?86)
              # Work around a WorkShop standard library bug.
              DEF_FAST_FLAGS="$DEF_FAST_FLAGS -D_RWSTD_NO_TEST_AND_SET"
              ;;
        esac
        # (Re)include ARCH_CFLAGS here because -fast implies -xtarget=native.
        DEF_FAST_FLAGS="$DEF_FAST_FLAGS $ARCH_CFLAGS"
        ;;
      KCC:* )
        DEF_FAST_FLAGS="+K3"
        case "$host_os" in
           solaris* )  DEF_FAST_FLAGS="$DEF_FAST_FLAGS -fast $ARCH_CFLAGS" ;;
        esac
        ;;
      ICC:* )
        if test "$with_mt" = "yes" -a "$compiler_version" -lt 800; then
           DEF_FAST_FLAGS="-O3 -ip"
        else
           DEF_FAST_FLAGS="-O3 -axW -ip"
        fi
        ;;
      MIPSpro* )
        DEF_FAST_FLAGS="-O3 -Ofast"
        ;;
      Compaq:* )
        DEF_FAST_FLAGS="-fast"
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
: ${FAST_LDFLAGS="$DEF_FAST_FLAGS"}


#### DLLs

# Generate position-independent code (PIC)
if test -z "$CFLAGS_DLL"  -a  -z "$CXXFLAGS_DLL" ; then
   case "$host_os:$compiler" in
    darwin*:GCC )
      CFLAGS_DLL="-fno-common"
      CXXFLAGS_DLL="-fno-common"
      ;;
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

# Flag for setting the runtime library search path
if test -z "$CONF_f_runpath" ; then
   case "$host_os:$compiler" in
    solaris*:WorkShop* | solaris*:KCC )
      CONF_f_runpath="-R"
      ;;
    solaris*:GCC )
      CONF_f_runpath="-Wl,-R"
      ;;
    linux*:GCC | *bsd*:GCC | cygwin*:GCC | osf*:GCC )
      CONF_f_runpath="-Wl,-rpath,"
      ;;
    linux*:ICC )
      # trying to use "-Wl,-rpath," here sends "ld" to endless 100% CPU loop
      CONF_f_runpath="-Xlinker -rpath -Xlinker "
      ;;
    irix*:* | linux*:KCC | *:Compaq )
      CONF_f_runpath="-rpath "
      ;;
   esac
fi

case "$compiler:$compiler_version" in
  GCC:3* )
    # Need to specify runpath for compiler-provided libraries
    case $compiler_version in
        30*   ) major=3 ;;
        31*   ) major=4 ;;
        3[23]*) major=5 ;;
        3*    ) major=6 ;;
    esac
    found=false
    for f in libstdc++.so.$major libgcc_s.so.1 libstdc++.a; do
        path=`$CXX $LDFLAGS -print-file-name=$f`
        dir=`dirname "$path"`
        abs_dir=`cd "$dir" && pwd`
        case "$dir:$abs_dir" in
            *:/lib | *:/usr/lib ) break ;; # no rpath needed
            /*                  ) found=true; break ;;
        esac
    done
    if test "$found" = "true" -a -n "$CONF_f_runpath"; then
        LDFLAGS="$CONF_f_runpath$dir:/usr/local/lib $LDFLAGS"
        case "$host_os" in
           osf*)
              # the linker doesn't merge -rpath flags...
              CONF_f_runpath="$CONF_f_runpath$dir:/usr/local/lib:"
              ;;
        esac
    fi
    ;;
  ICC:6* )
    # Seems to use libcxa.so.1 even without -i_dynamic...
    LDFLAGS="$CONF_f_runpath$compiler_root/lib $LDFLAGS"
    ;;
  ICC:7* )
    # Favor the static version, as libcxa.so.3 isn't exactly common....
    # (Undocumented, and unavailable at all in older versions. :-/)
    LDFLAGS="-static-libcxa $LDFLAGS"
    ;;
  ICC:8* )
    # Back to the runpath approach, since ICC now ignores
    # -static-libcxa when building shared libraries....
    LDFLAGS="$CONF_f_runpath$compiler_root/lib $LDFLAGS"
    ;;
esac

# DLLs and profiling don't mix on all platforms
if test "$with_profiling" = "yes" ; then
   case "$host_os" in
      linux*) # any others?
         case "$with_dll" in
            yes) AC_MSG_ERROR([This platform does not support DLL profiling.])
                 ;;
            no) ;;
            *) with_dll=no ;;
         esac
      ;;
   esac
fi

# Whether to build DLLs or static (and whether by default, or forcibly)
LIB_OR_DLL="lib"
if test "$with_dll" = "yes" ; then
   case "$host_os:$compiler" in
    linux*:GCC | irix*:* | solaris*:WorkShop* | *:KCC | linux*:ICC \
    | solaris*:GCC | osf*:* | darwin*:GCC | freebsd*:GCC )
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
         * )  AC_MSG_WARN([Configuration has been canceled by user.]) ; exit 1 ;;
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
   WithFeatures="$WithFeatures DLL"
   # Some shared libraries end up containing static libraries, so make
   # sure to use DLL flags for both on platforms where such inclusion
   # would otherwise be illegal....
   case "$host_os:$host_cpu" in
      linux*:*86) ;;
      linux* | darwin*)
        CFLAGS="$CFLAGS $CFLAGS_DLL"
        CFLAGS_DLL=
        CXXFLAGS="$CXXFLAGS $CXXFLAGS_DLL"
        CXXFLAGS_DLL=
        ;;
   esac
fi

if test "$LIB_OR_DLL" = "dll"; then
   STATIC=
else
   STATIC="-static"
fi
AC_SUBST(STATIC)


#### GNU compiler::  common flags and definitions
if test "$compiler" = "GCC" ; then
   CFLAGS="-Wall -Wno-format-y2k $CFLAGS"
   CXXFLAGS="-Wall -Wno-format-y2k $CXXFLAGS"
   case "$compiler_version" in
      2* ) CXXFLAGS="-ftemplate-depth-32 $CXXFLAGS" ;;
   esac
   case "$host_os" in
      solaris* )  CPPFLAGS="-D__EXTENSIONS__ $CPPFLAGS" ;;
      irix*    )  CXXFLAGS="$CXXFLAGS -D_LANGUAGE_C_PLUS_PLUS" ;;
   esac
   # orig_CFLAGS=$CFLAGS
   # orig_CXXFLAGS=$CXXFLAGS
   # flag=-fno-merge-debug-strs
   # CFLAGS="$flag $CFLAGS"
   # CXXFLAGS="$flag $CXXFLAGS"
   # AC_CACHE_CHECK([whether GCC supports $flag], ncbi_cv_prog_gcc_nomerge,
   #    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[return 0;]])],
   #                       ncbi_cv_prog_gcc_nomerge=yes,
   #                       ncbi_cv_prog_gcc_nomerge=no)])
   # if test "$ncbi_cv_prog_gcc_nomerge" = no; then
   #    CFLAGS=$orig_CFLAGS
   #    CXXFLAGS=$orig_CXXFLAGS
   # fi
fi


#### KAI compiler::  common flags and definitions
if test "$compiler" = "KCC" ; then
   AR=${AR:="$CXX $MT_FLAG -o"}
   CFLAGS="--display_error_number --c $CFLAGS"
   CXXFLAGS="--display_error_number --one_instantiation_per_object $CXXFLAGS"
   case "$host_os" in
      solaris* )  CPPFLAGS="-D__EXTENSIONS__ -D__STDC__=0 $CPPFLAGS" ;;
   esac
   # Often hangs for no good reason :-/; limit time to ~17 minutes per instance
   if test -x compilers/wrapper.KCC.sh; then
      wrap="compilers/wrapper.KCC.sh"
      CC_WRAPPER="$CC_WRAPPER $wrap"
      CXX_WRAPPER="$CXX_WRAPPER $wrap"
      AR_WRAPPER="$AR_WRAPPER $wrap"
      CXXCPP="$CXX -E" # Don't double-wrap...
   fi
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
elif make --version 2>/dev/null | grep 'GNU Make' ; then
   Rules="rules_with_autodep"
else
   Rules="rules"
fi


### Support for precompiled headers
GCCPCH="#"
if test "$compiler" = GCC -a "$with_pch" = "yes"; then
   AC_CACHE_CHECK([whether $CXX supports precompiled headers], ncbi_cv_cxx_pch,
      [echo '#include <iostream>' > conftest.hpp
       echo $CXX $CPPFLAGS $CXXFLAGS -xc++-header -c conftest.hpp >&AS_MESSAGE_LOG_FD
       if $CXX $CPPFLAGS $CXXFLAGS -xc++-header -c conftest.hpp >&AS_MESSAGE_LOG_FD 2>&1 \
         &&  test -f conftest.hpp.gch; then
          ncbi_cv_cxx_pch=yes
       else
          ncbi_cv_cxx_pch=no
       fi])
   if test "$ncbi_cv_cxx_pch" = "yes"; then
      if test "$Rules" = "rules"; then
         AC_MSG_WARN([Your version of make does not fully support PCH-related dependencies.])
      fi
      CXXFLAGS="-DNCBI_USE_PCH $CXXFLAGS"
      GCCPCH=""
   fi
fi


#### Tools
AC_PROG_LN_S
AC_PROG_RANLIB
AC_CHECK_PROG(AR, ar, ar cr, ErrorCannotFind_AR)
if test "x$RANLIB" != "x:"; then
   AC_CACHE_CHECK([ranlib's effectiveness], ncbi_cv_prog_ranlib_effect,
      [AC_LANG_CONFTEST([AC_LANG_SOURCE([[int libfunc(void) { return 0; }]])])
       echo $CXX $CPPFLAGS $CXXFLAGS -c conftest.$ac_ext >&AS_MESSAGE_LOG_FD
       $CXX $CPPFLAGS $CXXFLAGS -c conftest.$ac_ext >&AS_MESSAGE_LOG_FD 2>&1
       echo $AR conftest.a conftest.o >&AS_MESSAGE_LOG_FD
       $AR conftest.a conftest.o >&AS_MESSAGE_LOG_FD 2>&1
       save_LIBS=$LIBS
       LIBS="conftest.a $LIBS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM([[extern int libfunc(void);]],
                                       [[return libfunc();]])],
                      [ok_without=true], [ok_without=false])
       echo $RANLIB conftest.a >&AS_MESSAGE_LOG_FD
       $RANLIB conftest.a >&AS_MESSAGE_LOG_FD
       AC_LINK_IFELSE([AC_LANG_PROGRAM([[extern int libfunc(void);]],
                                       [[return libfunc();]])],
                      [ok_with=true], [ok_with=false])
       LIBS=$save_LIBS
       case $ok_with:$ok_without in
          true:false)  ncbi_cv_prog_ranlib_effect=positive ;;
          false:true)  ncbi_cv_prog_ranlib_effect=negative ;;
          true:true)   ncbi_cv_prog_ranlib_effect=neutral  ;;
          false:false) ncbi_cv_prog_ranlib_effect=unknown  ;;
       esac
      ])
   case "$ncbi_cv_prog_ranlib_effect" in
      negative) RANLIB=: ;;
      unknown)
        AC_MSG_WARN([Failed to make a working library with or without ranlib.])
        ;;
   esac
fi
if test "$with_strip" = yes; then
   AC_CHECK_PROG(STRIP, strip, strip, @:)
else
   STRIP=@:
fi
case "$host_os" in
   irix* | aix5.[2-9]* )
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

AC_CHECK_PROG(CCACHE, ccache, ccache, [])
if test -n "$CCACHE" ; then
   CC_WRAPPER="$CC_WRAPPER $CCACHE"
   CXX_WRAPPER="$CXX_WRAPPER $CCACHE"
fi

# Note the path, since syntax is version-specific....
AC_PATH_PROG(TAIL, tail)
AC_MSG_CHECKING([whether $TAIL accepts modern syntax (-n N)])
if $TAIL -n 1 config.log >/dev/null 2>&1; then
   TAIL_N="$TAIL -n "
   AC_MSG_RESULT(yes)
else
   TAIL_N="$TAIL -"
   AC_MSG_RESULT(no)
fi

AC_PROG_EGREP
AC_MSG_CHECKING([how to run $EGREP quietly])
if test -z "`echo foo | $EGREP -q fo+ 2>>config.log || echo $?`"; then
   EGREP_Q="$EGREP -q"
elif test -z "`echo foo | $EGREP -s fo+ 2>>config.log || echo $?`"; then
   EGREP_Q="$EGREP -s"
else
   EGREP_Q=">/dev/null $EGREP"
fi
AC_MSG_RESULT($EGREP_Q)

#### Check if "${build_root}" is defined;  provide a default one
if test -n "${with_build_root}" ; then
   build_root=${with_build_root}
fi

### Possibly add the version to the build directory name, but preseve
### the original default when reconfiguring.
if test -z "${with_version}" -a -n "${ac_cv_header_stdc}"; then
   with_version=${ncbi_cv_with_version:-no}
fi
if test -z "${build_root}" ; then
   build_root="${srcdir}/${compiler}"
   case "$compiler:$with_version" in
      *[[0-9]]:* | *:no) ;;
      *) build_root=${build_root}${compiler_version} ;;
   esac
   ncbi_cv_with_version=${with_version:-yes}
else
   : ${with_suffix:="no"}
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


#### Profiling; try to turn on both function weight and basic-block counting
if test "$with_profiling" = "yes" ; then
   prof_sfx="Prof"
   case "$compiler" in
      Compaq)    PFFLAGS="-pg" ;;
      GCC)       PFFLAGS="-pg -fprofile-arcs" ;;
      ICC)       PFFLAGS="-p" ;;
      KCC)       PFFLAGS="-pg" ;;
      MIPSpro73) PFFLAGS="" ;; # pixie(1) adds info at runtime
      MSVC)      PFFLAGS="/PROFILE" ;;
      VisualAge) PFFLAGS="-pg" ;;
      WorkShop*) PFFLAGS="-pg -xprofile=tcov" ;;
      *)         PFFLAGS="-p" ;; # Fairly standard; shouldn't rely on -pg
   esac
   CFLAGS="$CFLAGS $PFFLAGS"
   CXXFLAGS="$CXXFLAGS $PFFLAGS"
   LDFLAGS="$LDFLAGS $PFFLAGS"
   STRIP="@:" # Stripping may lose profiling info
else
   prof_sfx=
fi


#### Determine whether this is implicitly a 64-bit platform
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF(size_t)
NCBI_PLATFORM_BITS=`expr 8 \* $ac_cv_sizeof_size_t`
AC_DEFINE_UNQUOTED(NCBI_PLATFORM_BITS, $NCBI_PLATFORM_BITS,
                   [Define to the architecture size.])
if test $NCBI_PLATFORM_BITS -eq 64; then
   bit64_sfx=64
fi


#### Add "Release"/"Debug", "MT", "DLL", "64", "_p" suffixes to "${build_root}"
if test "$with_suffix" != "no" ; then
   build_root="${build_root}-${release_debug}${mt_sfx}${dll_sfx}${bit64_sfx}${prof_sfx}"
fi


#### Add full host specs to "${build_root}"
if test "$with_hostspec" = "yes" ; then
   build_root="${build_root}--${host}"
fi

case "$host_os" in
   linux*)
      kver=`uname -r | sed -e 's/-.*//'`
      libcver=`ldd --version | awk 'NR == 1 { print $NF }'`
      verbose_host=$host_cpu-$host_vendor-linux$kver-gnu$libcver
      ;;
   *) verbose_host=$host ;;
esac

#### Overall configuration mode summary
signature="${compiler}_${compiler_version}-${release_debug}${mt_sfx}${dll_sfx}${bit64_sfx}${prof_sfx}--${verbose_host}-`(hostname || uname -n) 2>/dev/null | sed 1q`"


#### Check if there is "${build_root}" dir
test -d "${build_root}"  ||  mkdir -p "${build_root}"

#### Try to find a version of pwd that yields /net paths.
AC_PATH_PROG(AMQ, amq, [], $PATH:/usr/sbin:/sbin)
case "$AMQ" in
   */amq)
      AC_CACHE_CHECK([whether $AMQ -w works], ncbi_cv_prog_amq_w,
      [if $AMQ -w >/dev/null 2>&1; then
          ncbi_cv_prog_amq_w=yes
       else
          ncbi_cv_prog_amq_w=no
       fi])
      if test "$ncbi_cv_prog_amq_w" = yes; then
         smart_pwd="$AMQ -w"
      else
         smart_pwd=pwd
      fi
      ;;
   *)
      smart_pwd=pwd
      ;;
esac

#### Make "build_root" absolute
case "${build_root}" in
   /* ) ;;
   . )
      CDPATH=
      if test "${PWD}" != "" && test "`(cd ${PWD} ; sh -c pwd)`" = "`pwd`"  ;
      then
         build_root="$PWD"
      else
         build_root="`(cd ${build_root}; $smart_pwd)`"
      fi
   ;;
   *  ) build_root="`(cd ${build_root}; $smart_pwd)`" ;;
esac
if test -n "$with_fake_root"; then
   build_root=$with_fake_root/`basename $build_root`
fi


#### Set runpath (may depend on build root)
if test -z "$with_runpath"; then
   # Don't allow libdir values containing variable expansions,
   # including in particular the default "${exec_prefix}/lib"
   if test -n "$libdir"  &&  expr "$libdir" : '[[^$]]*$' >/dev/null; then
      with_runpath=$libdir
   elif test -n "$exec_prefix"  -a  "$exec_prefix" != "NONE" ; then
      with_runpath=$exec_prefix/lib
   elif test -n "$prefix"  -a  "$prefix" != "NONE" ; then
      with_runpath=$prefix/lib
   fi
fi

if   test "$with_runpath" = "no" ; then
   runpath=
   pure_runpath=
elif test -n "$with_runpath"  -a  "$with_runpath" != "yes" ; then
   root_base=`basename $build_root`
   pure_runpath=`echo ${with_runpath} | sed s/{}/$root_base/`
   runpath="${CONF_f_runpath}${pure_runpath}"
   case "$host_os:$compiler" in
    darwin* )
      # Needed for a few gui projects, including gbench_plugin_scan;
      # harmless elsewhere, since it only affects projects that indirectly
      # need one of these libraries.  (There seems to be no general
      # equivalent of -rpath-link.)
      for l in dbapi_driver gui_utils ncbi_xloader_genbank ncbi_xreader \
               ncbi_xreader_id1 ncbi_xreader_pubseqos; do
         ll=lib$l.dylib
         LDFLAGS="$LDFLAGS -dylib_file ${pure_runpath}/$ll:\$(libdir)/$ll"
      done
      ;;
    *:GCC )
      if `$CXX -print-prog-name=ld` --version 2>&1 | grep GNU >/dev/null; then
         runpath="$runpath -Wl,-rpath-link,\$(libdir)"
      fi
      ;;
    *:ICC )
      runpath="$runpath -Xlinker -rpath-link -Xlinker \$(libdir)"
      ;;
   esac
elif test "$with_runpath" = "yes"  -o  "$with_dll" != "no" ; then
   runpath="${CONF_f_runpath}\$(libdir)"
   pure_runpath='${build_root}/lib'
else
   runpath=
   pure_runpath=
fi
if test -z "$CONF_f_runpath" ; then
   # Drop the linker flag (since passing directory names doesn't work
   # so well), but keep pure_runpath for other uses.
   runpath=
fi


#### Linker command-line to build DLL (may depend on runpath)
has_dll_loadable="@# "
case "$compiler" in
 GCC | MIPSpro73 | ICC | Compaq )
   case "$host_os" in
      solaris* )
        # See http://realm.progsoc.uts.edu.au/~subtle/solaris28_link.html
        LINK_DLL="$LINK $ARCH_CFLAGS -shared -mimpure-text -o"
        ;;
      darwin* )
        # There is a distinction between shared libraries (dylibs)
        # and loadable objects (bundles).
        # See <http://fink.sourceforge.net/doc/porting/shared.php>.
        has_dll_loadable=""
        if $CXX -v 2>&1 | grep -q Apple; then
           LINK_DLL="$LINK $ARCH_CFLAGS -dynamiclib -flat_namespace -undefined suppress -install_name ${pure_runpath%%:*}/\$(XDLL) -o"
           LINK_LOADABLE="$LINK $ARCH_CFLAGS -bundle -flat_namespace -undefined suppress -o"
        else
           LINK_DLL="$CC $ARCH_CFLAGS -nostartfiles -Wl,-dynamic -Wl,-flat_namespace -Wl,-undefined,suppress -install_name ${pure_runpath%%:*}/\$(XDLL) -o"
           LINK_LOADABLE="$CC $ARCH_CFLAGS -nostartfiles -Wl,-dynamic -Wl,-bundle -Wl,-flat_namespace -Wl,-undefined,suppress -Wl,-read_only_relocs,warning -lbundle1.o -o"
        fi
        ;;
      * )
        LINK_DLL="$LINK $ARCH_CFLAGS -shared -o"
        ;;
   esac
   ;;
 WorkShop* )
   LINK_DLL="$LINK $ARCH_CFLAGS $MT_FLAG -G -h \$@ -o"
   ;;
 KCC )
   LINK_DLL="$LINK $ARCH_CFLAGS $MT_FLAG --soname \$@ -o"
   ;;
 VisualAge )
   # NOTE: --without-dll still recommended, as the linker doesn't seem
   # to support building shared libraries with undefined symbols.
   LINK_DLL="$LINK $ARCH_CFLAGS $MT_FLAG -qmkshrobj -o"
   ;;
 * )
   if test "$with_dll" = "yes"  &&  test -z "$LINK_DLL" ; then
      AC_MSG_ERROR(["Do not know how to build DLLs with compiler $CXX $compiler_ver"])
   fi
   ;;
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
        srcdir="`(cd ${srcdir}; $smart_pwd)`"
      fi
   ;;
   * ) srcdir="`(cd ${srcdir}; $smart_pwd)`" ;;
esac
real_srcdir=$srcdir
if test -n "$with_fake_root"; then
   srcdir=$build_root/..
fi


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
if test "$CONF_loadable_ext"  = " " ;  then loadable_ext="" ; else loadable_ext="${CONF_loadable_ext:=.so}" ; fi
if test "$CONF_lib_l_ext" = " " ;  then lib_l_ext="" ; else lib_l_ext="${CONF_lib_l_ext}" ; fi
if test "$CONF_exe_ext"   = " " ;  then exe_ext="" ; else exe_ext="${CONF_exe_ext}" ; fi
if test "$CONF_f_compile" = " " ;  then f_compile="" ; else f_compile="${CONF_f_compile:=-c }" ; fi
if test "$CONF_f_outobj"  = " " ;  then f_outobj="" ; else f_outobj="${CONF_f_outobj:=-o }" ; fi
if test "$CONF_f_outlib"  = " " ;  then f_outlib="" ; else f_outlib="${CONF_f_outlib}" ; fi
if test "$CONF_f_libpath" = " " ;  then f_libpath="" ; else f_libpath="${CONF_f_libpath:=-L}" ; fi
if test "$CONF_f_runpath" = " " ;  then f_runpath="" ; else f_runpath="${CONF_f_runpath:=$CONF_f_libpath}" ; fi
if test "$CONF_f_outexe"  = " " ;  then f_outexe="" ; else f_outexe="${CONF_f_outexe:=-o }" ; fi


BINCOPY="cp -p"
MAC_BINCOPY="/Developer/Tools/CpMac -p"
if test "$with_bincopy" != "yes"; then
   AC_CACHE_CHECK([whether cross-directory hard links work],
      ncbi_cv_sys_xdir_ln,
      [mkdir conftestdir
       touch conftestdir/conftestfile
       if ln -f conftestdir/conftestfile .; then
          ncbi_cv_sys_xdir_ln=yes
       else
          ncbi_cv_sys_xdir_ln=no
       fi
       rm -rf conftestdir conftestfile])
   if test "$ncbi_cv_sys_xdir_ln" = "yes"; then
      BINCOPY="ln -f"
      MAC_BINCOPY="\$(top_srcdir)/scripts/ln_mac.sh"
   fi
fi
ifdiff="${CONFIG_SHELL} \$(top_srcdir)/scripts/if_diff.sh"
if test "$OSTYPE" = "darwin" -a -x /Developer/Tools/CpMac; then
   BINCOPY="$ifdiff \"$MAC_BINCOPY\""
else
   BINCOPY="$ifdiff \"$BINCOPY\""
fi


#### Manage the debug-vs.-optimization flags ("--*-debug", "--*-optimization")
changequote(, )dnl
if test "$compiler" != "MSVC" ; then
   if test "$with_debug" = "no" ; then
      with_optimization=${with_optimization:="yes"}
      CFLAGS=`  echo " $CFLAGS"   | sed 's/[ 	]-g[0-9]*//g'`
      CXXFLAGS=`echo " $CXXFLAGS" | sed 's/[ 	]-g[0-9]*//g'`
      LDFLAGS=` echo " $LDFLAGS"  | sed 's/[ 	]-g[0-9]*//g'`
   else
      with_optimization=${with_optimization:="no"}
      if echo " $CFLAGS"   |grep -v >/dev/null '[ 	]-g[0-9]*' ; then
         CFLAGS="$CFLAGS -g"     ; fi
      if echo " $CXXFLAGS" |grep -v >/dev/null '[ 	]-g[0-9]*' ; then
         CXXFLAGS="$CXXFLAGS -g" ; fi
      if test "$with_optimization" = "no" ; then
         CPPFLAGS="-D_DEBUG $CPPFLAGS" ; fi
      if echo " $LDFLAGS"  |grep -v >/dev/null '[ 	]-g[0-9]*' ;
         then LDFLAGS="$LDFLAGS -g" ; fi
      STRIP="@:"
   fi

   NOPT_CFLAGS=`  echo " $CFLAGS"    | sed 's/[ 	]-O[0-9]*//g'`
   NOPT_CXXFLAGS=`echo " $CXXFLAGS"  | sed 's/[ 	]-O[0-9]*//g'`
   NOPT_LDFLAGS=` echo " $LDFLAGS"   | sed 's/[ 	]-O[0-9]*//g'`
   if test "$with_optimization" = "no" ; then
      CFLAGS="$NOPT_CFLAGS"
      CXXFLAGS="$NOPT_CXXFLAGS"
      LDFLAGS=` echo " $LDFLAGS"   | sed 's/[ 	]-O[0-9]*//g'`
      FAST_CFLAGS="$CFLAGS"
      FAST_CXXFLAGS="$CXXFLAGS"
      FAST_LDFLAGS="$LDFLAGS"
   else
      if echo " $CFLAGS"   |grep -v >/dev/null '[ 	]-O[0-9]*' ; then
         CFLAGS="$CFLAGS -O" ; fi
      if echo " $CXXFLAGS" |grep -v >/dev/null '[ 	]-O[0-9]*' ; then
         CXXFLAGS="$CXXFLAGS -O" ; fi
      CPPFLAGS="-DNDEBUG $CPPFLAGS"
      if echo " $LDFLAGS"  |grep -v >/dev/null '[ 	]-O[0-9]*' ; then
         LDFLAGS="$LDFLAGS -O" ; fi
      FAST_CFLAGS="$NOPT_CFLAGS $FAST_CFLAGS"
      FAST_CXXFLAGS="$NOPT_CXXFLAGS $FAST_CXXFLAGS"
      FAST_LDFLAGS="$NOPT_LDFLAGS $FAST_LDFLAGS"
   fi

   if test "$with_debug" = "no"  -a  "$with_optimization" = "no" ; then
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

AC_CHECK_HEADERS(inttypes.h limits)

### Check for the platform (very roughly)
AC_CHECK_HEADERS(unistd.h,  x_have_unistd="yes")
AC_CHECK_HEADERS(windows.h, x_have_windows="yes")

if test "$x_have_windows" = "yes" -a "x$host_os" != "xcygwin32" -a "x$host_os" != "xcygwin"; then
   AC_DEFINE(NCBI_OS_MSWIN, 1, [Define to 1 on Windows.])
   AC_DEFINE(NCBI_OS, "MSWIN", [Operating system name])
elif test "$x_have_unistd" = "yes" ; then
   AC_DEFINE(NCBI_OS_UNIX, 1,  [Define to 1 on Unix.])
   AC_DEFINE(NCBI_OS, "UNIX",  [Operating system name])

   UNIX_SRC='$(UNIX_SRC)'
   UNIX_USR_PROJ='$(UNIX_USR_PROJ)'
   WithFeatures="$WithFeatures unix"

   # Extra defines for variants we care about
   case "$OSTYPE" in
     aix)     AC_DEFINE(NCBI_OS_AIX,     1, [Define to 1 on AIX.])          ;;
     *bsd)    AC_DEFINE(NCBI_OS_BSD,     1, [Define to 1 on *BSD.])         ;;
     cygwin)  AC_DEFINE(NCBI_OS_CYGWIN,  1, [Define to 1 on Cygwin.])       ;;
     darwin)  AC_DEFINE(NCBI_OS_DARWIN,  1, [Define to 1 on Mac OS X.])     ;;
     irix)    AC_DEFINE(NCBI_OS_IRIX,    1, [Define to 1 on IRIX.])         ;;
     linux)   AC_DEFINE(NCBI_OS_LINUX,   1, [Define to 1 on Linux.])        ;;
     osf)     AC_DEFINE(NCBI_OS_OSF1,    1, [Define to 1 on Tru64 Unix.])   ;;
     solaris) AC_DEFINE(NCBI_OS_SOLARIS, 1, [Define to 1 on Solaris.])      ;;
   esac
else
   AC_DEFINE(NCBI_OS_MAC, 1, [Define to 1 on MacOS.])
   AC_DEFINE(NCBI_OS, "MAC", [Operating system name])
fi

# AC_CHECK_TYPES is insufficient, as linking may still fail :-/
AC_CACHE_CHECK([for wstring], ac_cv_type_wstring_linkable,
   [AC_LINK_IFELSE([AC_LANG_PROGRAM(
       [[#include <string>
         using std::wstring;]],
       [[wstring ws, ws2; ws.find_first_of(ws2);]])],
       [ac_cv_type_wstring_linkable=yes], [ac_cv_type_wstring_linkable=no])])
if test "$ac_cv_type_wstring_linkable" = "yes"; then
   AC_DEFINE(HAVE_WSTRING, 1,
      [Define to 1 if the system has the type `wstring'.])
fi

AC_CHECK_TYPES(socklen_t,,,[#include <sys/types.h>
#include <sys/socket.h>])
AC_CHECK_HEADERS(dlfcn.h ieeefp.h poll.h sys/sockio.h sys/sysinfo.h)
# for FreeTDS 0.60
AC_CHECK_HEADERS(arpa/inet.h malloc.h netinet/in.h sys/socket.h)

## gethostbyname_r() -- 2 different variants: 5-arg (Solaris), 6-arg (Linux).

AC_CACHE_CHECK([for gethostbyname_r], ncbi_cv_func_gethostbyname_r,
   [ncbi_cv_func_gethostbyname_r="no"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <netdb.h>]],
       [[
         void*           dummy = (void*) &gethostbyname_r;
         char            buf[1024];
         int             err;
         struct hostent* hp = 0;
         hp = gethostbyname_r("", hp, buf, sizeof(buf), &err);
       ]])],
       [
         ncbi_cv_func_gethostbyname_r="5"
       ]
    )
    if test "$ncbi_cv_func_gethostbyname_r" = "no" ; then
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <netdb.h>]],
       [[
         void*           dummy = (void*) &gethostbyname_r;
         char            buf[1024];
         int             err;
         struct hostent* hp = 0;
         err = gethostbyname_r("", hp, buf, sizeof(buf), &hp, &err);
       ]])],
       [
         ncbi_cv_func_gethostbyname_r="6"
       ]
    )
    fi])

if test "$ncbi_cv_func_gethostbyname_r" != "no" ; then
   AC_DEFINE_UNQUOTED(HAVE_GETHOSTBYNAME_R, $ncbi_cv_func_gethostbyname_r,
    [If you have the `gethostbyname_r' function, define to the number of
     arguments it takes (normally 5 or 6).])
fi


## gethostbyaddr_r() -- 2 different variants: 7-arg (Solaris), 8-arg (Linux).

AC_CACHE_CHECK([for gethostbyaddr_r], ncbi_cv_func_gethostbyaddr_r,
   [ncbi_cv_func_gethostbyaddr_r="no"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[
         #include <netdb.h>
         #include <sys/types.h>
         #include <sys/socket.h>
       ]],
       [[
         void* dummy = (void*) &gethostbyaddr_r;
         char            buf[1024];
         int             err;
         unsigned int    host;
         struct hostent* hp = 0;
         hp = gethostbyaddr_r((char *) &host, sizeof(host), AF_INET,
                              hp, buf, sizeof(buf), &err);
       ]])],
       [
         ncbi_cv_func_gethostbyaddr_r="7"
       ]
    )
    if test "$ncbi_cv_func_gethostbyaddr_r" = "no" ; then
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[
         #include <netdb.h>
         #include <sys/types.h>
         #include <sys/socket.h>
       ]],
       [[
         void* dummy = (void*) &gethostbyaddr_r;
         char            buf[1024];
         int             err;
         unsigned int    host;
         struct hostent* hp = 0;
         int res = gethostbyaddr_r((char *) &host, sizeof(host), AF_INET,
                                   hp, buf, sizeof(buf), &hp, &err);
       ]])],
       [
         ncbi_cv_func_gethostbyaddr_r="8"
       ]
    )
    fi])

if test "$ncbi_cv_func_gethostbyaddr_r" != "no" ; then
   AC_DEFINE_UNQUOTED(HAVE_GETHOSTBYADDR_R, $ncbi_cv_func_gethostbyaddr_r,
    [If you have the `gethostbyaddr_r' function, define to the number of
     arguments it takes (normally 7 or 8).])
fi


## getservbyname_r() -- 2 different variants: 5-arg (Solaris), 6-arg (Linux).

AC_CACHE_CHECK([for getservbyname_r], ncbi_cv_func_getservbyname_r,
   [ncbi_cv_func_getservbyname_r="no"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <netdb.h>]],
       [[
         void*           dummy = (void*) &getservbyname_r;
         char            buf[1024];
         int             err;
         struct servent* sp = 0;
         sp = getservbyname_r("", "", sp, buf, sizeof(buf));
       ]])],
       [
         ncbi_cv_func_getservbyname_r="5"
       ]
    )
    if test "$ncbi_cv_func_getservbyname_r" = "no" ; then
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <netdb.h>]],
       [[
         void*           dummy = (void*) &getservbyname_r;
         char            buf[1024];
         int             err;
         struct servent* sp = 0;
         err = getservbyname_r("", "", sp, buf, sizeof(buf), &sp);
       ]])],
       [
         ncbi_cv_func_getservbyname_r="6"
       ]
    )
    fi])

if test "$ncbi_cv_func_getservbyname_r" != "no" ; then
   AC_DEFINE_UNQUOTED(HAVE_GETSERVBYNAME_R, $ncbi_cv_func_getservbyname_r,
    [If you have the `getservbyname_r' function, define to the number of
     arguments it takes (normally 5 or 6).])
fi


AC_LANG_PUSH(C)
# some of these can lose in C++ due to the way autoconf works
AC_CHECK_FUNCS(localtime_r pthread_setconcurrency pthread_atfork)

AC_SEARCH_LIBS(sched_yield, rt,
    AC_DEFINE(HAVE_SCHED_YIELD, 1,
              [Define to 1 if you have the `sched_yield' function.])
)


### Check for other standard library functions

AC_FUNC_VPRINTF
AC_CHECK_FUNCS(strcasecmp strdup sysmp timegm atoll vasprintf vsnprintf \
               getpagesize)
AC_LANG_POP(C)

# We want the program to return 0, which is *false* in C.  Also, since
# use of lowercase seems to be customary if not universal, the default
# is yes when cross-compiling; anyone cross-compiling for exceptional
# platforms can set ncbi_cv_func_strcasecmp_lc=no in config.site.
if test "$ac_cv_func_strcasecmp" = yes; then
   AC_CACHE_CHECK([whether strcasecmp treats letters as lowercase],
                  ncbi_cv_func_strcasecmp_lc,
      [AC_RUN_IFELSE([AC_LANG_PROGRAM(
          [[#include <string.h>]],
          [[return strcasecmp("A", "_") <= 0;]])],
          [ncbi_cv_func_strcasecmp_lc=yes], [ncbi_cv_func_strcasecmp_lc=no],
          [ncbi_cv_func_strcasecmp_lc=yes])])
   if test "$ncbi_cv_func_strcasecmp_lc" = yes; then
      AC_DEFINE(HAVE_STRCASECMP_LC, 1,
                [Define to 1 if strcasecmp treats letters as lowercase.])
   fi
fi

AC_CACHE_CHECK([for Linux-like sysinfo with 1 arg], ncbi_cv_func_sysinfo_1,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <sys/sysinfo.h>]],
       [[
         void* dummy = (void*) &sysinfo;
         struct sysinfo* info = 0;
         int retval = sysinfo(info);
       ]])], [ncbi_cv_func_sysinfo_1=yes], [ncbi_cv_func_sysinfo_1=no])])

if test "$ncbi_cv_func_sysinfo_1" = yes; then
   AC_DEFINE(HAVE_SYSINFO_1, 1,
             [Define to 1 if Linux-like 1-arg sysinfo exists.])
fi

dnl Too much overkill, and now wants getloadavg.c to exist.
dnl AC_FUNC_GETLOADAVG
AC_CHECK_FUNCS(getloadavg)

## readdir_r() -- 2 different variants: 2-arg (trad. Solaris), 3-arg (POSIX).

AC_CACHE_CHECK([for readdir_r], ncbi_cv_func_readdir_r,
   [ncbi_cv_func_readdir_r="no"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <dirent.h>]],
       [[DIR* dir; struct dirent* entry; readdir_r(dir, entry, &entry);]])],
       [ncbi_cv_func_readdir_r=3])
    if test "$ncbi_cv_func_readdir_r" = "no" ; then
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <dirent.h>]],
       [[DIR* dir; struct dirent* entry; readdir_r(dir, entry);]])],
       [ncbi_cv_func_readdir_r=2])
    fi])

if test "$ncbi_cv_func_readdir_r" != "no" ; then
   AC_DEFINE_UNQUOTED(HAVE_READDIR_R, $ncbi_cv_func_readdir_r,
    [If you have the `readdir_r' function, define to the number of
     arguments it takes (normally 2 or 3).])
fi

### Check for typedefs, structures, and compiler features.
AC_C_CONST

### Check for C standard types and sizes
AC_C_BIGENDIAN
AC_C_CHAR_UNSIGNED
AC_CHECK_SIZEOF(void*)
AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(__int64)
AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(double)
AC_CHECK_SIZEOF(long double)
AC_CHECK_TYPES(intptr_t)

AC_CHECK_MEMBER(struct sockaddr_in.sin_len,
   [AC_DEFINE(HAVE_SIN_LEN, 1,
      [Define to 1 if `sin_len' is a member of `struct sockaddr_in'.])],
   [], [#include <sys/types.h>
        #include <netinet/in.h>])


AC_CACHE_CHECK([whether function prototypes can use throw(...)],
   ncbi_cv_cxx_throw_spec,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[template <class T> inline int test_throw_spec(const T&) throw() {
          return 0;
       }]],
       [[return test_throw_spec(123);]])],
       [ncbi_cv_cxx_throw_spec=yes], [ncbi_cv_cxx_throw_spec=no])])
if test "$ncbi_cv_cxx_throw_spec" = yes; then
   AC_DEFINE(NCBI_USE_THROW_SPEC, 1,
             [Define to 1 if prototypes can use exception specifications.])
fi


AC_CACHE_CHECK([for obsolete string::compare() syntax],
   ncbi_cv_func_string_compare_obsolete,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <string>]],
       [[std::string s;  int i = 0;  i = s.compare("aaa", 0, 2);]])],
       [ncbi_cv_func_string_compare_obsolete=yes],
       [ncbi_cv_func_string_compare_obsolete=no])])
if test "$ncbi_cv_func_string_compare_obsolete" = yes; then
   AC_DEFINE(NCBI_OBSOLETE_STR_COMPARE, 1,
             [Define to 1 if `string::compare()' is non-standard.])
fi


AC_CACHE_CHECK([whether the auto_ptr template class works],
   ncbi_cv_type_auto_ptr_works,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <memory>]],
       [[int *ip = 0; std::auto_ptr<int> ap(ip); *ip = *ap; ap.reset(ip);]])],
       [ncbi_cv_type_auto_ptr_works=yes], [ncbi_cv_type_auto_ptr_works=no])])
if test "$ncbi_cv_cxx_auto_ptr_works" = no; then
   AC_DEFINE(HAVE_NO_AUTO_PTR, 1,
             [Define to 1 if `auto_ptr<T>' is missing or broken.])
fi


AC_CACHE_CHECK([for min/max templates], ncbi_cv_func_min_max,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[
        #include <algorithm>
        #ifdef min
        #undef min
        #endif
        #ifdef max
        #undef max
        #endif
       ]],
       [[int a=0, b=1;  a = std::min(a, b);  b = std::max(a, b);]])],
       [ncbi_cv_func_min_max=yes], [ncbi_cv_func_min_max=no])])
if test "$ncbi_cv_func_min_max" = no; then
   AC_DEFINE(HAVE_NO_MINMAX_TEMPLATE, 1,
             [Define to 1 if `min'/`max' templates are not implemented.])
fi


AC_CACHE_CHECK([whether new C++ streams have ios_base::],
   ncbi_cv_type_ios_base,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <iostream>]],
       [[(void)(std::ios_base::out != std::ios_base::beg);]])],
       [ncbi_cv_type_ios_base=yes], [ncbi_cv_type_ios_base=no])])
if test "$ncbi_cv_type_ios_base" = no; then
   AC_DEFINE(HAVE_NO_IOS_BASE, 1,
             [Define to 1 if new C++ streams lack `ios_base::'.])
fi


AC_CACHE_CHECK([for ios(_base)::register_callback],
   ncbi_cv_func_ios_register_callback,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <iostream>]],
       [[std::cout.register_callback(0, std::ios::erase_event);]])],
       [ncbi_cv_func_ios_register_callback=yes],
       [ncbi_cv_func_ios_register_callback=no])])
if test "$ncbi_cv_func_ios_register_callback" = yes; then
   AC_DEFINE(HAVE_IOS_REGISTER_CALLBACK, 1,
             [Define to 1 if you have `ios(_base)::register_callback'.])
fi


AC_CACHE_CHECK([whether C++ streams delete their buffers],
   ncbi_cv_cxx_auto_sb_dest,
   [AC_RUN_IFELSE([AC_LANG_PROGRAM(
       [[#include <iostream>
         int status = 1;
         class mybuf : public std::streambuf
         {
         public:
             ~mybuf() { status = 0; }
         };
         class mystream : public std::iostream
         {
         public:
               mystream() { init(new mybuf); }
         };
         ]],
       [[{ mystream s; }  return status;]])],
       [ncbi_cv_cxx_auto_sb_dest=yes], [ncbi_cv_cxx_auto_sb_dest=no],
       [ncbi_cv_cxx_auto_sb_dest=no])])
# Rare, so assume "no" when cross-compiling
if test "$ncbi_cv_cxx_auto_sb_dest" = yes; then
   AC_DEFINE(AUTOMATIC_STREAMBUF_DESTRUCTION, 1,
             [Define to 1 if C++ streams delete their buffers.])
fi



AC_CACHE_CHECK([for std::char_traits::], ncbi_cv_type_char_traits,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <iostream>]],
       [[using namespace std;
       char_traits<char>::int_type  i = char_traits<char>::eof();
       i = char_traits<char>::not_eof(i);
       char_traits<char>::char_type c = '\0';
       char_traits<char>::pos_type  p = 0;
       char_traits<char>::off_type  o = 1;
       ]])],
       [ncbi_cv_type_char_traits=yes], [ncbi_cv_type_char_traits=no])])
if test "$ncbi_cv_type_char_traits" = no; then
   AC_DEFINE(HAVE_NO_CHAR_TRAITS, 1,
             [Define to 1 if `std::char_traits' is missing.])
fi



AC_CACHE_CHECK([for SysV semaphores], ncbi_cv_sys_semaphores,
   AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <sys/types.h>
#include <sys/sem.h>],
      [struct sembuf buf; int id = semget(0x1234, 0, IPC_CREAT);
       buf.sem_op = SEM_UNDO;])],
      [ncbi_cv_sys_semaphores=yes], [ncbi_cv_sys_semaphores=no]))
if test "$ncbi_cv_sys_semaphores" = yes; then
   AC_DEFINE(HAVE_SYSV_SEMAPHORES, 1,
             [Define to 1 if you have SysV semaphores.])
fi

AC_CHECK_TYPE([union semun],
   AC_DEFINE(HAVE_SEMUN, 1, [Define to 1 if you have `union semun'.]),
   [], [#include <sys/types.h>
#include <sys/sem.h>])


AC_CACHE_CHECK([which way the stack grows], ncbi_cv_sys_stack_dir,
   [AC_RUN_IFELSE([AC_LANG_PROGRAM(
        [[ int f(int* p) { int i; return (&i < p) ? 1 : 0; } ]],
        [[ int p; return f(&p); ]])],
        [ncbi_cv_sys_stack_dir=up], [ncbi_cv_sys_stack_dir=down],
        [ncbi_cv_sys_stack_dir="unknown (cross-compiling)"])])
case "$ncbi_cv_sys_stack_dir" in
 up)   AC_DEFINE(STACK_GROWS_UP,   1, [Define to 1 if the stack grows up.])  ;;
 down) AC_DEFINE(STACK_GROWS_DOWN, 1, [Define to 1 if the stack grows down.]);;
esac


AC_CACHE_CHECK([whether the preprocessor supports C99-style variadic macros],
   ncbi_cv_cpp_std_varargs,
   [AC_PREPROC_IFELSE(
       [[#define foo(...) 1
         #if foo(x,y,z) != 1
         #error failed
         #endif]],
       [ncbi_cv_cpp_std_varargs=yes], [ncbi_cv_cpp_std_varargs=no])])
if test "$ncbi_cv_cpp_std_varargs" = yes; then
   AC_DEFINE(HAVE_CPP_STD_VARARGS, 1,
      [Define to 1 if the preprocessor supports C99-style variadic macros.])
fi

AC_CACHE_CHECK([whether the preprocessor supports GNU-style variadic macros],
   ncbi_cv_cpp_gnu_varargs,
   [AC_PREPROC_IFELSE(
       [[#define foo(args...) 1
         #if foo(x,y,z) != 1
         #error failed
         #endif]],
       [ncbi_cv_cpp_gnu_varargs=yes], [ncbi_cv_cpp_gnu_varargs=no])])
if test "$ncbi_cv_cpp_gnu_varargs" = yes; then
   AC_DEFINE(HAVE_CPP_GNU_VARARGS, 1,
      [Define to 1 if the preprocessor supports GNU-style variadic macros.])
fi


# for FreeTDS
AC_CACHE_CHECK([whether FIONBIO requires BSD_COMP to be defined],
   ncbi_cv_decl_fionbio_needs_bsd_comp,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[ #include <sys/types.h>
          #include <sys/ioctl.h> ]],
       [[ int request = FIONBIO; ]])],
       [ncbi_cv_decl_fionbio_needs_bsd_comp=no],
       [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
           [[ #define BSD_COMP 1
              #include <sys/types.h>
              #include <sys/ioctl.h> ]],
           [[ int request = FIONBIO; ]])],
           [ncbi_cv_decl_fionbio_needs_bsd_comp=yes],
           [ncbi_cv_decl_fionbio_needs_bsd_comp="never defined at all"])])])
if test "$ncbi_cv_decl_fionbio_needs_bsd_comp" = yes; then
   AC_DEFINE(BSD_COMP, 1,
             [Define to 1 if necessary to get FIONBIO (e.g., on Solaris)])
fi

AC_CACHE_CHECK([whether INADDR_NONE is defined], ncbi_cv_decl_inaddr_none,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[
         #ifdef HAVE_SYS_TYPES_H
         #include <sys/types.h>
         #endif
         #ifdef HAVE_SYS_SOCKET_H
         #include <sys/socket.h>
         #endif
         #ifdef HAVE_NETINET_IN_H
         #include <netinet/in.h>
         #endif
         #ifdef HAVE_ARPA_INET_H
         #include <arpa/inet.h>
         #endif
       ]],
       [[ unsigned long addr = INADDR_NONE; ]])],
       [ncbi_cv_decl_inaddr_none=yes], [ncbi_cv_decl_inaddr_none=no])])
if test "$ncbi_cv_decl_inaddr_none" != yes; then
   AC_DEFINE(INADDR_NONE, 0xffffffff,
             [Define to 0xffffffff if your operating system doesn't.])
fi

### Check for the availability of other packages
### --------------------------------------------


### Common-use system and miscellaneous libs
if test -z "${NETWORK_LIBS+set}"; then
   AC_CHECK_LIB(nsl, gethostbyname, [libnsl=-lnsl], [libnsl=])
   AC_CHECK_LIB(socket, connect, [NETWORK_LIBS="-lsocket $libnsl"],
      [NETWORK_LIBS=$libnsl])
fi
AC_CACHE_CHECK([for res_search in -lresolv], ac_cv_lib_resolv_res_search,
   [orig_LIBS=$LIBS
    LIBS="-lresolv $NETWORK_LIBS $orig_LIBS"
    AC_LINK_IFELSE([AC_LANG_PROGRAM(
       [[#include <resolv.h>]],
       [[unsigned char ans[256];
         int st = res_search("foo.bar", 1, 1, ans, sizeof(ans));]])],
       [ac_cv_lib_resolv_res_search=yes], [ac_cv_lib_resolv_res_search=no])
    LIBS=$orig_LIBS])
if test "$ac_cv_lib_resolv_res_search" = "yes"; then
   RESOLVER_LIBS=-lresolv
else
   RESOLVER_LIBS=
fi

if test "$with_ncbi_public" = "yes"  -a  "$OSTYPE" = "solaris"; then
   AC_MSG_NOTICE([Skipping checks for get{addr,name}info (new in Solaris 8).])
else
   orig_LIBS=$LIBS
   LIBS="$NETWORK_LIBS $orig_LIBS"
   AC_CACHE_CHECK([for getaddrinfo], ac_cv_func_getaddrinfo,
      [AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <netdb.h>]],
          [[struct addrinfo *p; getaddrinfo("foo", "bar", p, &p);]])],
          [ac_cv_func_getaddrinfo=yes], [ac_cv_func_getaddrinfo=no])])
   test "$ac_cv_func_getaddrinfo" = "yes" && \
     AC_DEFINE(HAVE_GETADDRINFO, 1,
               [Define to 1 if you have the `getaddrinfo' function.])
   AC_CACHE_CHECK([for getnameinfo], ac_cv_func_getnameinfo,
      [AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <netdb.h>
            #include <sys/types.h>
            #include <sys/socket.h>]],
          [[struct sockaddr p; char buf[256], buf2[256];
            getnameinfo(&p, sizeof(p), buf, 256, buf2, 256, 0);]])],
          [ac_cv_func_getnameinfo=yes], [ac_cv_func_getnameinfo=no])])
   test "$ac_cv_func_getnameinfo" = "yes" && \
     AC_DEFINE(HAVE_GETNAMEINFO, 1,
               [Define to 1 if you have the `getnameinfo' function.])
   LIBS=$orig_LIBS
fi

MATH_LIBS=${MATH_LIBS:="-lm"}


### Math lib to be always linked in
LIBS="$MATH_LIBS $LIBS"
C_LIBS="$MATH_LIBS $C_LIBS"


### Save original compiler/linker flags
orig_CPPFLAGS="$CPPFLAGS"
orig_LDFLAGS="$LDFLAGS"
orig_LIBS="$LIBS"


NCBI_CHECK_LIBS(KSTAT, kstat, kstat_open)
NCBI_CHECK_LIBS(RPCSVC, rpcsvc, rstat)
AC_LANG_PUSH(C)
# Force C for this test because Sun WorkShop otherwise complains about
# multiple extern "C" declarations of setkey.
NCBI_CHECK_LIBS(CRYPT, crypt crypt_i, setkey)
AC_LANG_POP(C)
case "$host_os:$compiler" in
     solaris*:GCC ) ac_cv_search_dlopen="-ldl" ;;
esac
NCBI_CHECK_LIBS(DL, dl, dlopen)
# Normally redundant, but needed --with-static-exe.
# (Too bad there's no libdl.a.)
case "$host_os:$libnsl" in
     solaris*:-lnsl ) NETWORK_LIBS="$NETWORK_LIBS $DL_LIBS" ;;
esac
NCBI_CHECK_LIBS(RT, rt posix4, clock_gettime)
dnl NCBI_CHECK_LIBS(ICONV, iconv, iconv)
# We need to include <iconv.h>, since some implementations rename the symbols.
# This logic adapted from Bruno Haible's iconv.m4.
AC_CACHE_CHECK(for library containing iconv, ac_cv_search_iconv, [
   ac_cv_search_iconv=no
   AC_TRY_LINK([#include <stdlib.h>
#include <iconv.h>],
     [iconv_t cd = iconv_open("","");
      iconv(cd,NULL,NULL,NULL,NULL);
      iconv_close(cd);],
     ac_cv_search_iconv="none required")
   if test "$ac_cv_search_iconv" != "none required"; then
      for l in iconv; do
         LIBS="-l$l $orig_LIBS"
         AC_TRY_LINK([#include <stdlib.h>
#include <iconv.h>],
           [iconv_t cd = iconv_open("","");
            iconv(cd,NULL,NULL,NULL,NULL);
            iconv_close(cd);],
           ac_cv_search_iconv=-l$l)
         LIBS="$orig_LIBS"
      done
   fi])
if test "x$ac_cv_search_iconv" != xno; then
   AC_DEFINE(HAVE_LIBICONV, 1,
   [Define to 1 if ICONV is available, either in its own library or as part
    of the standard libraries.])
   if test "x$ac_cv_search_iconv" != "xnone required"; then
      ICONV_LIBS=$ac_cv_search_iconv
   fi
fi


### Third-party libraries
NCBI_CHECK_THIRD_PARTY_LIB(z,
 [[AC_LANG_PROGRAM([#include <zlib.h>],
      [[gzFile fp; char buf[1024]; int n = gzread(fp, buf, sizeof(buf))]])]])
if test -z "$Z_LIBS"; then
   zlocal=util/compress/zlib
   AC_MSG_NOTICE([using local copy in $zlocal])
   Z_PATH="<$zlocal>"
   Z_INCLUDE="-I\$(includedir)/$zlocal -I\$(includedir0)/$zlocal"
   # Z_LIBS="-lz"
   Z_LIB="z"
   # AC_DEFINE(USE_LOCAL_ZLIB, 1, [Define to 1 if using a local copy of zlib.])
   WithPackages="$WithPackages Z LocalZ"
   WithoutPackages=`echo "$WithoutPackages" | sed -e 's/ Z//'`
fi

NCBI_CHECK_THIRD_PARTY_LIB(bz2,
 [[AC_LANG_PROGRAM([#include <bzlib.h>],
      [[BZFILE* fp; char buf[1024]; int err;
        int n = BZ2_bzRead(&err, fp, buf, sizeof(buf))]])]])
if test -z "$BZ2_LIBS"; then
   bzlocal=util/compress/bzip2
   AC_MSG_NOTICE([using local copy in $bzlocal])
   BZ2_PATH="<$bzlocal>"
   BZ2_INCLUDE="-I\$(includedir)/$bzlocal -I\$(includedir0)/$bzlocal"
   # BZ2_LIBS="-lbz2"
   BZ2_LIB="bz2"
   AC_DEFINE(USE_LOCAL_BZLIB, 1, [Define to 1 if using a local copy of bzlib.])
   WithPackages="$WithPackages BZ2 LocalBZ2"
   WithoutPackages=`echo "$WithoutPackages" | sed -e 's/ BZ2//'`
fi

if test -z "$PCRE_PATH"  &&  pcre-config --version >/dev/null 2>&1; then
    p=`pcre-config --prefix`
    test "x$p" = "x/usr"  ||  PCRE_PATH=$p
fi
NCBI_CHECK_THIRD_PARTY_LIB(pcre,
 [[AC_LANG_PROGRAM([#include <pcre.h>],
   [[const char*s[]={"x"}; pcre* p; pcre_extra* x = pcre_study(p, 1, s);]])]])
if test -z "$PCRE_LIBS"; then
   pcrelocal=util/regexp
   AC_MSG_NOTICE([using local copy in $pcrelocal])
   PCRE_PATH="<$pcrelocal>"
   PCRE_INCLUDE="-I\$(includedir)/$pcrelocal -I\$(includedir0)/$pcrelocal"
   # PCRE_LIBS="-lregexp"
   # PCREPOSIX_LIBS="-lregexp"
   PCRE_LIB="regexp"
   AC_DEFINE(USE_LOCAL_PCRE, 1, [Define to 1 if using a local copy of PCRE.])
   WithPackages="$WithPackages PCRE LocalPCRE"
   WithoutPackages=`echo "$WithoutPackages" | sed -e 's/ PCRE//'`
else
   PCREPOSIX_LIBS=`echo "$PCRE_LIBS" | sed -e 's/-lpcre/-lpcreposix -lpcre/'`
fi

## SYBASE libraries

NETWORK_PURE_LIBS="$NETWORK_LIBS"
if test "$with_sybase" != "no" ; then
   net_sybase_root="/netopt/Sybase/clients"
   local_sybase_root="/export/home/sybase/clients"
   default_local_sybase="$local_sybase_root/current"
   sybase_lib=lib$bit64_sfx
   : ${with_sybase_local:="no"}
   : ${with_sybase_new:="no"}

   if test "$with_sybase_local" != "no" ; then
      if test "$with_sybase_local" = "yes" ; then
         with_sybase_local=${SYBASE_PATH:-$default_local_sybase}
      fi
      AC_CACHE_CHECK([for local Sybase install in $with_sybase_local],
         ncbi_cv_path_sybase_local,
         [if test -d "$with_sybase_local"; then
             ncbi_cv_path_sybase_local=yes
          else
             ncbi_cv_path_sybase_local=no
          fi])
      if test "$ncbi_cv_path_sybase_local" = yes; then
         SYBASE_PATH=$with_sybase_local
      else
         with_sybase_local=no
      fi
   fi
   if test "$with_sybase_local" = "no" ; then
      if test "$with_sybase_new" = "yes" ; then
         if test -d "$net_sybase_root/new${bit64_sfx:-32}"; then
             new_version=new${bit64_sfx:-32}
         elif test -d "$net_sybase_root/new"; then
             new_version=new
         fi
         if test -n "$new_version"; then
            new_sybase_path=$net_sybase_root/$new_version
            new_sybase_local=$local_sybase_root/$new_version
            AC_CACHE_CHECK([for new Sybase install in $new_sybase_local:$new_sybase_path],
               ncbi_cv_path_sybase_new,
               [if test -d $new_sybase_path; then
                   if test -d $new_sybase_local; then
                      ncbi_cv_path_sybase_new=yes
                   else
                      ncbi_cv_path_sybase_new=net-only
                   fi
                else
                   ncbi_cv_path_sybase_new=no
                fi])
            if test "$ncbi_cv_path_sybase_new" = no; then
               with_sybase_new=no
            else
               SYBASE_PATH=$new_sybase_path
               sybase_lib=lib
               if test "$ncbi_cv_path_sybase_new" = yes; then
                  SYBASE_LCL_PATH=$new_sybase_local
               fi
            fi
         fi # New Sybase potentially available?
      fi # New Sybase requested?
      if test "$with_sybase_new" = "no" ; then
         : ${SYBASE_PATH:="$net_sybase_root/current"}
         : ${SYBASE_LCL_PATH:="$local_sybase_root/current"}
      fi
   fi # No local Sybase (requested)?

   # Lock in the version we found
   test -d "$SYBASE_PATH"  &&  SYBASE_PATH=`cd $SYBASE_PATH && /bin/pwd`
   if test -d "$SYBASE_LCL_PATH"; then
      SYBASE_LCL_PATH=`cd $SYBASE_LCL_PATH && /bin/pwd`
   elif test -n "$SYBASE_LCL_PATH"; then # assume identity
      SYBASE_LCL_PATH=`dirname $SYBASE_LCL_PATH`/`basename $SYBASE_PATH`
   fi

   SYBASE_LIBPATH="$SYBASE_PATH/${sybase_lib}"
   syb_sfx=$bit64_sfx
   if test "$with_mt" = "yes" ; then
      AC_CACHE_CHECK([for reentrant Sybase libraries], ncbi_cv_lib_sybase_r,
         [if test -f "$SYBASE_LIBPATH/libct_r$bit64_sfx.so"; then
             ncbi_cv_lib_sybase_r=yes
          else
             ncbi_cv_lib_sybase_r=no
          fi])
      if test $ncbi_cv_lib_sybase_r = yes; then
         syb_sfx=_r$bit64_sfx
      fi
   fi
   case "$host_os" in
      linux*) sybtcl=-lsybtcl$syb_sfx; sybtli="" ;;
      *)      sybtcl=-ltcl$syb_sfx;    sybtli=-ltli$syb_sfx ;;
   esac

   # quick-patch for the bad "tli" lib (Solaris8/Intel specific)
   case "$host_os:$host_vendor" in
      solaris*:pc )  : ${sybase_sol8pc_patch:="yes"} ;;
   esac
   if test "$sybase_sol8pc_patch" = "yes" ; then
      sybtli="-ltli_orig$syb_sfx -ltli$syb_sfx"
      SYBASE_NETWORK_LIBS="-ltli$syb_sfx $NETWORK_LIBS"
   else
      SYBASE_NETWORK_LIBS="$NETWORK_LIBS"
   fi

   test -n "$bit64_sfx"  &&  SYBASE_INCLUDE="-DSYB_LP64"
   SYBASE_LIBLIST="-lblk$syb_sfx -lct$syb_sfx -lcs$syb_sfx $sybtcl"
   SYBASE_LIBLIST="$SYBASE_LIBLIST -lcomn$syb_sfx -lintl$syb_sfx"
   SYBASE_DLLLIST="$sybtli"

   # deduce conf.macro: SYBASE_INCLUDE, SYBASE_LIBS, SYBASE_DLLS, SYBASE_DBLIBS
   SYBASE_INCLUDE="-I$SYBASE_PATH/include $SYBASE_INCLUDE"
   if test -z "$SYBASE_LCL_PATH" ; then
      SYBASE_LPATH="-L$SYBASE_LIBPATH ${CONF_f_runpath}$SYBASE_LIBPATH"
   else
      SYBASE_LCL_LIBPATH="$SYBASE_LCL_PATH/${sybase_lib}"
      SYBASE_LPATH="-L$SYBASE_LCL_LIBPATH -L$SYBASE_LIBPATH ${CONF_f_runpath}$SYBASE_LCL_LIBPATH:$SYBASE_LIBPATH"
      SYBASE_INCLUDE="-I$SYBASE_LCL_PATH/include $SYBASE_INCLUDE"
   fi
   SYBASE_LIBS="$SYBASE_LPATH $SYBASE_LIBLIST"
   SYBASE_DBLIBS="$SYBASE_LPATH $SYBASE_DBLIBS"
   SYBASE_DLLS="$SYBASE_DLLLIST"

   AC_CACHE_CHECK([for Sybase in $SYBASE_PATH], ncbi_cv_lib_sybase,
      [CPPFLAGS="$SYBASE_INCLUDE $orig_CPPFLAGS"
       LIBS="$SYBASE_LIBS $SYBASE_DLLS $SYBASE_NETWORK_LIBS $DL_LIBS $orig_LIBS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <ctpublic.h>]],
          [[CS_RETCODE i = ct_init(0,0);  i = cs_ctx_global(0,0); ]])],
          [ncbi_cv_lib_sybase=yes], [ncbi_cv_lib_sybase=no])])

   if test "$ncbi_cv_lib_sybase" = yes; then
      WithPackages="$WithPackages Sybase"
      AC_DEFINE(HAVE_LIBSYBASE, 1,
                [Define to 1 if SYBASE libraries are available.])
      if test "$with_mt" = "yes" -a "$ncbi_cv_lib_sybase_r" = "yes"; then
         AC_DEFINE(HAVE_SYBASE_REENTRANT, 1,
                   [Define to 1 if SYBASE has reentrant libraries.])
      fi
      AC_CACHE_CHECK([for PubSeq service availability], ncbi_cv_db_pubseq,
         [if grep PUBSEQ_OS $SYBASE_PATH/interfaces >/dev/null; then
             ncbi_cv_db_pubseq=yes
          else
             ncbi_cv_db_pubseq=no
          fi])
      if test "$ncbi_cv_db_pubseq" = yes; then
         AC_DEFINE(HAVE_PUBSEQ_OS, 1,
                   [Define to 1 if the PUBSEQ service is available.])
      fi

      SYBASE_DBLIBS="$SYBASE_LPATH -lsybdb${bit64_sfx}"
      AC_CACHE_CHECK([for Sybase DBLib], ncbi_cv_lib_sybdb,
         [CPPFLAGS="$SYBASE_INCLUDE $prig_CPPFLAGS"
          LIBS="$SYBASE_DBLIBS $SYBASE_LIBS $SYBASE_DLLS $SYBASE_NETWORK_LIBS $DL_LIBS $orig_LIBS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM(
             [[
              #include <sybfront.h>
              #include <sybdb.h>
             ]],
             [[
              LOGINREC*  x_login     = 0;
              DBPROCESS* x_dbproc    = dbopen(x_login, (char*) "SRV_NAME");
             ]])],
             [ncbi_cv_lib_sybdb=yes], [ncbi_cv_lib_sybdb=no])])
      if test "$ncbi_cv_lib_sybdb" = yes; then
         WithPackages="$WithPackages DBLib"
         AC_DEFINE(HAVE_LIBSYBDB, 1,
                   [Define to 1 if SYBASE DBLib is available.])
      else
         WithoutPackages="$WithoutPackages DBLib"
         SYBASE_DBLIBS=
      fi
      # apply the Sybase patch to $NETWORK_LIBS
      if test "$sybase_sol8pc_patch" = "yes" ; then
         NETWORK_LIBS="$SYBASE_LPATH $SYBASE_NETWORK_LIBS"
      fi
   else
      with_sybase=no
   fi
fi # with_sybase != no?

if test "$with_sybase" = no; then
   WithoutPackages="$WithoutPackages Sybase DBLib"
   SYBASE_PATH="No_Sybase"
   SYBASE_INCLUDE=
   SYBASE_LIBS=
   SYBASE_DLLS=
   SYBASE_DBLIBS=
fi

SYBASE_LPATH=
SYBASE_LCL_LIBPATH=
SYBASE_LIBLIST=
SYBASE_DLLLIST=
SYBASE_LIBPATH=


## FreeTDS libraries
freetds=
if test "$with_ftds" != "no" ; then
   if test -z "$with_ftds"  -o  "$with_ftds" = "yes" ; then
      with_ftds=8
   fi
   if test "$with_ftds" = 8 ; then
      FTDS8_LOCAL_PATH="dbapi/driver/ftds8/freetds"
      if test -f ${real_srcdir}/src/$FTDS8_LOCAL_PATH/Makefile.in ; then
         FTDS8_LIB="sybdb_ftds${STATIC} tds_ftds${STATIC}"
         FTDS8_PATH="<$FTDS8_LOCAL_PATH>"
         FTDS8_INCLUDE="-I\$(includedir)/$FTDS8_LOCAL_PATH -I\$(includedir0)/$FTDS8_LOCAL_PATH"
      fi
      test -n "$FTDS8_PATH" && FTDS_PATH=$FTDS8_PATH && freetds="freetds"
   else
      FTDS_PATH="$with_ftds"
   fi
   if test -n "$freetds" ; then
      FTDS_LIB="\$(FTDS${with_ftds}_LIB)"
      FTDS_INCLUDE="\$(FTDS${with_ftds}_INCLUDE)"
      if test "$with_ftds_renamed" != "no" ; then
          AC_DEFINE(NCBI_FTDS_RENAME_SYBDB, 1,
                    [Rename DBLIB symbols in FTDS to avoid name clash with Sybase DBLIB.])
      fi
      AC_MSG_NOTICE([Using bundled FreeTDS (protocol v$with_ftds) from $FTDS_PATH])
   else
      FTDS_PATH=${FTDS_PATH:="/netopt/Sybase/clients-mssql/current"}
      FTDS_LIBS=${FTDS_LIBS:="-lsybdb -ltds"}
      FTDS_LIBS="-L$FTDS_PATH/lib ${CONF_f_runpath}$FTDS_PATH/lib $FTDS_LIBS"
      FTDS_INCLUDE="-I$FTDS_PATH/include"

      AC_CACHE_CHECK([for FreeTDS in $FTDS_PATH], ncbi_cv_lib_freetds,
         [CPPFLAGS="$FTDS_INCLUDE $orig_CPPFLAGS"
          LIBS="$FTDS_LIBS $NETWORK_LIBS $orig_LIBS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM(
            [[
             #include <sybdb.h>
             #include <tds.h>
            ]],
            [[
             LOGINREC*  x_login     = 0;
             DBPROCESS* x_dbproc    = dbopen(x_login, (char*) "SRV_NAME");
             TDSLOGIN*  x_tds_login = 0;
             /* TDSSOCKET* x_sock      = tds_connect(x_tds_login); */
             tds_set_timeouts(x_tds_login, 0, 0, 0);
            ]])],
            [ncbi_cv_lib_freetds=yes], [ncbi_cv_lib_freetds=no])])
      if test "$ncbi_cv_lib_freetds" = "no"; then
         with_ftds=no
      else
         FTDS8_INCLUDE=$FTDS_INCLUDE
         FTDS8_LIBS=$FTDS_LIBS
      fi
   fi
fi
if test "$with_ftds" = "no" ; then
   FTDS_PATH=
   FTDS_INCLUDE=
   FTDS_LIBS=
   FTDS_LIB=
   FTDS8_INCLUDE=
   FTDS8_LIBS=
   FTDS8_LIB=
   freetds=
   WithoutPackages="$WithoutPackages FreeTDS"
else
   AC_DEFINE(HAVE_LIBFTDS, 1,
             [Define to 1 if FreeTDS libraries are available.])
   WithPackages="$WithPackages FreeTDS"
fi
AC_SUBST(FTDS8_INCLUDE)
AC_SUBST(FTDS8_LIBS)
AC_SUBST(FTDS8_LIB)
AC_SUBST(FTDS_INCLUDE)
AC_SUBST(FTDS_LIBS)
AC_SUBST(FTDS_LIB)
AC_SUBST(freetds)


## MySQL
if test "$with_mysql" != "no" ; then
   : ${MYSQL_BINPATH=${MYSQL_PATH-/netopt/mysql/current}/bin}
   AC_PATH_PROG(mysql_config, mysql_config, [], [$MYSQL_BINPATH:$PATH])
   if test -n "$mysql_config" ; then
      : ${MYSQL_BINPATH=`dirname $mysql_config`}
      # Kill off single quotes, due to later requoting
      : ${MYSQL_INCLUDE=`$mysql_config --cflags | tr -d \'`}
      : ${MYSQL_LIBS=`$mysql_config --libs |\
             sed -e "s/'//g; s/-L\\([[^ ]]*\\)/-L\\1 ${CONF_f_runpath}\\1/"`}
      AC_CACHE_CHECK([whether MySQL works], ncbi_cv_lib_mysql,
         [CPPFLAGS="$MYSQL_INCLUDE $orig_CPPFLAGS"
          LIBS="$MYSQL_LIBS $orig_LIBS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <mysql.h>]],
          [[MYSQL *p;  p = mysql_init(p);]])],
          [ncbi_cv_lib_mysql=yes], [ncbi_cv_lib_mysql=no])])
      if test "$ncbi_cv_lib_mysql" = no; then
         with_mysql=no
      fi
   else
      with_mysql=no
   fi
fi
if test "$with_mysql" = "no" ; then
   MYSQL_PATH="No_MYSQL"
   MYSQL_INCLUDE=
   MYSQL_LIBS=
   WithoutPackages="$WithoutPackages MySQL"
else
   WithPackages="$WithPackages MySQL"
   AC_DEFINE(HAVE_MYSQL, 1, [Define to 1 if MySQL is available.])
fi


## Berkeley DB library
if test "$with_bdb" != "no" ; then
   case "$with_bdb" in
      yes | "" )
         : ${BERKELEYDB_PATH:="$NCBI/BerkeleyDB"}
         ;;
      */*)
          BERKELEYDB_PATH=$with_bdb
          AS_UNSET(BERKELEYDB_INCLUDE)
          AS_UNSET(BERKELEYDB_LIBPATH)
          AS_UNSET(BERKELEYDB_LIBS)
          ;;
      *)
          BERKELEYDB_PATH=$NCBI/BerkeleyDB-${with_bdb}
          AS_UNSET(BERKELEYDB_INCLUDE)
          AS_UNSET(BERKELEYDB_LIBPATH)
          AS_UNSET(BERKELEYDB_LIBS)
          ;;
   esac
   : ${BERKELEYDB_INCLUDE:="-I$BERKELEYDB_PATH/include"}
   if test -z "${BERKELEYDB_LIBPATH}"; then
      for d in "${release_debug}${bit64_sfx}" "lib${bit64_sfx}" lib; do
         BERKELEYDB_LIBPATH=${BERKELEYDB_PATH}/$d
         test -d ${BERKELEYDB_LIBPATH}  &&  break
      done
   fi
   : ${BERKELEYDB_LIBS:="-L${BERKELEYDB_LIBPATH} ${CONF_f_runpath}${BERKELEYDB_LIBPATH} -ldb"}
   if test -f "${BERKELEYDB_LIBPATH}/libdb-static.a"; then
      BERKELEYDB_STATIC_LIBS="-L${BERKELEYDB_LIBPATH} -ldb-static"
   else
      BERKELEYDB_STATIC_LIBS=${BERKELEYDB_LIBS}
   fi
   AC_CACHE_CHECK([for Berkeley DB libraries in $BERKELEYDB_PATH],
      ncbi_cv_lib_berkeley_db,
      [CPPFLAGS="$BERKELEYDB_INCLUDE $orig_CPPFLAGS"
       LIBS="$BERKELEYDB_LIBS $orig_LIBS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <db.h>]],
          [[ DB* dbp;  db_create(&dbp, NULL, 0);  dbp->app_private = 0; ]])],
          [ncbi_cv_lib_berkeley_db=yes], [ncbi_cv_lib_berkeley_db=no])])
   if test "$ncbi_cv_lib_berkeley_db" = "no"; then
      with_bdb="no"
   else
      AC_CACHE_CHECK([Berkeley DB version (4.1 or newer required)],
         ncbi_cv_lib_berkeley_db_version,
         [AC_LANG_CONFTEST([AC_LANG_SOURCE([[
#include <db.h>
ncbi_cv_lib_berkeley_db_version=DB_VERSION_MAJOR.DB_VERSION_MINOR.DB_VERSION_PATCH
          ]])])
          eval "$ac_cpp $BERKELEYDB_INCLUDE conftest.$ac_ext" \
             2>&AS_MESSAGE_LOG_FD | grep '^ncbi_cv_' | tr -d ' 	' > conftest.sh
          . ./conftest.sh
          rm -f contest*
         ])
      case "$ncbi_cv_lib_berkeley_db_version" in
         1.* | 2.* | 3.* | 4.0.* )
            AC_MSG_WARN(
               [Your Berkeley DB version is too old to use.  (Needed 4.1.)])
            with_bdb=no
            ;;
         4.1.* | 4.2.* | 4.3.* ) ;;
         * )
            AC_MSG_WARN(
               [Untested Berkeley DB version; may prove incompatible.])
            AC_MSG_WARN(
               [If so, please re-run this script with the flag --without-bdb.])
            ;;
      esac
   fi
fi
if test "$with_bdb" = "no" ; then
   BERKELEYDB_PATH="No_BerkeleyDB"
   BERKELEYDB_INCLUDE=
   BERKELEYDB_LIBS=
   WithoutPackages="$WithoutPackages BerkeleyDB"
else
   WithPackages="$WithPackages BerkeleyDB"
   AC_DEFINE(HAVE_BERKELEY_DB, 1,
             [Define to 1 if Berkeley DB libraries are available.])
fi


## ODBC
if test "$with_odbc" != "no" ; then
   case "$with_odbc" in
      yes | "" ) : ${ODBC_PATH:="/opt/machine/merant"} ;;
      */*      ) ODBC_PATH=$with_odbc                  ;;
   esac
   ODBC_INCLUDE="-I$ODBC_PATH/include"
   ODBC_LIBPATH="${ODBC_PATH}/lib"
   ODBC_LIBS="-L${ODBC_LIBPATH} ${CONF_f_runpath}${ODBC_LIBPATH} -lodbc -lodbcinst"
   CPPFLAGS="$ODBC_INCLUDE $orig_CPPFLAGS"
   LIBS="$ODBC_LIBS $orig_LIBS"
   AC_CACHE_CHECK([for ODBC libraries in $ODBC_PATH], ncbi_cv_lib_odbc,
      [AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <sql.h>]],
          [[SQLHDBC hdbc;  SQLCHAR* cp = (SQLCHAR*) "x";
            SQLRETURN x = SQLConnect(hdbc, cp, 1, cp, 2, cp, 3);]])],
          [ncbi_cv_lib_odbc=yes], [ncbi_cv_lib_odbc=no])])
   if test "$ncbi_cv_lib_odbc" = "no"; then
      with_odbc="no"
   else
      # odbcss.h isn't self-contained :-/
      AC_CHECK_HEADERS(odbcss.h, [], [], [#include <sql.h>])
   fi
fi
if test "$with_odbc" = "no" ; then
   ODBC_PATH="No_ODBC"
   ODBC_INCLUDE=
   ODBC_LIBS=
   WithoutPackages="$WithoutPackages ODBC"
else
   WithPackages="$WithPackages ODBC"
   AC_DEFINE(HAVE_ODBC, 1, [Define to 1 if ODBC libraries are available.])
fi

## Python
if test "$with_python" != "no" ; then
   case "$with_python" in
      yes | "" ) : ${PYTHON_PATH:="/usr/local/python-2.4"} ;;
      */*      ) PYTHON_PATH=$with_python                  ;;
   esac
   PYTHON_INCLUDE="-I$PYTHON_PATH/include/python2.4"
   PYTHON_LIBPATH="$PYTHON_PATH/lib/python2.4/config"
   PYTHON_LIBS="-L$PYTHON_LIBPATH ${CONF_f_runpath}${PYTHON_LIBPATH} -lpython2.4 -lutil -lpthread -lm -ldl"
   CPPFLAGS="$PYTHON_INCLUDE $orig_CPPFLAGS"
   LIBS="$PYTHON_LIBS $orig_LIBS"
   AC_CACHE_CHECK([for PYTHON libraries in $PYTHON_PATH], ncbi_cv_lib_python,
      [AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <Python.h>]],
          [[Py_Initialize(); Py_Finalize();]])],
          [ncbi_cv_lib_python=yes], [ncbi_cv_lib_python=no])])
   if test "$ncbi_cv_lib_python" = "no"; then
      with_python="no"
   fi
fi
if test "$with_python" = "no" ; then
   PYTHON_PATH="No_Python"
   PYTHON_INCLUDE=
   PYTHON_LIBS=
   WithoutPackages="$WithoutPackages PYTHON"
else
   WithPackages="$WithPackages PYTHON"
   AC_DEFINE(HAVE_PYTHON, 1, [Define to 1 if Python libraries are available.])
fi

## CPPUNIT
if test "$with_cppunit" != "no" ; then
   if test -z "$CPPUNIT_PATH"; then
      if test -d "$with_cppunit"; then
         CPPUNIT_PATH=$with_cppunit
      else
         CPPUNIT_PATH="$NCBI/cppunit"
      fi
   fi
   if test -d "$CPPUNIT_PATH/${compiler_vpfx}Debug${bit64_sfx}/lib" \
        -a -z "$CPPUNIT_ARCH_PATH"; then
      CPPUNIT_ARCH_PATH="$CPPUNIT_PATH/${compiler_vpfx}Debug${bit64_sfx}"
   elif test -d "$CPPUNIT_PATH/${compiler_pfx}Debug${bit64_sfx}/lib" \
        -a -z "$CPPUNIT_ARCH_PATH"; then
      CPPUNIT_ARCH_PATH="$CPPUNIT_PATH/${compiler_pfx}Debug${bit64_sfx}"
   else
      CPPUNIT_ARCH_PATH="$CPPUNIT_PATH"
   fi
   CPPUNIT_BINPATH=${CPPUNIT_BINPATH:="$CPPUNIT_ARCH_PATH/bin"}
   CPPUNIT_LIBPATH=${CPPUNIT_LIBPATH:="$CPPUNIT_ARCH_PATH/lib"}
   CPPUNIT_INCLUDE=
   AC_CACHE_CHECK([for CPPUNIT in $CPPUNIT_ARCH_PATH], ncbi_cv_lib_cppunit,
      [if test -x "$CPPUNIT_BINPATH/cppunit-config" ; then
          CPPUNIT_INCLUDE=`$CPPUNIT_BINPATH/cppunit-config --cflags`
          CPPUNIT_LIBS=`$CPPUNIT_BINPATH/cppunit-config --libs |\
             sed -e "s/-L\\([[^ ]]*\\)/-L\\1 ${CONF_f_runpath}\\1/"`

          CPPFLAGS="$CPPUNIT_INCLUDE $orig_CPPFLAGS"
          LIBS="$CPPUNIT_LIBS $orig_LIBS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM(
            [[
               #include <cppunit/TestResult.h>
               #include <cppunit/TestResultCollector.h>
            ]],
            [[
               CPPUNIT_NS::TestResult controller;
               CPPUNIT_NS::TestResultCollector result;
               controller.addListener( &result );        
            ]])],
            [ncbi_cv_lib_cppunit=yes], [ncbi_cv_lib_cppunit=no])
       else
          ncbi_cv_lib_cppunit=no
       fi])
   if test "$ncbi_cv_lib_cppunit" = "no"; then
      with_cppunit="no"
   fi
fi

if test "$with_cppunit" = "no" ; then
   CPPUNIT_PATH="No_CPPUNIT"
   CPPUNIT_INCLUDE=
   CPPUNIT_LIBS=
   WithoutPackages="$WithoutPackages CPPUNIT"
else
   WithPackages="$WithPackages CPPUNIT"
   CPPUNIT_CONFIG="$CPPUNIT_BINPATH/cppunit-config"
   # The "yes" may have been cached; get the actual settings again if needed
   if test -z "$CPPUNIT_INCLUDE"; then
      CPPUNIT_INCLUDE=`$CPPUNIT_CONFIG --cflags`
      CPPUNIT_LIBS=`$CPPUNIT_CONFIG --libs |\
         sed -e "s/-L\\([[^ ]]*\\)/-L\\1 ${CONF_f_runpath}\\1/"`
   fi
   AC_DEFINE(HAVE_CPPUNIT, 1, [Define to 1 if CPPUNIT libraries are available.])
fi


## NCBI C Toolkit
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
   if test -n "$NCBI_C_ncbi" ; then
     :
   elif test "$OSTYPE" = "darwin" ; then
     NCBI_C_ncbi="-lncbi -Wl,-framework,AppKit"
   else
     NCBI_C_ncbi="-lncbi"
   fi
   NCBI_C_LIBPATH="-L$NCBI_C_LIBPATH"
   AC_CACHE_CHECK([for the NCBI C toolkit in $NCBI_C_PATH], ncbi_cv_lib_ctools,
      [CPPFLAGS="$NCBI_C_INCLUDE $orig_CPPFLAGS"
       LIBS="$NCBI_C_LIBPATH $NCBI_C_ncbi $NETWORK_LIBS $orig_LIBS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <ncbi.h>]],
          [[ Nlm_Boolean b = Nlm_GetArgs("fake", -1, 0); ]])],
          [ncbi_cv_lib_ctools=yes], [ncbi_cv_lib_ctools=no])])
   if test "$ncbi_cv_lib_ctools" = no; then
      with_ncbi_c=no
   fi
fi
if test "$with_ncbi_c" = "no" ; then
   NCBI_C_PATH="No_NCBI_CToolkit"
   NCBI_C_INCLUDE=
   NCBI_C_LIBPATH=
   NCBI_C_ncbi=
   WithoutPackages="$WithoutPackages C-Toolkit"
else
   WithPackages="$WithPackages C-Toolkit"
   AC_DEFINE(HAVE_NCBI_C, 1, [Define to 1 if the NCBI C toolkit is available.])
fi


## OpenGL
if test "$with_opengl" != "no"; then
   case "$OSTYPE" in
      darwin) # Use native interface
         OPENGL_LIBS="-framework Carbon -framework AGL -framework OpenGL"
         ;;
      # cygwin) ... ;;
      *) # Default -- assume X-based
         LIBS="$orig_LIBS"
         CPPFLAGS="$orig_CPPFLAGS"
         AC_PATH_XTRA
         LDFLAGS="$orig_LDFLAGS $X_LIBS"
         AC_CHECK_LIB(Xext, XextCreateExtension, Xext="-lXext", Xext=,
            [$X_PRE_LIBS -lX11 $X_EXTRA_LIBS])
         AC_CHECK_LIB(Xt, XtMainLoop, Xt="-lXt", Xt=,
            [$Xext $X_PRE_LIBS -lX11 $X_EXTRA_LIBS])
         AC_CHECK_LIB(Xmu, XmuMakeAtom, Xmu="-lXmu", Xmu=,
            [$Xt $Xext $X_PRE_LIBS -lX11 $X_EXTRA_LIBS])
         X_ALL_LIBS="$Xmu $Xt $Xext $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"
         ogl_lib_sfx=
         case "${with_opengl:=yes}" in
            yes)
               # Check for NCBI's oddly located in-house installations.
               case "$OSTYPE:$host_cpu:$bit64_sfx" in
                  solaris:sparc:64)
                     with_opengl=/usr/ncbi/lib/OpenGL64/openwin
                     ogl_lib_sfx=/sparcv9
                     ;;
                  solaris:i386:) with_opengl=/usr/ncbi ;;
               esac
               ;;
         esac
         if test -d "$with_opengl/include/GL"; then
            X_CFLAGS="-I$with_opengl/include $X_CFLAGS"
         fi
         if test -d "$with_opengl/lib$ogl_lib_sfx"; then
            l=$with_opengl/lib$ogl_lib_sfx
            X_LIBS="-L$l ${CONF_f_runpath}$l $X_LIBS"
         fi
         LDFLAGS="$orig_LDFLAGS $X_LIBS"
         OPENGL_INCLUDE="$X_CFLAGS"
         OPENGL_LIBS="$X_LIBS -lGLU -lGL $X_ALL_LIBS"
         ;;
   esac
   case "${with_opengl}" in
      */*) where=" in $with_opengl" ;;
      *  ) where= ;;
   esac
   AC_CACHE_CHECK([for OpenGL$where], ncbi_cv_lib_opengl,
      [CPPFLAGS="$orig_CPPFLAGS $OPENGL_INCLUDE"
       LIBS="$OPENGL_LIBS $orig_LIBS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[
           #ifdef NCBI_OS_DARWIN
           #  include <OpenGL/gl.h>
           #else
           #  include <GL/gl.h>
           #endif
          ]],
          [[glBegin(0);]])],
          [ncbi_cv_lib_opengl=yes], [ncbi_cv_lib_opengl=no])])
fi

if test "$with_opengl" = "no" -o "$ncbi_cv_lib_opengl" = "no"; then
   OPENGL_INCLUDE=
   OPENGL_LIBS=
   OSMESA_INCLUDE=
   OSMESA_LIBS=
   GLUT_INCLUDE=
   GLUT_LIBS=
   with_opengl="no"
   WithoutPackages="$WithoutPackages OpenGL MESA"
else
   WithPackages="$WithPackages OpenGL"
   AC_DEFINE(HAVE_OPENGL, 1, [Define to 1 if you have OpenGL (-lGL).])
   : ${OSMESA_PATH=$NCBI/MesaGL}
   orig_LDFLAGS_X=$LDFLAGS
   if test "$with_mesa" != "no"; then
      if test "${with_mesa-yes}" = "yes" -a -d "$OSMESA_PATH"; then
         with_mesa=$OSMESA_PATH
      fi
      if test -n "$with_mesa" -a "$with_mesa" != "yes"; then
         OSMESA_INCLUDE=-I$with_mesa/include
         OSMESA_LDFLAGS="-L$with_mesa/lib ${CONF_f_runpath}$with_mesa/lib"
      fi
      LDFLAGS="$orig_LDFLAGS_X $OSMESA_LDFLAGS"
      AC_CHECK_LIB(OSMesa, OSMesaCreateContext,
         [WithPackages="$WithPackages MESA";
          OSMESA_LIBS="$OSMESA_LDFLAGS $X_LIBS -lOSMesa -lGLU -lGL $X_ALL_LIBS"
          AC_DEFINE(HAVE_LIBOSMESA, 1, [Define to 1 if you have libOSMesa.])],
         [WithoutPackages="$WithoutPackages MESA"; OSMESA_LIBS=],
         $OPENGL_LIBS)
   fi
   : ${GLUT_PATH=$NCBI/glut}
   if test "$with_glut" != "no"; then
      if test "${with_glut-yes}" = "yes" -a -d "$GLUT_PATH"; then
         with_glut=$GLUT_PATH
      fi
      if test -n "$with_glut" -a "$with_glut" != "yes"; then
         GLUT_INCLUDE=-I$with_glut/include
         GLUT_LDFLAGS="-L$with_glut/lib ${CONF_f_runpath}$with_glut/lib"
      fi
      LDFLAGS="$orig_LDFLAGS_X $GLUT_LDFLAGS"
      AC_CHECK_LIB(glut, glutInit,
         [WithPackages="$WithPackages GLUT";
          GLUT_LIBS="$GLUT_LDFLAGS $X_LIBS -lglut -lGLU -lGL $X_ALL_LIBS"
          AC_DEFINE(HAVE_LIBGLUT, 1, [Define to 1 if you have libglut.])],
         [WithoutPackages="$WithoutPackages GLUT"; GLUT_LIBS=],
         $OPENGL_LIBS)
   fi
fi

LDFLAGS=$orig_LDFLAGS


## FLTK
if test "$with_fltk" != "no"; then
   if test -z "$FLTK_PATH"; then
      if test -d "$with_fltk"; then
         FLTK_PATH=$with_fltk
      else
         FLTK_PATH="$NCBI/fltk"
      fi
   fi
   if test -d "$FLTK_PATH/${compiler_vpfx}${release_debug}${mt_sfx}${bit64_sfx}/lib" \
        -a -z "$FLTK_ARCH_PATH"; then
      FLTK_ARCH_PATH="$FLTK_PATH/${compiler_vpfx}${release_debug}${mt_sfx}${bit64_sfx}"
   elif test -d "$FLTK_PATH/${compiler_pfx}${release_debug}${mt_sfx}${bit64_sfx}/lib" \
        -a -z "$FLTK_ARCH_PATH"; then
      FLTK_ARCH_PATH="$FLTK_PATH/${compiler_pfx}${release_debug}${mt_sfx}${bit64_sfx}"
   else
      FLTK_ARCH_PATH="$FLTK_PATH"
   fi
   FLTK_BINPATH=${FLTK_BINPATH:="$FLTK_ARCH_PATH/bin"}
   FLTK_LIBPATH=${FLTK_LIBPATH:="$FLTK_ARCH_PATH/lib"}
   FLTK_INCLUDE=
   AC_CACHE_CHECK([for FLTK in $FLTK_ARCH_PATH], ncbi_cv_lib_fltk,
      [if test -x "$FLTK_BINPATH/fltk-config" ; then
          FLTK_INCLUDE=`$FLTK_BINPATH/fltk-config --cxxflags --use-gl`
          FLTK_LIBS=`$FLTK_BINPATH/fltk-config --ldflags`

          CPPFLAGS="$FLTK_INCLUDE $orig_CPPFLAGS"
          LIBS="$FLTK_LIBS $orig_LIBS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM(
             [[#include <FL/Fl.H>]],
             [[ Fl::run(); ]])],
             [ncbi_cv_lib_fltk=yes], [ncbi_cv_lib_fltk=no])
       else
          ncbi_cv_lib_fltk=no
       fi])
   if test "$ncbi_cv_lib_fltk" = "no"; then
      with_fltk="no"
   fi
fi

if test "$with_fltk" = "no" ; then
   FLTK_LIBPATH="No_FLTK"
   FLTK_INCLUDE=
   FLTK_LIBS=
   FLTK_LIBS_GL=
   WithoutPackages="$WithoutPackages FLTK"
else
   WithPackages="$WithPackages FLTK"
   FLTK_CONFIG="$FLTK_BINPATH/fltk-config"
   # The "yes" may have been cached; get the actual settings again if needed
   if test -z "$FLTK_INCLUDE"; then
      FLTK_INCLUDE=`$FLTK_CONFIG --cxxflags --use-gl`
      FLTK_LIBS=`$FLTK_CONFIG --ldflags`
   fi
   FLTK_LIBS_GL=`$FLTK_CONFIG --ldflags --use-gl`
   FLTK_LIBS_IMAGES=`$FLTK_CONFIG --ldflags --use-images`
   FLTK_LIBS_ALL=`$FLTK_CONFIG --ldflags --use-gl --use-images`
   # Also supply static versions for use in CGIs to be deployed to servers
   # without access to the shared libraries.
   FLTK_STATIC_LIBS=`$FLTK_CONFIG --ldstaticflags`
   FLTK_STATIC_LIBS_GL=`$FLTK_CONFIG --ldstaticflags --use-gl`
   FLTK_STATIC_LIBS_IMAGES=`$FLTK_CONFIG --ldstaticflags --use-images`
   FLTK_STATIC_LIBS_ALL=`$FLTK_CONFIG --ldstaticflags --use-gl --use-images`
   AC_DEFINE(HAVE_FLTK, 1, [Define to 1 if FLTK is available.])
fi


## wxWindows
if test "$with_wxwin" != "no" ; then
   if test -z "$WXWIN_PATH"; then
      if test -d "$with_wxwin"; then
         WXWIN_PATH=$with_wxwin
      else
         WXWIN_PATH="$NCBI/wxwin"
      fi
   fi
   if test -d "$WXWIN_PATH/${compiler_vpfx}${release_debug}${mt_sfx}${bit64_sfx}/lib" \
        -a -z "$WXWIN_ARCH_PATH"; then
      WXWIN_ARCH_PATH="$WXWIN_PATH/${compiler_vpfx}${release_debug}${mt_sfx}${bit64_sfx}"
   elif test -d "$WXWIN_PATH/${compiler_pfx}${release_debug}${mt_sfx}${bit64_sfx}/lib" \
        -a -z "$WXWIN_ARCH_PATH"; then
      WXWIN_ARCH_PATH="$WXWIN_PATH/${compiler_pfx}${release_debug}${mt_sfx}${bit64_sfx}"
   else
      WXWIN_ARCH_PATH="$WXWIN_PATH"
   fi
   WXWIN_BINPATH=${WXWIN_BINPATH:="$WXWIN_ARCH_PATH/bin"}
   WXWIN_LIBPATH=${WXWIN_LIBPATH:="$WXWIN_ARCH_PATH/lib"}
   WXWIN_INCLUDE=
   wxconf=$WXWIN_BINPATH/wx-config
   if test -x "$WXWIN_BINPATH/gtk-config"; then
      gtkconf=$WXWIN_BINPATH/gtk-config
   elif gtk-config --version >/dev/null 2>&1; then
      gtkconf=gtk-config
   else
      # May work without gtk-config (e.g., on Mac OS X)
      gtkconf=:
   fi
   AC_CACHE_CHECK([for wxWindows in $WXWIN_ARCH_PATH], ncbi_cv_lib_wxwindows,
      [if test -x "$wxconf" ; then
          WXWIN_INCLUDE="`$gtkconf --cflags` `$wxconf --cflags`"
          WXWIN_LIBS=`$wxconf --libs`

          CPPFLAGS="$WXWIN_INCLUDE $orig_CPPFLAGS"
          LIBS="$WXWIN_LIBS $orig_LIBS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM(
             [[#include <wx/wx.h>]],
             [[ wxExit(); ]])],
             [ncbi_cv_lib_wxwindows=yes], [ncbi_cv_lib_wxwindows=no])
       else
          ncbi_cv_lib_wxwindows=no
       fi])
   if test "$ncbi_cv_lib_wxwindows" = "no"; then
      with_wxwin="no"
   fi
fi

if test "$with_wxwin" = "no" ; then
   WXWIN_LIBPATH="No_wxWindows"
   WXWIN_INCLUDE=
   WXWIN_LIBS=
   WXWIN_GL_LIBS=
   WithoutPackages="$WithoutPackages wxWindows"
else
   WithPackages="$WithPackages wxWindows"
   # The "yes" may have been cached; get the actual settings again if needed
   if test -z "$WXWIN_INCLUDE"; then
      WXWIN_INCLUDE="`$gtkconf --cflags` `$wxconf --cflags`"
      WXWIN_LIBS=`$wxconf --libs`
   fi
   WXWIN_GL_LIBS=`$wxconf --gl-libs`
   AC_DEFINE(HAVE_WXWINDOWS, 1, [Define to 1 if wxWindows is available.])
fi


## In-house Fast-CGI library
if test "$with_fastcgi" != "no" ; then
   case "$with_fastcgi" in
      yes | "" ) : ${FASTCGI_PATH:="$NCBI/fcgi-current"} ;;
      */*      ) FASTCGI_PATH=$with_fastcgi              ;;
      *        ) FASTCGI_PATH=$NCBI/fcgi-${with_fastcgi} ;;
   esac
   FASTCGI_INCLUDE="-I$FASTCGI_PATH/include${bit64_sfx}"
   if test "$with_debug" = "no" ; then
      FASTCGI_LIBS="-L$FASTCGI_PATH/lib${bit64_sfx} -lfcgi $NETWORK_LIBS"
   else
      FASTCGI_LIBS="-L$FASTCGI_PATH/altlib${bit64_sfx} -lfcgi $NETWORK_LIBS"
   fi
   FASTCGI_OBJS="fcgibuf"
   LIBS="$FASTCGI_LIBS $orig_LIBS"
   AC_CACHE_CHECK([for FastCGI libraries in $FASTCGI_PATH], ncbi_cv_lib_fcgi,
      [CPPFLAGS="$FASTCGI_INCLUDE $orig_CPPFLAGS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <fcgiapp.h>]],
          [[ (void) FCGX_IsCGI(); ]])],
          [ncbi_cv_lib_fcgi=yes], [ncbi_cv_lib_fcgi=no])])
   if test "$ncbi_cv_lib_fcgi" = "no"; then
      with_fastcgi="no"
   fi
   AC_CHECK_FUNCS(FCGX_Accept_r)
fi
if test "$with_fastcgi" = "no" ; then
   FASTCGI_PATH="No_FastCgi"
   FASTCGI_INCLUDE=
   FASTCGI_LIBS=
   FASTCGI_OBJS=
   WithoutPackages="$WithoutPackages Fast-CGI"
else
   WithPackages="$WithPackages Fast-CGI"
   AC_DEFINE(HAVE_LIBFASTCGI, 1,
             [Define to 1 if FastCGI libraries are available.])
fi


## NCBI SSS libraries
with_sss=${with_sss:="yes"}
with_sssutils=${with_sssutils:="yes"}
with_sssdb=${with_sssdb:="yes"}

if test "$with_sybase" = "no" ; then
   with_sssdb="no"
fi

case "$host_os:$compiler:$compiler_version" in
   linux*:GCC:34*)
      : ${with_included_sss=yes}
      : ${with_included_geo=yes}
   ;;
esac

if test "$with_sss" != "no" ; then
   local_sss=${real_srcdir}/src/internal/sss
   if test "${with_included_sss-no}" = "yes" \
       -a -f "$local_sss/utils/sssdebug.hpp" \
       -a -f "${local_sss}srv/cli/sssconnection.cpp"; then
      with_included_sss=yes
      NCBI_SSS_PATH=$local_sss
      NCBI_SSS_INCLUDE="\$(incdir)/sss"
      NCBI_SSS_LIBPATH="\$(libdir)"
      sss=sss
      ssssrv=ssssrv
      stax=stax
   else
      with_included_sss=no
      sss=
      ssssrv=
      stax=
   fi
   NCBI_SSS_PATH=${NCBI_SSS_PATH:="$NCBI/sss/BUILD"}
   NCBI_SSS_INCLUDE=${NCBI_SSS_INCLUDE:="$NCBI_SSS_PATH/include"}
   if test -z "$NCBI_SSS_LIBPATH" ; then
      NCBI_SSS_LIBPATH="${NCBI_SSS_PATH}/lib/${compiler_vpfx}${release_debug}${bit64_sfx}"
      if test "$with_mt" = "yes" ; then
         NCBI_SSS_LIBPATH="${NCBI_SSS_LIBPATH}mt"
      fi
   fi
   AC_MSG_CHECKING([for NCBI SSS directories in $NCBI_SSS_PATH])
   if test "$with_included_sss" = "yes"; then
      AC_MSG_RESULT([yes]) # duh
   elif test ! -d "${NCBI_SSS_LIBPATH}"  -o  ! -d "${NCBI_SSS_INCLUDE}" ; then
      AC_MSG_RESULT([no])
      with_sss="no"
   else
      AC_MSG_RESULT([yes])
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
   if test "$with_included_sss" = "yes"; then
      NCBI_SSS_LIBPATH= # Not needed
      ncbi_cv_lib_sssutils=yes
      ncbi_cv_lib_sssdb=yes
   else
      NCBI_SSS_LIBPATH="-L${NCBI_SSS_LIBPATH}"
   fi
   NCBI_SSS_INCLUDE="-I${NCBI_SSS_INCLUDE}"

   # SSS UTILS
   if test "$with_sssutils" != "no" ; then
      LIBSSSUTILS="-lsssutils"
      AC_CACHE_CHECK([for the NCBI SSS UTILS library], ncbi_cv_lib_sssutils,
         [CPPFLAGS="$NCBI_SSS_INCLUDE $SYBASE_INCLUDE $orig_CPPFLAGS"
          LIBS="$NCBI_SSS_LIBPATH $LIBSSSUTILS $SYBASE_LIBS $NETWORK_LIBS $DL_LIBS $orig_LIBS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM(
             [[#include <hashtab.hpp>]],
             [[ (new C_HashTab(123))->Clear(); ]])],
             [ncbi_cv_lib_sssutils=yes], [ncbi_cv_lib_sssutils=no])])
      if test "$ncbi_cv_lib_sssutils" = yes; then
         AC_DEFINE(HAVE_LIBSSSUTILS, 1,
                   [Define to 1 if the NCBI SSS UTILS library is available.])
         WithPackages="$WithPackages SSSUTILS"
      else
         with_sssutils="no"
         with_sss="no"
         LIBSSSUTILS=""
         WithoutPackages="$WithoutPackages SSSUTILS"
      fi
   fi

   # SSS DB
   if test "$with_sssdb" != "no" ; then
      if test "$with_mt" = "yes" ; then
         LIBSSSDB="-lsssdb -lssssys"
      else
         LIBSSSDB="-lsssdb"
      fi
      AC_CACHE_CHECK([for the NCBI SSS DB library], ncbi_cv_lib_sssdb,
         [CPPFLAGS="$NCBI_SSS_INCLUDE $SYBASE_INCLUDE $orig_CPPFLAGS"
          LIBS="$NCBI_SSS_LIBPATH $LIBSSSDB $SYBASE_LIBS $NETWORK_LIBS $DL_LIBS $orig_LIBS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM(
             [[#include <cslink.hpp>]],
             [[ (new CSLink)->connect2server("srv", "user", "pwd", "db"); ]])],
             [ncbi_cv_lib_sssdb=yes], [ncbi_cv_lib_sssdb=no])])
      if test "$ncbi_cv_lib_sssdb" = "yes"; then
         AC_DEFINE(HAVE_LIBSSSDB, 1,
                   [Define to 1 if the NCBI SSS DB library is available.])
         WithPackages="$WithPackages SSSDB"
      else
         with_sssdb="no"
         with_sss="no"
         LIBSSSDB=""
         WithoutPackages="$WithoutPackages SSSDB"
      fi
   fi
fi


# GEO
with_geo=${with_geo:="yes"}

if test "$with_sssdb" = "no"  -o  "$with_sssutils" = "no" ; then
   with_geo="no"
fi

if test "$with_geo" != "no" ; then
   local_geo='<internal/geo>'
   if test "${with_included_geo-no}" = "yes" \
       -a -f "${real_srcdir}/include/internal/geo/geodb/geo_api.hpp" \
       -a -f "${real_srcdir}/src/internal/geo/geodb/utils.cpp"; then
      with_included_geo=yes
      NCBI_GEO_PATH=$local_geo
      NCBI_GEO_INCLUDE='$(includedir)/internal/geo/geodb -I$(includedir0)/internal/geo/geodb'
      NCBI_GEO_LIBPATH='$(libdir)'
      geo=geo
   else
      with_included_geo=no
      geo=
   fi
   NCBI_GEO_PATH=${NCBI_GEO_PATH:="$NCBI/geo/production"}
   NCBI_GEO_INCLUDE=${NCBI_GEO_INCLUDE:="$NCBI_GEO_PATH/include"}
   if test -z "$NCBI_GEO_LIBPATH" ; then
      NCBI_GEO_LIBPATH="${NCBI_GEO_PATH}/lib/${compiler_vpfx}${release_debug}${bit64_sfx}"
   fi
   AC_MSG_CHECKING([for NCBI GEO directories in $NCBI_GEO_PATH])
   if test "$with_included_geo" = "yes"; then
      AC_MSG_RESULT([yes])
   elif test ! -d "${NCBI_GEO_LIBPATH}"  -o  ! -d "${NCBI_GEO_INCLUDE}" ; then
      AC_MSG_RESULT([no])
      with_geo="no"
   else
      AC_MSG_RESULT([yes])
   fi
fi

if test "$with_geo" = "no" ; then
   NCBI_GEO_INCLUDE=
   NCBI_GEO_LIBPATH=
   LIBGEODB=
else
   if test "$with_included_geo" = "yes"; then
      NCBI_GEO_LIBPATH= # Not needed
      ncbi_cv_lib_geodb=yes
   else
      NCBI_GEO_LIBPATH="-L${NCBI_GEO_LIBPATH}"
   fi
   # GEO_API defaults to GeoTest, leading to link errors.
   NCBI_GEO_INCLUDE="-I${NCBI_GEO_INCLUDE} -DGEO_API=Geo"
   LIBGEODB="-lgeodb"

   # We have to be careful here, since GEO uses our libxncbi and SSS; as such,
   # we must simply assume success in some cases.
   case "$compiler" in
      WorkShop*) ncbi_cv_lib_geodb=yes ;;
      *)         test "$with_included_sss" = yes  &&  ncbi_cv_lib_geodb=yes ;;
   esac
   AC_CACHE_CHECK([for the NCBI GEO DB library], ncbi_cv_lib_geodb,
     [CPPFLAGS="$NCBI_GEO_INCLUDE $NCBI_SSS_INCLUDE $SYBASE_INCLUDE -I$real_srcdir/include $orig_CPPFLAGS"
      LIBS="$NCBI_GEO_LIBPATH $LIBGEODB $NCBI_SSS_LIBPATH $LIBSSSDB $LIBSSSUTILS $SYBASE_LIBS $NETWORK_LIBS $DL_LIBS $orig_LIBS"
      AC_LINK_IFELSE([AC_LANG_PROGRAM(
         [[#define FORWARDING_NCBICONF_H
           #include <geo_api.hpp>]],
         [[GEO_API::Pattern* p; if (p->isValid()) p->bind(1, "x", true, "y"); ]])],
         [ncbi_cv_lib_geodb=yes], [ncbi_cv_lib_geodb=no])])

   if test "$ncbi_cv_lib_geodb" = yes; then
      AC_DEFINE(HAVE_LIBGEODB, 1,
                [Define to 1 if NCBI GEO DB library is available.])
   else
      with_geo=no
   fi
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
      SP_LIBPATH="${SP_PATH}/${compiler_vpfx}${release_debug}${mt_sfx}${bit64_sfx}"
   fi
   AC_MSG_CHECKING([for SP directories in $SP_PATH])
   if test ! -d "${SP_LIBPATH}"  -o  ! -d "${SP_INCLUDE}" ; then
      AC_MSG_RESULT([no])
      with_sp="no"
   else
      AC_MSG_RESULT([yes])
   fi
fi

if test "$with_sp" != "no" ; then
   if test -d "$SP_GENERIC"; then
      SP_INCLUDE="-I${SP_INCLUDE} -I${SP_GENERIC}"
   else
      SP_INCLUDE="-I${SP_INCLUDE}"
   fi
   SP_LIBS="-L$SP_LIBPATH -lsp"
   AC_CACHE_CHECK([for the SP SGML library], ncbi_cv_lib_sp,
      [CPPFLAGS="$SP_INCLUDE $orig_CPPFLAGS"
       LIBS="$SP_LIBS $orig_LIBS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <SGMLApplication.h>]],
          [[ SP_API::SGMLApplication app; ]])],
          [ncbi_cv_lib_sp=yes], [ncbi_cv_lib_sp=no])])
   if test "$ncbi_cv_lib_sp" = "no"; then
      with_sp="no"
   fi
fi

if test "$with_sp" = "no" ; then
   SP_INCLUDE=
   SP_LIBS=
   WithoutPackages="$WithoutPackages SP"
else
   WithPackages="$WithPackages SP"
   AC_DEFINE(HAVE_LIBSP, 1, [Define to 1 if the SP SGML library is available.])
fi


## NCBI PubMed
if test "$with_ncbi_c" = "no" ; then
   with_pubmed="no"
fi

if test "$with_pubmed" != "no" ; then
   case "$NCBI_PM_PATH" in
      "" )          NCBI_PM_PATH=$NCBI/pubmed${bit64_sfx} ;;
      *$bit64_sfx ) ;;
      * )           NCBI_PM_PATH=${NCBI_PM_PATH}${bit64_sfx} ;;
   esac
   pmtag=${release_debug}${bit64_sfx:-32}
   vpath="$NCBI_PM_PATH/${compiler_vpfx}build"
   test -d "$vpath"  &&  NCBI_PM_PATH=$vpath

   AC_CACHE_CHECK([for NCBI PubMed libraries in $NCBI_PM_PATH], ncbi_cv_lib_pm,
      [if test ! -d "${NCBI_PM_PATH}" ; then
          ncbi_cv_lib_pm="no"
       else
          CPPFLAGS="-I$NCBI_PM_PATH/pmentrez/include/Ncbi -I$NCBI_PM_PATH/xmldb/ur/include $NCBI_C_INCLUDE $orig_CPPFLAGS"
          LIBS="-L$NCBI_PM_PATH/xmldb$mt_sfx/out.$pmtag/lib -lpmurapi-static -L$NCBI_PM_PATH/pmentrez.$pmtag$mt_sfx/pmentrez/libs -lpmncbi $NCBI_C_LIBPATH -lncbi $orig_LIBS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM(
             [[#include <ncbistr.hpp>
               #include <hastring.h>]],
             [[ CNcbiString("abc"); CAString("def"); ]])],
             [ncbi_cv_lib_pm=yes], [ncbi_cv_lib_pm=no])
       fi])
   if test "$ncbi_cv_lib_pm" = "no"; then
      with_pubmed="no"
   fi
fi

if test "$with_pubmed" = "no" ; then
   NCBI_PM_PATH="No_NCBI_PubMed"
   WithoutPackages="$WithoutPackages PubMed"
else
   WithPackages="$WithPackages PubMed"
   AC_DEFINE(HAVE_LIBPUBMED, 1,
             [Define to 1 if NCBI PubMed libraries are available.])
fi


## ORBacus CORBA
if test "${with_orbacus:-yes}" = yes; then
    : ${ORBACUS_PATH=$NCBI/corba/OB-4.0.1}
elif test "$with_orbacus" != no; then
    ORBACUS_PATH=$with_orbacus
fi
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
   AC_CACHE_CHECK([for ORBacus in $ORBACUS_PATH], ncbi_cv_lib_orbacus,
      [CPPFLAGS="$ORBACUS_INCLUDE $orig_CPPFLAGS"
       LIBS="$ORBACUS_LIBPATH $LIBOB $NETWORK_LIBS $orig_LIBS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <OB/CORBA.h>]],
          [[
            int c = 0;
            char* v[] = { NULL };
            CORBA::ORB_var orb = CORBA::ORB_init(c, v);
            if (!CORBA::is_nil(orb)) orb->destroy();
          ]])],
          [ncbi_cv_lib_orbacus=yes], [ncbi_cv_lib_orbacus=no])])
   if test "$ncbi_cv_lib_orbacus" = "no"; then
      with_orbacus="no"
   fi
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
   AC_DEFINE(HAVE_ORBACUS, 1,
             [Define to 1 if the ORBacus CORBA package is available.])
fi
# Sigh... without this, the linker pulls in the contents of the cache
# and then gripes about missing JTC stuff.
test -d SunWS_cache  &&  rm -r SunWS_cache


### XML/XSL libraries
: ${EXPAT_PATH=$NCBI/expat}
NCBI_CHECK_THIRD_PARTY_LIB(expat,
 AC_LANG_PROGRAM([#include <expat.h>],
    [XML_Parser parser = XML_ParserCreate("utf-8");]))
if test -f "${EXPAT_PATH}/lib${bit64_sfx}/libexpat-static.a"; then
   EXPAT_STATIC_LIBS="-L${EXPAT_PATH}/lib${bit64_sfx} -lexpat-static"
else
   EXPAT_STATIC_LIBS=${EXPAT_LIBS}
fi
: ${SABLOT_PATH=$NCBI/Sablot}
vpath="$SABLOT_PATH/${compiler_vpfx}build"
test -d "$vpath"  &&  SABLOT_PATH=$vpath
NCBI_CHECK_THIRD_PARTY_LIB(sablot,
 AC_LANG_PROGRAM([#include <sablot.h>],
    [SDOM_Document* D; int x = SablotParse(NULL, "file:/dev/null", D);]),
 $EXPAT_LIBS)
test -d SunWS_cache  &&  rm -r SunWS_cache
if test -f "${SABLOT_PATH}/lib${bit64_sfx}/libsablot-static.a"; then
   SABLOT_STATIC_LIBS="-L${SABLOT_PATH}/lib${bit64_sfx} -lsablot-static $EXPAT_STATIC_LIBS"
else
   SABLOT_STATIC_LIBS=${SABLOT_LIBS}
fi

: ${LIBXML_PATH=$NCBI/libxml}
vpath="$LIBXML_PATH/${compiler_vpfx}${release_debug}${mt_sfx}"
test -d "$vpath"  &&  LIBXML_PATH=$vpath
NCBI_CHECK_THIRD_PARTY_LIB_(libxml, LIBXML, xml2,
 AC_LANG_PROGRAM([#include <libxml/parser.h>],
    [xmlDocPtr doc = xmlParseFile("foo.xml");]))
if test -f "${LIBXML_PATH}/lib${bit64_sfx}/libxml2-static.a"; then
   LIBXML_STATIC_LIBS="-L${LIBXML_PATH}/lib -lxml2-static"
else
   LIBXML_STATIC_LIBS=${LIBXML_LIBS}
fi

### SQLite
: ${SQLITE_PATH=$NCBI/sqlite}
NCBI_CHECK_THIRD_PARTY_LIB(sqlite,
 AC_LANG_PROGRAM([#include <sqlite.h>],
    [char *errs; sqlite* db = sqlite_open("foo.db", 0, &errs);]))


### OEChem
# somewhat kludgish, as we now wanto to add in oeiupac and oedepict,
# which depend on oechem....
: ${OECHEM_PATH=$NCBI/oechem}
NCBI_CHECK_THIRD_PARTY_LIB(oechem,
 AC_LANG_PROGRAM([#include <oechem.h>],
    [OEChem::OEMol mol; OEChem::OEConfBase* c = mol.GetActive();]),
  -loeiupac -loedepict -loechem -loesystem -loeplatform -lz, $NETWORK_LIBS)
if test -n "$OECHEM_LIBS"; then
   OECHEM_LIBS=`echo $OECHEM_LIBS | sed -e 's/-loechem \(.*-loechem .*\)/\1/'`
fi
test -d SunWS_cache  &&  rm -r SunWS_cache


### Image libraries
: ${JPEG_PATH=$NCBI/gd}
# Grr... jpeglib.h isn't especially well-behaved.
NCBI_CHECK_THIRD_PARTY_LIB(jpeg,
 AC_LANG_PROGRAM([#include <sys/types.h>
                  #include <stdio.h>
                  extern "C" {
                  #include <jpeglib.h>
                  }],
    [struct jpeg_decompress_struct cinfo;
     struct jpeg_error_mgr         jerr;
     cinfo.err = jpeg_std_error(&jerr);
 ]))

: ${PNG_PATH=$NCBI/gd}
NCBI_CHECK_THIRD_PARTY_LIB(png,
 AC_LANG_PROGRAM([#include <png.h>],
    [png_structp png_ptr
      = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
 ]), $Z_LIBS, [], $Z_INCLUDE)

: ${TIFF_PATH=/usr/sfw}
NCBI_CHECK_THIRD_PARTY_LIB(tiff,
 AC_LANG_PROGRAM([#include <tiffio.h>],
    [TIFF* tiff = TIFFOpen("foo", "r");]), $Z_LIBS)
## etc.

# Paths?
with_ungif=$with_gif
NCBI_CHECK_THIRD_PARTY_LIB(ungif,
 AC_LANG_PROGRAM([extern "C" {
                  #include <gif_lib.h>
                  }],
    [GifFileType* fp = DGifOpenFileName("foo");]), $X_LIBS -lX11)
if test "$with_ungif" = "no"; then
   NCBI_CHECK_THIRD_PARTY_LIB(gif,
    AC_LANG_PROGRAM([extern "C" {
                     #include <gif_lib.h>
                     }],
       [GifFileType* fp = DGifOpenFileName("foo");]), $X_LIBS -lX11)
else
   AC_DEFINE(HAVE_LIBGIF, 1, [Define to 1 if libgif is available.])
   GIF_INCLUDE=$UNGIF_INCLUDE
   GIF_LIBS=$UNGIF_LIBS
fi

test -n "$x_libraries"  &&  : ${XPM_PATH=`dirname "$x_libraries"`}
NCBI_CHECK_THIRD_PARTY_LIB(Xpm,
 AC_LANG_PROGRAM([#include <X11/xpm.h>],
    [XpmImage image; XpmInfo info;
     XpmReadFileToXpmImage("foo", &image, &info);]), $X_LIBS -lX11, [],
    $X_CFLAGS)
# The use of X_CFLAGS is probably redundant, but shouldn't hurt.

### Restore original compiler/linker flags
LIBS="$orig_LIBS"
CPPFLAGS="$orig_CPPFLAGS"



### Optional projects
### --------------------------------------------


## `local_lbsm' support

local_lbsm="ncbi_lbsm ncbi_lbsm_ipc ncbi_lbsmd"

## requires semaphores (lacking on older versions of Cygwin and Darwin)
test $ncbi_cv_sys_semaphores = no  &&  with_local_lbsm=no

for fff in $local_lbsm ; do
   if test ! -f ${real_srcdir}/src/connect/${fff}.c ; then
      if test "$with_local_lbsm" = "yes" ; then
         AC_MSG_ERROR(["--with-local-lbsm:  src/connect/${fff}.c is  missing"])
      fi
      with_local_lbsm="no"
      break
   fi
done

if test "$with_local_lbsm" = "no" ; then
   WithoutProjects="$WithoutProjects local_lbsm"
   local_lbsm="ncbi_lbsmd_stub"
else
   WithProjects="$WithProjects local_lbsm"
   AC_DEFINE(HAVE_LOCAL_LBSM, 1,
             [Define to 1 if local LBSM support is available.])
fi


## `serial' projects

if test "$with_serial" = "no" ; then
   NoConfProjects="$NoConfProjects serial"
fi

if test ! -f ${real_srcdir}/src/serial/Makefile.in  -o  \
        ! -d ${real_srcdir}/include/serial ; then
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

if test "$with_objects" = "no" ; then
   NoConfProjects="$NoConfProjects objects"
fi

if test ! -f ${real_srcdir}/src/objects/Makefile.in  -o  \
        ! -d ${real_srcdir}/include/objects ; then
   if test "$with_objects" = "yes" ; then
      AC_MSG_ERROR(["--with-objects:  OBJECTS sources are missing"])
   fi
   with_objects="no"
fi

if test "$with_objects" = "no"  -o  -z "$serial" ; then
   WithoutProjects="$WithoutProjects objects"
   objects=
else
   WithProjects="$WithProjects objects"
   objects="objects"
fi


## `bdb' projects

if test "$with_bdb" = "no" ; then
   NoConfProjects="$NoConfProjects bdb"
fi

if test ! -f ${real_srcdir}/src/bdb/Makefile.in  -o  ! -d ${real_srcdir}/include/bdb
then
   if test "$with_bdb" = "yes" ; then
      AC_MSG_ERROR(["--with-bdb:  BDB sources are missing"])
   fi
   with_bdb="no"
fi

if test "$with_bdb" = "no" ; then
   WithoutProjects="$WithoutProjects bdb"
   bdb=
   BDB_LIB=
   BDB_CACHE_LIB=
else
   WithProjects="$WithProjects bdb"
   bdb="bdb"
   BDB_LIB="bdb"
   AC_DEFINE(HAVE_BDB, 1,
             [Define to 1 if NCBI C++ API for BerkeleyDB is available.])
   bdb_cache="ncbi_xcache_bdb"
   BDB_CACHE_LIB="ncbi_xcache_bdb"
   AC_DEFINE(HAVE_BDB_CACHE, 1,
             [Define to 1 if NCBI C++ API for BerkeleyDB based data cache is available.])
fi


## `dbapi' projects

if test "$with_dbapi" = "no" ; then
   NoConfProjects="$NoConfProjects dbapi"
fi

if test ! -f ${real_srcdir}/src/dbapi/Makefile.in  -o  \
        ! -d ${real_srcdir}/include/dbapi ; then
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

if test ! -f ${real_srcdir}/src/app/Makefile.in  -o  \
        ! -d ${real_srcdir}/include/app ; then
   if test "$with_app" = "yes" ; then
      AC_MSG_ERROR(["--with-app:  APP sources are missing"])
   fi
   with_app="no"
fi

if test "$with_app" = "no" ; then
   WithoutProjects="$WithoutProjects app"
   app=
else
   WithProjects="$WithProjects app"
   app="app"
fi


## `ctools' project cluster

if test "$with_ctools" = "no" ; then
   NoConfProjects="$NoConfProjects ctools"
fi

if test ! -f ${real_srcdir}/src/ctools/Makefile.in  -o  \
        ! -d ${real_srcdir}/include/ctools  -o  "$with_ncbi_c" = "no"
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
   WithProjects="$WithProjects ctools"
fi


## `gui' projects

if test "$with_gui" = "no" ; then
   NoConfProjects="$NoConfProjects gui"
fi

if test ! -f ${real_srcdir}/src/gui/Makefile.in  -o  ! -d ${real_srcdir}/include/gui \
    -o  "$with_fltk" = "no"
then
   if test "$with_gui" = "yes" ; then
      if test "$with_fltk" = "no" ; then
         AC_MSG_ERROR(["--with-gui:  FLTK package is missing"])
      else
         AC_MSG_ERROR(["--with-gui:  GUI sources are missing"])
      fi
   fi
   with_gui="no"
fi

if test "$with_gui" = "no" ; then
   WithoutProjects="$WithoutProjects gui"
   gui=
else
   WithProjects="$WithProjects gui"
   gui="gui"
fi


## `algo' projects

if test "$with_algo" = "no" ; then
   NoConfProjects="$NoConfProjects algo"
fi

if test ! -f ${real_srcdir}/src/algo/Makefile.in  -o  ! -d ${real_srcdir}/include/algo
then
   if test "$with_algo" = "yes" ; then
      AC_MSG_ERROR(["--with-algo:  ALGO sources are missing"])
   fi
   with_algo="no"
fi

if test "$with_algo" = "no" ; then
   WithoutProjects="$WithoutProjects algo"
   algo=
else
   WithProjects="$WithProjects algo"
   algo="algo"
fi


## `internal' project cluster

if test "$with_internal" = "no"  -o  ! -f ${real_srcdir}/src/internal/Makefile.in \
    -o  ! -d ${real_srcdir}/include/internal; then
   NoConfProjects="$NoConfProjects internal"
   internal=
else
   reason=""
   sep=
   if test "$with_sybase" = "no"; then
     reason="$reason${sep}Sybase"
     sep=", "
   fi
   if test "$with_ncbi_c" = "no"; then
     reason="$reason${sep}C-Toolkit"
     sep=", "
   fi
   if test "$with_sss" = "no"; then
     reason="$reason${sep}SSS"
     sep=", "
   fi
   if test "$with_pubmed" = "no"; then
     reason="$reason${sep}PubMed"
     sep=", "
   fi
   if test -n "$reason" -a "$with_internal" = "yes"; then
      if test -z "$with_projects"  -o  "$with_projects" = "no"; then
         AC_MSG_ERROR(["--with-internal:  Cannot build INTERNAL projects: missing $reason"])
      else
         AC_MSG_WARN(["--with-internal:  Cannot build all INTERNAL projects: missing $reason"])
         OPT_GROUPS="$OPT_GROUPS internal"
         internal="internal"
      fi
   elif test -n "$reason"; then
      NoConfProjects="$NoConfProjects internal"
      internal=
   else
      OPT_GROUPS="$OPT_GROUPS internal"
      internal="internal"
   fi
fi



### Miscellanea
### --------------------------------------------

if test "$with_execopy" = "no" ; then
   APP_NOCOPY="BINCOPY=@:"
else
   APP_NOCOPY=
fi

if test "$with_exe" = "no" ; then
   APP_OR_NULL=null
else
   APP_OR_NULL=app
fi

IF_REBUILDING_LIBS="#"
IF_REBUILDING_CONDITIONALLY="#"
case "$with_lib_rebuilds" in
  yes) IF_REBUILDING_LIBS="" ;;
  ask) IF_REBUILDING_CONDITIONALLY="" ;;
esac

case "$with_projects" in
 "") if test -r projects; then
        AC_MSG_NOTICE([using default projects file "projects".])
        with_projects=projects
        AC_SUBST(PROJECTS, "\$(top_srcdir)/projects")
     else
        AC_SUBST(PROJECTS, "")
        if test -f projects; then
           AC_MSG_WARN([default projects file "projects" is unreadable.])
        elif test -h projects; then
           AC_MSG_WARN([default projects file "projects" is a dangling link.])
        fi
     fi
     ;;
 no) with_projects="" ;  AC_SUBST(PROJECTS, "") ;;
 /*) AC_SUBST(PROJECTS, $with_projects) ;;
 * ) AC_SUBST(PROJECTS, "\$(top_srcdir)/$with_projects") ;;
esac


# Run the test suite after the build?

if test -n "$with_check"  -a  "$with_check" != "no" ; then
   check="check"
   if test "$with_check" != "yes" ; then
      CHECK_ARG=`echo "$with_check" | sed 's/%/ /g'`
   fi
   WithFeatures="$WithFeatures check"
fi


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

### Adjust paths to wrapper.*.sh
case "$CC"     in compilers/*)     CC="\$(top_srcdir)/$CC"     ;; esac
case "$CXX"    in compilers/*)    CXX="\$(top_srcdir)/$CXX"    ;; esac
case "$CXXCPP" in compilers/*) CXXCPP="\$(top_srcdir)/$CXXCPP" ;; esac
case "$AR"     in compilers/*)     AR="\$(top_srcdir)/$AR"     ;; esac

### Get Insure++ to honor the top-level .psrc file
case "$CC" in
   */insure*)
      CC="$CC -Zop \$(top_srcdir)/.psrc -D__INSURE__"
      CXX="$CXX -Zop \$(top_srcdir)/.psrc -D__INSURE__"
      CXXCPP="$CXXCPP -Zop \$(top_srcdir)/.psrc -D__INSURE__"
      LINK_DLL=`echo "$LINK_DLL" | sed -e 's:insure:& -Zop \$(top_srcdir)/.psrc:'`
   ;;
esac


#############################################################################
### Compose a list of configurables("*.in" -- makefiles, shell scripts, etc.)
### in subdirs to configure
### Compose their target counterparts
### Configure

AC_MSG_NOTICE([looking for configurables below $real_srcdir...])
excl_file="`pwd`/config.tmp.$$"
changequote(<<, >>)dnl
echo "NoConfProjects $NoConfProjects" | sed 's% \([^ ][^ ]*\)%\
^\\./\1/%g' > $excl_file
changequote([, ])dnl
configurables=`cd "${real_srcdir}/src"  &&  find . -name '*.in' -print | $EGREP -v -f $excl_file | sed "s%\.\(.*\)\.in%$builddir\1:\./src\1\.in%g"`
rm $excl_file

if test -z "$configurables" ; then
  AC_MSG_ERROR([cannot find any configurables below "${real_srcdir}"!])
else
  AC_MSG_NOTICE([...done])
fi

eval ncbi_runpath=`echo $pure_runpath`
dnl AC_DEFINE_UNQUOTED(NCBI_RUNPATH, "$ncbi_runpath", [NCBI runpath])

### Template substitutions
AC_SUBST(signature)

AC_SUBST(build_root)

AC_SUBST(top_srcdir)
AC_SUBST(srcdir)
#AC_SUBST(real_srcdir)
AC_SUBST(includedir)
AC_SUBST(status_dir)
AC_SUBST(builddir)
AC_SUBST(runpath)
AC_SUBST(ncbi_runpath)

AC_SUBST(CC)
AC_SUBST(CPP)
AC_SUBST(CXX)
AC_SUBST(AR)
AC_SUBST(RANLIB)
AC_SUBST(STRIP)
AC_SUBST(LINK)
AC_SUBST(C_LINK)
AC_SUBST(TAIL_N)
AC_SUBST(EGREP_Q)

AC_SUBST(CFLAGS)
AC_SUBST(FAST_CFLAGS)
AC_SUBST(CXXFLAGS)
AC_SUBST(FAST_CXXFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(DEPFLAGS)
AC_SUBST(DEPFLAGS_POST)
AC_SUBST(LDFLAGS)
AC_SUBST(FAST_LDFLAGS)
AC_SUBST(LIBS)
AC_SUBST(C_LIBS)
AC_SUBST(GCCPCH)

AC_SUBST(D_SFX)
AC_SUBST(DEBUG_SFX)

AC_SUBST(LIB_OR_DLL)
AC_SUBST(FORCE_STATIC_LIB)
AC_SUBST(LINK_DLL)
AC_SUBST(has_dll_loadable)
AC_SUBST(LINK_LOADABLE)
AC_SUBST(CFLAGS_DLL)
AC_SUBST(CXXFLAGS_DLL)

AC_SUBST(OPT_GROUPS)
AC_SUBST(local_lbsm)
AC_SUBST(serial)
AC_SUBST(bdb)
AC_SUBST(dbapi)
AC_SUBST(objects)
AC_SUBST(gui)
AC_SUBST(algo)
AC_SUBST(app)
AC_SUBST(internal)

AC_SUBST(check)
AC_SUBST(CHECK_ARG)

AC_SUBST(script_shell)
AC_SUBST(make_shell)
AC_SUBST(obj_ext)
AC_SUBST(lib_pre)
AC_SUBST(lib_l_pre)
AC_SUBST(lib_ext)
AC_SUBST(dll_ext)
AC_SUBST(loadable_ext)
AC_SUBST(lib_l_ext)
AC_SUBST(exe_ext)
AC_SUBST(f_compile)
AC_SUBST(f_outobj)
AC_SUBST(f_outlib)
AC_SUBST(f_libpath)
AC_SUBST(f_runpath)
AC_SUBST(f_outexe)

AC_SUBST(BDB_LIB)
AC_SUBST(BDB_CACHE_LIB)

AC_SUBST(THREAD_LIBS)
AC_SUBST(NETWORK_LIBS)
AC_SUBST(NETWORK_PURE_LIBS)
AC_SUBST(RESOLVER_LIBS)
AC_SUBST(MATH_LIBS)
AC_SUBST(KSTAT_LIBS)
AC_SUBST(RPCSVC_LIBS)
AC_SUBST(CRYPT_LIBS)
AC_SUBST(DL_LIBS)
AC_SUBST(RT_LIBS)
AC_SUBST(ICONV_LIBS)
AC_SUBST(Z_LIB)
AC_SUBST(BZ2_LIB)
AC_SUBST(PCREPOSIX_LIBS)
AC_SUBST(PCRE_LIB)
AC_SUBST(SYBASE_PATH)
AC_SUBST(SYBASE_INCLUDE)
AC_SUBST(SYBASE_LIBS)
AC_SUBST(SYBASE_DLLS)
AC_SUBST(SYBASE_DBLIBS)
AC_SUBST(MYSQL_INCLUDE)
AC_SUBST(MYSQL_LIBS)
AC_SUBST(BERKELEYDB_INCLUDE)
AC_SUBST(BERKELEYDB_STATIC_LIBS)
AC_SUBST(BERKELEYDB_LIBS)
AC_SUBST(ODBC_INCLUDE)
AC_SUBST(ODBC_LIBS)
AC_SUBST(PYTHON_INCLUDE)
AC_SUBST(PYTHON_LIBS)
AC_SUBST(CPPUNIT_INCLUDE)
AC_SUBST(CPPUNIT_LIBS)
AC_SUBST(NCBI_C_INCLUDE)
AC_SUBST(NCBI_C_LIBPATH)
AC_SUBST(OPENGL_INCLUDE)
AC_SUBST(OPENGL_LIBS)
AC_SUBST(OSMESA_INCLUDE)
AC_SUBST(OSMESA_LIBS)
AC_SUBST(GLUT_INCLUDE)
AC_SUBST(GLUT_LIBS)
AC_SUBST(FLTK_INCLUDE)
AC_SUBST(FLTK_LIBS)
AC_SUBST(FLTK_LIBS_GL)
AC_SUBST(FLTK_LIBS_IMAGES)
AC_SUBST(FLTK_LIBS_ALL)
AC_SUBST(FLTK_STATIC_LIBS)
AC_SUBST(FLTK_STATIC_LIBS_GL)
AC_SUBST(FLTK_STATIC_LIBS_IMAGES)
AC_SUBST(FLTK_STATIC_LIBS_ALL)
AC_SUBST(FLTK_CONFIG)
AC_SUBST(WXWIN_INCLUDE)
AC_SUBST(WXWIN_LIBS)
AC_SUBST(WXWIN_GL_LIBS)
AC_SUBST(FASTCGI_INCLUDE)
AC_SUBST(FASTCGI_LIBS)
AC_SUBST(FASTCGI_OBJS)
AC_SUBST(NCBI_SSS_INCLUDE)
AC_SUBST(NCBI_SSS_LIBPATH)
AC_SUBST(LIBSSSUTILS)
AC_SUBST(LIBSSSDB)
AC_SUBST(sss)
AC_SUBST(ssssrv)
AC_SUBST(stax)
AC_SUBST(NCBI_GEO_INCLUDE)
AC_SUBST(NCBI_GEO_LIBPATH)
AC_SUBST(LIBGEODB)
AC_SUBST(geo)
AC_SUBST(SP_INCLUDE)
AC_SUBST(SP_LIBS)
AC_SUBST(NCBI_PM_PATH)
AC_SUBST(ORBACUS_INCLUDE)
AC_SUBST(ORBACUS_LIBPATH)
AC_SUBST(LIBOB)
AC_SUBST(LIBIMR)
AC_SUBST(EXPAT_STATIC_LIBS)
AC_SUBST(SABLOT_STATIC_LIBS)
AC_SUBST(LIBXML_STATIC_LIBS)

AC_SUBST(NCBI_C_ncbi)

AC_SUBST(BINCOPY)
AC_SUBST(APP_NOCOPY)
AC_SUBST(APP_OR_NULL)
AC_SUBST(IF_REBUILDING_LIBS)
AC_SUBST(IF_REBUILDING_CONDITIONALLY)
AC_SUBST(CC_FILTER)
AC_SUBST(CXX_FILTER)
AC_SUBST(AR_FILTER)
AC_SUBST(LINK_FILTER)
AC_SUBST(CC_WRAPPER)
AC_SUBST(CXX_WRAPPER)
AC_SUBST(AR_WRAPPER)
AC_SUBST(LINK_WRAPPER)

AC_SUBST(KeepStateTarget)
AC_SUBST(Rules)
AC_SUBST(serial_ws50_rtti_kludge)
AC_SUBST(ncbicntr)

AC_SUBST(UNIX_SRC)
AC_SUBST(UNIX_USR_PROJ)

AC_SUBST(compiler)
AC_SUBST(compiler_root)
AC_SUBST(compiler_version)

AC_SUBST(COMPILER)
AC_SUBST(OSTYPE)
AC_SUBST(NCBI_PLATFORM_BITS)


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

## Create ncbicfg.c.last if necessary
ncbicfg="$builddir/corelib/ncbicfg.c"
test -f "$ncbicfg" -a ! -f "$ncbicfg.last" && cp -p "$ncbicfg" "$ncbicfg.last"

## Configure the compiler/flag-dependent header
AC_CONFIG_HEADER(${ncbiconf}:config.h.in)

##
## Configure makefiles, shell scripts, etc.
##
AC_CONFIG_FILES($configurables $srcdir/./Makefile:Makefile.in)

AC_CONFIG_COMMANDS([default], [
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

dnl Create dummy .dep files for system installations of (b)zlib.
dnl This is a stopgap measure, and fails for installations in non-system dirs.
# test -f "$status_dir"/LocalZ.enabled  ||  \
#    touch -t 197001010000 $status_dir/.z.dep
# test -f "$status_dir"/LocalBZ2.enabled  ||  \
#    touch -t 197001010000 $status_dir/.bz2.dep

dnl Avoid gratuitous relinking
ncbicfg="$builddir/corelib/ncbicfg.c"
if test -f "$ncbicfg.last" && cmp -s "$ncbicfg.last" "$ncbicfg"; then
   echo "$ncbicfg" is unchanged
   touch -r "$ncbicfg.last" "$ncbicfg"
elif test -f "$ncbicfg"; then
   echo "$ncbicfg" is updated
   rm -f "$ncbicfg.last"
   cp -p "$ncbicfg" "$ncbicfg.last"
else
   echo "$ncbicfg" is not present
fi

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
or simply run make in the current directory
$build_proj
******* CONFIGURATION SUCCESSFUL *******
EOCONF
], [
status_dir='$status_dir'
builddir='$builddir'
build_root='$build_root'
x_configurables='$x_configurables'
ncbiconf='$ncbiconf'
CFLAGS='$CFLAGS'
CXXFLAGS='$CXXFLAGS'
CPPFLAGS='$CPPFLAGS'
LDFLAGS='$LDFLAGS'
LIB_OR_DLL='$LIB_OR_DLL'
with_dll='$with_dll'
WithFeatures='$WithFeatures'
WithPackages='$WithPackages'
WithoutPackages='$WithoutPackages'
WithProjects='$WithProjects $OPT_GROUPS'
WithoutProjects='$WithoutProjects'
with_extra_action='$with_extra_action'
with_projects='$with_projects'
smart_pwd='$smart_pwd'
real_srcdir='$real_srcdir'
])

AC_OUTPUT

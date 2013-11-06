#############################################################################
#  $Id$
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
#      a) "<builddir>/inc/ncbiconf_unix.h"
#      b) "<builddir>/build/Makefile.mk"
#      c) other Makefile's and scripts in "<builddir>/build" and below
#
#############################################################################

AC_PREREQ(2.59)

dnl Early setup, most crucially for locking.  The diversion magic lets
dnl this occur before AC_INIT, which already interferes with other
dnl configure processes.

AC_DIVERT_PUSH(INIT_PREPARE)

# Check --with-3psw=.../--without-3psw early because a lot hinges on it
orig_NCBI=$NCBI
case "$with_3psw" in
   '' | yes | [[\$/][Nn]*:[SsLl]*] | [[Nn]*:[SsLl]*] )
      ncbi_favor_std_packages=no
      ;;
   [[SsLl]]*:* )
      ncbi_favor_std_packages=yes
      ;;
   [[SsLl]]* )
      AS_UNSET(NCBI)
      ;;
   no )
      if test "${with_ncbi_c-no}" != "no"; then
         AC_MSG_ERROR([incompatible options: --with-ncbi-c but --without-3psw])
      else
         with_ncbi_c=no
      fi
      m4_foreach(X, [sss, sssutils, sssdb, z, bz2, lzo, pcre,
                     gcrypt, gnutls, openssl, krb5, boost,
                     sybase, ftds, mysql, opengl, mesa, glut, glew,
                     wxwidgets, freetype, ftgl, fastcgi, bdb, orbacus, odbc,
                     python, perl, jni, sqlite3, mimetic, sge, icu, sp, expat,
                     sablot, libxml, libxslt, libexslt, xerces, xalan, zorba,
                     oechem, muparser, hdf5, gif, jpeg, png, tiff, xpm,
                     magic, curl, gsoap, mongodb],
        [if test "${[with_]X-no}" != "no"; then
            AC_MSG_ERROR([incompatible options: --with-]X[ but --without-3psw])
         else
            [with_]X=no
         fi
        ])
      AS_UNSET(NCBI)
      ;;
   * )
      AC_MSG_ERROR([Unsupported option --with-3psw=$with_3psw])
      ;;
esac

if $srcdir/scripts/common/impl/get_lock.sh configure $$; then
    ac_clean_files=configure.lock
else
    exit 1
fi
# reconfigure.sh expects config.cache to exist; unconditionally clobber
# any command-line setting.
cache_file=config.cache
if test -z "$CONFIG_SITE" -a -r $srcdir/src/build-system/config.site; then
    CONFIG_SITE=$srcdir/src/build-system/config.site
fi
AC_DIVERT_POP

AC_INIT([ncbi-tools++],[0.0],[cpp-core@ncbi.nlm.nih.gov])
AC_CONFIG_SRCDIR(src/build-system/Makefile.mk.in)



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
AC_ARG_WITH(max-debug,
   [ --with-max-debug        enable extra runtime checks (esp. of STL usage)])
AC_ARG_WITH(symbols,
   [ --with-symbols          retain debugging symbols in non-debug mode])
AC_ARG_WITH(optimization,
   [ --without-optimization  turn off optimization flags in non-debug mode])
AC_ARG_WITH(profiling,
   [ --with-profiling        build profiled versions of libs and apps])
AC_ARG_WITH(tcheck,
   [ --with-tcheck(=DIR)     build for Intel Thread Checker (in DIR)])
AC_ARG_WITH(dll,
   [ --with-dll              build all libraries as DLLs])
AC_ARG_WITH(static,
   [ --with-static           build all libraries statically even if --with-dll])
AC_ARG_WITH(static-exe,
   [ --with-static-exe       build all executables as statically as possible])
AC_ARG_WITH(plugin-auto-load,
   [ --with-plugin-auto-load always enable the plugin manager by default])
AC_ARG_WITH(bundles,
   [ --with-bundles          build bundles in addition to dylibs on Mac OS X])
AC_ARG_WITH(bin-release,
   [ --with-bin-release      build executables suitable for public release])
AC_ARG_WITH(mt,
   [ --with-mt               compile in a MultiThread-safe manner])
AC_ARG_WITH(64,
   [ --with-64               compile to 64-bit code])
AC_ARG_WITH(universal,
   [ --with-universal        build universal binaries on Mac OS X])
AC_ARG_WITH(universal2,
   [ --with-universal=CPUs   build universal binaries targeting the given CPUs])
AC_ARG_WITH(exe,
   [ --without-exe           do not build executables])
AC_ARG_WITH(runpath,
   [ --with-runpath=PATH     specify the usual runtime path to DLLs])
AC_ARG_WITH(relative-runpath,
   [ --with-relative-runpath=P specify an executable-relative DLL search path])
AC_ARG_WITH(hard-runpath,
   [ --with-hard-runpath     hard-code runtime path, ignoring LD_LIBRARY_PATH])
AC_ARG_WITH(lfs,
   [ --with-lfs              enable large file support to the extent possible])
AC_ARG_WITH(limited-linker,
   [ --with-limited-linker   don't attempt to build especially large projects])

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
AC_ARG_WITH(build-root-sfx,
   [ --with-build-root-sfx=X add a user-specified suffix to the build dir name])
AC_ARG_WITH(execopy,
   [ --without-execopy       do not copy built executables to the BIN area])
AC_ARG_WITH(bincopy,
   [ --with-bincopy          populate lib and bin with copies, not hard links])
AC_ARG_WITH(lib-rebuilds,
   [ --with-lib-rebuilds     ensure that apps use up-to-date libraries])
AC_ARG_WITH(lib-rebuilds2,
   [ --with-lib-rebuilds=ask ask whether to update each app's libraries])dnl'
AC_ARG_WITH(deactivation,
   [ --without-deactivation  keep old copies of libraries that no longer build])
AC_ARG_WITH(makefile-auto-update,
   [ --without-makefile-auto-update  do not auto-update generated makefiles])
AC_ARG_WITH(projects,
   [ --with-projects=FILE    build projects listed in FILE by default])
AC_ARG_WITH(flat-makefile,
   [ --without-flat-makefile do not generate an all-encompassing flat makefile])
AC_ARG_WITH(configure-dialog,
   [ --with-configure-dialog allow interactive flat makefile project selection])
AC_ARG_WITH(saved-settings,
   [ --with-saved-settings=F load configuration settings from the file F])
AC_ARG_WITH(check,
   [ --with-check            run test suite after the build])
AC_ARG_WITH(check-tools,
   [ --with-check-tools=...  use the specified tools for testing])
AC_ARG_WITH(ncbi-public,
   [ --with-ncbi-public      ensure compatibility for all in-house platforms])
AC_ARG_WITH(strip,
   [ --with-strip            strip binaries at build time])
AC_ARG_WITH(pch,
   [ --with-pch              use precompiled headers if possible])
AC_ARG_WITH(caution,
   [ --with-caution          cancel configuration unconditionally when in doubt])
AC_ARG_WITH(caution2,
   [ --without-caution       proceed without asking when in doubt])
AC_ARG_WITH(ccache,
   [ --without-ccache        do not automatically use ccache if available])
AC_ARG_WITH(distcc,
   [ --without-distcc        do not automatically use distcc if available])

## NCBI packages
AC_ARG_WITH(ncbi-c,
   [ --with-ncbi-c=DIR       use NCBI C Toolkit installation in DIR])
AC_ARG_WITH(ncbi-c2,
   [ --without-ncbi-c        do not use NCBI C Toolkit])
AC_ARG_WITH(sss,
   [ --with-sss=DIR          use NCBI SSS installation in DIR])
AC_ARG_WITH(sss2,
   [ --without-sss           do not use NCBI SSS libraries])
AC_ARG_WITH(sssutils,
   [ --without-utils         do not use NCBI SSS UTIL library])
AC_ARG_WITH(sssdb,
   [ --without-sssdb         do not use NCBI SSS DB library])
AC_ARG_WITH(included-sss,
   [ --with-included-sss     use the in-tree copy of SSS])

## Third-party and system packages
AC_ARG_WITH(z,
   [ --with-z=DIR            use zlib installation in DIR])
AC_ARG_WITH(z2,
   [ --without-z             use internal copy of zlib])
AC_ARG_WITH(bz2,
   [ --with-bz2=DIR          use bzlib installation in DIR])
AC_ARG_WITH(bz2b,
   [ --without-bz2           use internal copy of bzlib])
AC_ARG_WITH(lzo,
   [ --with-lzo=DIR          use LZO installation in DIR (requires 2.x or up)])
AC_ARG_WITH(lzo_,
   [ --without-lzo           do not use LZO])
AC_ARG_WITH(pcre,
   [ --with-pcre=DIR         use PCRE installation in DIR])
AC_ARG_WITH(pcre2,
   [ --without-pcre          use internal copy of PCRE])
AC_ARG_WITH(gcrypt,
   [ --with-gcrypt=DIR       use gcrypt installation in DIR])
AC_ARG_WITH(gcrypt2,
   [ --without-gcrypt        do not use gcrypt])
AC_ARG_WITH(gnutls,
   [ --with-gnutls=DIR       use GNUTLS installation in DIR])
AC_ARG_WITH(gnutls2,
   [ --without-gnutls        do not use GNUTLS])
AC_ARG_WITH(openssl,
   [ --with-openssl=DIR      use OpenSSL installation in DIR])
AC_ARG_WITH(openssl2,
   [ --without-openssl       do not use OpenSSL])
AC_ARG_WITH(krb5,
   [ --with-krb5=DIR         use Kerberos 5 installation in DIR])
AC_ARG_WITH(krb5b,
   [ --without-krb5          do not use Kerberos 5])
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
AC_ARG_WITH(glew,
   [ --without-glew          do not use GLEW])
AC_ARG_WITH(glew2,
   [ --with-glew=DIR         use GLEW installation in DIR])
AC_ARG_WITH(glew-mx,
   [ --with-glew-mx          insist on a multi-context-ready GLEW installation])
AC_ARG_WITH(wxwidgets,
   [ --without-wxwidgets     do not use wxWidgets (2.6+)])
AC_ARG_WITH(wxwidgets2,
   [ --with-wxwidgets=DIR    use wxWidgets installation in DIR])
AC_ARG_WITH(wxwidgets-ucs,
   [ --with-wxwidgets-ucs    use Unicode builds of wxWidgets])
AC_ARG_WITH(wxwidgets-ucs2,
   [ --without-wxwidgets-ucs do not use Unicode builds of wxWidgets])
AC_ARG_WITH(freetype,
   [ --without-freetype      do not use FreeType])
AC_ARG_WITH(freetype_,
   [ --with-freetype=DIR     use FreeType installation in DIR])
AC_ARG_WITH(ftgl,
   [ --without-ftgl          do not use FTGL])
AC_ARG_WITH(ftgl2,
   [ --with-ftgl=DIR         use FTGL installation in DIR])
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
AC_ARG_WITH(python,
   [ --with-python=DIR       use Python installation in DIR])
AC_ARG_WITH(python2,
   [ --without-python        do not use Python])
AC_ARG_WITH(perl,
   [ --with-perl=DIR         use Perl installation in DIR])
AC_ARG_WITH(perl2,
   [ --without-perl          do not use Perl])
AC_ARG_WITH(jni,
   [ --with-jni(=JDK-DIR)    build Java bindings (against the JDK in JDK-DIR)])
AC_ARG_WITH(boost,
   [ --with-boost=DIR        use Boost installation in DIR])
AC_ARG_WITH(boost2,
   [ --without-boost         do not use Boost])
AC_ARG_WITH(boost-tag,
   [ --with-boost-tag=TAG    use TAG as the tag appended to Boost library names])
AC_ARG_WITH(boost-tag2,
   [ --without-boost-tag     use untagged Boost library names])
AC_ARG_WITH(sqlite3,
   [ --with-sqlite3=DIR      use SQLite 3.x installation in DIR])
AC_ARG_WITH(sqlite3b,
   [ --without-sqlite3       do not use SQLite 3.x])
AC_ARG_WITH(icu,
   [ --with-icu=DIR          use ICU installation in DIR])
AC_ARG_WITH(icu2,
   [ --without-icu           do not use ICU])
AC_ARG_WITH(expat,
   [ --with-expat=DIR        use Expat installation in DIR])
AC_ARG_WITH(expat2,
   [ --without-expat         do not use Expat])
AC_ARG_WITH(sablot,
   [ --with-sablot=DIR       use Sablotron installation in DIR])
AC_ARG_WITH(sablot2,
   [ --without-sablot        do not use Sablotron])
AC_ARG_WITH(libxml,
   [ --with-libxml=DIR       use libxml2 installation in DIR])
AC_ARG_WITH(libxml_,
   [ --without-libxml        do not use libxml2])
AC_ARG_WITH(libxslt,
   [ --with-libxslt=DIR      use libxslt installation in DIR])
AC_ARG_WITH(libxslt2,
   [ --without-libxslt       do not use libxslt])
AC_ARG_WITH(libexslt,
   [ --with-libexslt=DIR     use libexslt installation in DIR])
AC_ARG_WITH(libexslt2,
   [ --without-libexslt      do not use libexslt])
AC_ARG_WITH(xerces,
   [ --with-xerces=DIR       use Xerces-C++ installation in DIR])
AC_ARG_WITH(xerces2,
   [ --without-xerces        do not use Xerces-C++])
AC_ARG_WITH(xalan,
   [ --with-xalan=DIR        use Xalan-C++ installation in DIR])
AC_ARG_WITH(xalan2,
   [ --without-xalan         do not use Xalan-C++])
AC_ARG_WITH(zorba,
   [ --with-zorba=DIR        use Zorba installation in DIR])
AC_ARG_WITH(zorba2,
   [ --without-zorba         do not use Zorba])
AC_ARG_WITH(oechem,
   [ --with-oechem=DIR       use OpenEye OEChem installation in DIR])
AC_ARG_WITH(oechem2,
   [ --without-oechem        do not use OEChem])
AC_ARG_WITH(sge,
   [ --with-sge=DIR          use Sun/Univa Grid Engine installation in DIR])
AC_ARG_WITH(sge2,
   [ --without-sge           do not use Sun/Univa Grid Engine])
AC_ARG_WITH(muparser,
   [ --with-muparser=DIR     use muParser installation in DIR])
AC_ARG_WITH(muparser2,
   [ --without-muparser      do not use muParser])
AC_ARG_WITH(hdf5,
   [ --with-hdf5=DIR         use HDF5 installation in DIR])
AC_ARG_WITH(hdf5b,
   [ --without-hdf5          do not use HDF5])
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
AC_ARG_WITH(magic,
   [ --with-magic=DIR        use libmagic installation in DIR])
AC_ARG_WITH(magic2,
   [ --without-magic         do not use libmagic])
AC_ARG_WITH(curl,
   [ --with-curl=DIR         use libcurl installation in DIR])
AC_ARG_WITH(curl2,
   [ --without-curl          do not use libcurl])
AC_ARG_WITH(mimetic,
   [ --with-mimetic=DIR      use libmimetic installation in DIR])
AC_ARG_WITH(mimetic2,
   [ --without-mimetic       do not use libmimetic])
AC_ARG_WITH(gsoap,
   [ --with-gsoap=DIR        use gSOAP++ installation in DIR])
AC_ARG_WITH(gsoap2,
   [ --without-gsoap         do not use gSOAP++])
AC_ARG_WITH(mongodb,
   [ --with-mongodb=DIR      use MongoDB installation in DIR])
AC_ARG_WITH(mongodb2,
   [ --without-mongodb       do not use MongoDB])
AC_ARG_WITH(3psw,
   [ --with-3psw=std:netopt  favor standard (system) builds of the above pkgs.])
AC_ARG_WITH(3psw2,
   [ --without-3psw          do not use any of the above packages])

## Optional projects
AC_ARG_WITH(local-lbsm,
   [ --without-local-lbsm    turn off support for IPC with locally running LBSMD])
AC_ARG_WITH(ncbi-crypt,
   [ --without-ncbi-crypt    use a dummy stubbed-out version of ncbi_crypt])
AC_ARG_WITH(connext,
   [ --without-connext       do not build non-public CONNECT library extensions])
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
   [ --without-gui           do not build most graphical projects])
AC_ARG_WITH(algo,
   [ --without-algo          do not build CPU-intensive algorithms])
AC_ARG_WITH(internal,
   [ --with-internal         always try to build internal projects])
AC_ARG_WITH(internal2,
   [ --without-internal      never build internal projects])
AC_ARG_WITH(gbench,
   [ --with-gbench           ensure that Genome Workbench can be built])
AC_ARG_WITH(gbench2,
   [ --without-gbench        do not build Genome Workbench])

## Manuals
AC_ARG_WITH(manuals,
   [***** See also HTML documentation in ./doc/index.html *****])


#### Check the passed arguments against the list of available ones
x_with_list="\
debug max-debug symbols optimization profiling tcheck dll static static-exe \
plugin-auto-load bundles bin-release mt 64 universal exe runpath hard-runpath \
lfs limited-linker \
autodep suffix hostspec version execopy bincopy lib-rebuilds lib-rebuilds=ask \
deactivation makefile-auto-update projects flat-makefile configure-dialog \
check ncbi-public strip pch caution ccache distcc \
ncbi-c wxwidgets wxwidgets-ucs fastcgi sss sssdb sssutils included-sss \
geo included-geo \
z bz2 lzo pcre gcrypt gnutls openssl krb5 sybase sybase-local sybase-new \
ftds mysql orbacus freetype ftgl opengl mesa glut glew glew-mx \
bdb python perl jni sqlite3 icu boost boost-tag \
sp expat sablot libxml libxslt libexslt xerces xalan zorba \
oechem sge muparser hdf5 \
gif jpeg tiff png xpm magic curl mimetic gsoap mongodb 3psw \
local-lbsm ncbi-crypt connext \
serial objects dbapi app ctools gui algo internal gbench"

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
      --with-extra-action= | --exec-prefix= | --with-projects= | --srcdir= \
      | --cache-file= | --build= | --host= | --target= | --with-runpath= \
      | --with-relative-runpath= | --x-includes= | --x-libraries= )
      AC_MSG_ERROR([$x_arg:  requires value;  use --help to show usage])
      ;;

      --with-extra-action=* | --with-build-root=* | --with-build-root-sfx=* \
      | --with-fake-root=* | --with-saved-settings=* \
      | --with-projects=* | --with-check=* | --with-check-tools=* \
      | --with-universal=* | --with-tcheck=* \
      | --cache-file=* | --build=* | --host=* | --prefix=* | --exec-prefix=* \
      | --libdir=* | --bindir=* | --includedir=* | --srcdir=* \
      | [[A-Z]*=*] \
      | --with-z=* | --with-bz2=* | --with-lzo=* \
      | --with-pcre=* | --with-ncbi-c=* | --with-sss=* \
      | --with-gcrypt=* | --with-gnutls=* | --with-openssl=* \
      | --with-krb5=* | --with-curl=* | --with-gsoap=* | --with-mongodb=* \
      | --with-sybase-local=* | --with-wxwidgets=* | --with-mimetic=* \
      | --with-ftds=* | --with-mysql=* | --with-fastcgi=* \
      | --with-sqlite3=* | --with-expat=* | --with-sablot=* \
      | --with-icu=* | --with-xerces=* | --with-xalan=* | --with-zorba=* \
      | --with-libxml=* | --with-libxslt=* | --with-exslt=* \
      | --with-bdb=* | --with-odbc=* | --with-python=* | --with-perl=* \
      | --with-freetype=* | --with-ftgl=* | --with-gif=* | --with-jpeg=* \
      | --with-png=* | --with-tiff=* | --with-xpm=* | --with-opengl=* \
      | --with-mesa=* | --with-glut=* | --with-glew=* | --with-oechem=* \
      | --with-boost=* | --with-boost-tag=* | --with-orbacus=* | --with-sge=* \
      | --with-muparser=* | --with-hdf5=* | --with-jni=* | --with-magic=* \
      | --x-includes=* | --x-libraries=* | --with-3psw=* \
      | --target=* | --with-runpath=* | --with-relative-runpath=* \
      | --no-create | --no-recursion)
      ;;

      * )
      AC_MSG_ERROR([$x_arg:  unknown option;  use --help to show usage])
      ;;
   esac
done


if test "$with_gbench" = "yes" ; then
   m4_foreach(OPT, [dll,mt,gui,exe,serial,objects,algo,glew_mx],
     [if test "$[with_]OPT" = "no"; then
         AC_MSG_ERROR([incompatible options: --without-]OPT[ but --with-gbench])
      else
         [with_]OPT=yes
      fi
     ])
   : ${with_projects=scripts/projects/ncbi_gbench.lst}
elif test "$with_jni" = "yes" ; then
   m4_foreach(OPT, [dll,mt],
     [if test "$[with_]OPT" = "no"; then
         AC_MSG_ERROR([incompatible options: --without-]OPT[ but --with-jni])
      else
         [with_]OPT=yes
      fi
     ])
   : ${with_exe=no}
fi

if test "$with_bin_release" = "yes" ; then
   # Default some other options accordingly
   : ${with_ncbi_public=yes}
   : ${with_dll=no}
   : ${with_local_lbsm=no}
   : ${with_ncbi_crypt=no}
   : ${with_connext=no}
   AC_DEFINE(NCBI_BIN_RELEASE, 1,
             [Define to 1 when building binaries for public release.])
fi

if test "$with_debug" = no; then
    : ${with_symbols=no}
    : ${with_optimization=yes}
    if test "$with_max_debug" = yes; then
       AC_MSG_ERROR([incompatible options: --without-debug but --with-max-debug])
    fi
else
    : ${with_symbols=yes}
    : ${with_optimization=no}
fi

#### Check for special options
if test "$with_extra_action" = "yes" ; then
   AC_MSG_ERROR([--with-extra-action must have a value after =])
fi
if test "$with_extra_action" = "no" ; then
   AC_MSG_ERROR([--without-extra-action is not allowed])
fi

case "$with_projects" in
 ""|yes)
     if test -r projects; then
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

if test -n "$with_projects"; then
   case "$with_projects" in
      /* ) abs_projects=$with_projects         ;;
      yes) abs_projects=$srcdir/projects       ;;
      *  ) abs_projects=$srcdir/$with_projects ;;
   esac
   test -r "$abs_projects"  ||  \
      AC_MSG_ERROR([unable to read requested projects file "$abs_projects".])
fi

# Check for custom optimization flags before potentially going with defaults.
skip_fast_flags=no
if test -z "${DEF_FAST_FLAGS}${FAST_CXXFLAGS}"; then
   case " $CFLAGS $CXXFLAGS" in
      *\ -O* | *\ -xO* ) skip_fast_flags=yes ;;
   esac
fi
# Generally save any originally specified flags.
USER_CFLAGS=$CFLAGS
USER_CXXFLAGS=$CXXFLAGS
USER_LDFLAGS=$LDFLAGS

#### Always define this
AC_DEFINE(NCBI_CXX_TOOLKIT, 1, [This is the NCBI C++ Toolkit.])


#### Get the running host's properties
AC_CONFIG_AUX_DIR(src/build-system)
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
case "$CXX" in
   insure* | */insure*)
      psrcdump=`echo "$CXX" | sed -e 's/insure.*/psrcdump/'`
      real_CXX=`$psrcdump -t insure++ | sed -ne 's/^Compiler \(.*\)/\1/p'`
      real_dir=`$psrcdump -t insure++ | sed -ne 's/^Compilerinstalldir \(.*\)/\1/p'`
      test -n "$real_dir" && test -x "$real_dir/$real_CXX" && \
        real_CXX="$real_dir/$real_CXX"
      case "$real_CXX" in
          *gcc* ) LIBS="$LIBS -lstdc++"
      esac
      ;;
   *)
      real_CXX=$CXX
      ;;
esac


#### Detect KCC compiler
if test "$GCC" != "yes" ; then
   touch kcc_test.cpp
   kcc_ver="`$real_CXX -V -c kcc_test.cpp 2>&1 | grep '^KAI '`"
   kcc_license="`$real_CXX -V -c kcc_test.cpp 2>&1 | grep -v '^KAI '`"
   rm -f kcc_test.cpp kcc_test.o
   case "$kcc_ver" in
      KAI* )  KCC="yes" ;;
   esac
fi


#### Detect ICC compiler (newer versions of which configure can mis-ID as GCC)
if test "$KCC" != "yes"; then
   icc_ver="`$real_CXX -V 2>&1 | grep '^Intel(R) C'`"
   icc_license="`$real_CXX -c 2>&1 | grep '^icc: NOTE: The evaluation period for this product ends on '`"
   case "$icc_ver" in
      Intel*C*Compiler* )  ICC="yes"; GCC="no" ;;
   esac
fi


#### Detect IBM VisualAge compiler
if test "$GCC" != "yes"  -a  "$KCC" != "yes"  -a  "$ICC" != "yes"; then
   vac_ver="`$real_CXX -qversion 2>&1`"
   case "$vac_ver" in
      *AIX* ) VAC="yes" ;;
   esac
fi


#### Detect Compaq compiler
if test "$GCC" != "yes"  -a  "$KCC" != "yes"  -a  "$ICC" != "yes" \
  -a  "$VAC" != "yes"; then
   ccc_ver="`$real_CXX -V 2>&1 | grep '^Compaq C++ V'`"
   case "$ccc_ver" in
      *Compaq* ) CCC="yes" ;;
   esac
fi


#### Detect supported compilers
compiler=
compiler_ver=
ncbi_compiler="UNKNOWN"
ncbi_compiler_ver="0"

if test "$GCC" = "yes" ; then
   compiler_ver="`$real_CXX -dumpversion 2>&1`"
   case "$compiler_ver" in
     2.95* | 2.96* | 3.* | 4.* )
       compiler="GCC"
       ncbi_compiler="GCC"
       ncbi_compiler_ver="$compiler_ver"
       NCBI_FEATURE(GCC)
       ;;
   esac
elif test "$KCC" = "yes" ; then
   compiler_ver="$kcc_ver"
   compiler="KCC"
   ncbi_compiler="KCC"
   ncbi_compiler_ver="`echo $kcc_ver | sed 's%^KAI .*C[[+]][[+]] \([[0-9.]]*\).*%\1%'`"
   NCBI_FEATURE(KCC)
elif test "$ICC" = "yes" ; then
   compiler_ver="$icc_ver"
   compiler="ICC"
   ncbi_compiler="ICC"
   ncbi_compiler_ver="`echo $icc_ver | sed 's%.*Version \([[0-9.]]*\).*%\1%'`"
   NCBI_FEATURE(ICC)
elif test "$VAC" = "yes" ; then
   compiler_ver="$vac_ver"
   compiler="VisualAge"
   ncbi_compiler="VISUALAGE"
   ncbi_compiler_ver="`echo $vac_ver | sed 's%.*[[Vv]]ersion:* 0*\([[0-9.]]*\).*%\1%'`"
   NCBI_FEATURE(VisualAge)
elif test "$CCC" = "yes" ; then
   compiler_ver="$ccc_ver"
   compiler=Compaq
   ncbi_compiler="COMPAQ"
   ncbi_compiler_ver="`echo $ccc_ver | sed 's%.*C[[+]][[+]] V\([[0-9.]]*\).*%\1%'`"
   NCBI_FEATURE(CompaqCompiler)
else
   case "$host_os" in
    solaris* )
      compiler_ver="`$real_CXX -V 2>&1`"
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
        *Sun*C*5\.5* )
          compiler="WorkShop55"
          ncbi_compiler="WORKSHOP"
          ncbi_compiler_ver="5.5" ;;
        *Sun*C*5\.8* )
          compiler="WorkShop58"
          ncbi_compiler="WORKSHOP"
          ncbi_compiler_ver="5.8" ;;
        *Sun*C*5\.9* )
          compiler="WorkShop59"
          ncbi_compiler="WORKSHOP"
          ncbi_compiler_ver="5.9" ;;
        *Sun*C*5\.10* )
          compiler="WorkShop510"
          ncbi_compiler="WORKSHOP"
          ncbi_compiler_ver="5.10" ;;
        *Sun*C*5\.11* )
          compiler="WorkShop511"
          ncbi_compiler="WORKSHOP"
          ncbi_compiler_ver="5.11" ;;
      esac
      NCBI_FEATURE(WorkShop)
      ;;

    irix* )
      compiler_ver="`$real_CXX -version 2>&1`"
      case "$compiler_ver" in
        MIPSpro*Compilers:*Version*7\.3* )
          compiler="MIPSpro73"
          ncbi_compiler="MIPSPRO"
          ncbi_compiler_ver="7.3"
          NCBI_FEATURE(MIPSpro)
          ;;
      esac
      ;;

    cygwin* )
      case "$real_CXX" in
        *cl\.exe* | cl)
          compiler="MSVC"
          ncbi_compiler="MSVC"
          ncbi_compiler_ver="6.0"
          NCBI_FEATURE(MSVC)
          ;;
      esac
      ;;
   esac
fi

changequote(, )dnl
#ncbi_compiler_ver="`echo $ncbi_compiler_ver | sed 's%\([0-9]\)\.\([0-9]\)\.\([0-9]\).*%\1\2\3%; s%\([0-9]\)\.\([0-9][0-9]*\).*%\1\2%; s%^\([0-9][0-9]\)$%\10%; s%^\([0-9]\)$%\100%'`"
case "$compiler:$ncbi_compiler_ver" in
    WorkShop*:?.?? )   ncbi_compiler_sed='s/\([0-9]\)\.\([0-9][0-9]\)/\1\20/' ;;
    WorkShop*:[6-9].?) ncbi_compiler_sed='s/\([0-9]\)\.\([0-9\)/\10\20/' ;;
    ICC:??.? )  ncbi_compiler_sed='s/\([0-9][0-9]\)\.\([0-9]\)/\1\20/' ;;
    ICC:??.?.? | \
    ICC:??.?.?.* ) ncbi_compiler_sed='s/\([0-9][0-9]\)\.\([0-9]\)\.\([0-9]\).*/\1\2\3/' ;;
    *:?.?.?)    ncbi_compiler_sed='s/\([0-9]\)\.\([0-9]\)\.\([0-9]\)/\1\2\3/' ;;
    *:?.??*)    ncbi_compiler_sed='s/\([0-9]\)\.\([0-9][0-9]\).*/\1\2/' ;;
    *:?.?)      ncbi_compiler_sed='s/\([0-9]\).\([0-9]\)/\1\20/' ;;
    *:?)        ncbi_compiler_sed='s/\([0-9]\)/\100/' ;;
changequote([, ])dnl
    *)    AC_MSG_ERROR([Do not know how to parse $compiler version number $ncbi_compiler_ver]) ;;
esac
ncbi_compiler_ver=`echo $ncbi_compiler_ver | sed "$ncbi_compiler_sed"`
compiler_version=$ncbi_compiler_ver

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

AC_DEFINE_UNQUOTED(NCBI_COMPILER, "$ncbi_compiler", [Compiler name])
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
       NCBI_CAUTION([Do you still want to proceed (at your own risk)?])
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
   C_LINK='$(top_srcdir)/scripts/common/impl/favor-static $(CC)'
   LINK='$(top_srcdir)/scripts/common/impl/favor-static $(CXX)'
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
    darwin* | irix* | cygwin*)  ;;
    solaris*:WorkShop*       )  : ${MT_FLAG:="-mt"} ;;
    solaris*:GCC             )  : ${MT_FLAG:="-pthreads"} ;;
    *:KCC                    )  : ${MT_FLAG:="--thread_safe"} ;;
    *:Compaq | *:GCC | *:ICC )  : ${MT_FLAG:="-pthread"} ;;
    *:VisualAge              )  : ${MT_FLAG:="-qthreaded"} ;;
    * )   AC_MSG_ERROR([Do not know how to build MT-safe with compiler $CXX $compiler_ver]) ;;
   esac
   CPPFLAGS="$CPPFLAGS -D_MT -D_REENTRANT -D_THREAD_SAFE"
   case "$host_os:$compiler" in
     solaris2.10:GCC ) : ${THREAD_LIBS:="-lposix4"} ;;
     solaris*        ) : ${THREAD_LIBS:="-lpthread -lposix4"} ;;
     freebsd*        ) ;; # -pthread already substitutes libc_r for libc
     *               ) : ${THREAD_LIBS:="-lpthread"} ;;
   esac
   LIBS="$LIBS $THREAD_LIBS"
   case "$host_os:$compiler" in
     solaris2.??:* | solaris*:GCC | *:Compaq | irix* | aix* | darwin* | cygwin*)
        NCBIATOMIC_LIB=
        ;;
     *:GCC | *:ICC | linux*:KCC )
        case "$host_cpu" in
           i?86 | powerpc* | ppc* | x86_64 | sparc* ) NCBIATOMIC_LIB= ;;
           * ) NCBIATOMIC_LIB=xncbi ;;
        esac
        ;;
     *:WorkShop* | solaris*:KCC )
        NCBIATOMIC_LIB=xncbi
        ncbicntr="ncbicntr_workshop ncbiatomic_workshop"
        ;;
     * ) NCBIATOMIC_LIB=xncbi ;;
   esac
   case "$compiler" in
      GCC )
         if $CC -v | grep clang >/dev/null; then
            :
            # Will allegedly support -openmp at some point, but as of 3.4,
            # just parses it as specifying an output filename: -o penmp.
         else
            case "$compiler_version" in
               [[123]]?? | 4[[01]]? ) ;;
               * ) : ${OPENMP_FLAGS=-fopenmp} ;;
            esac
         fi
         ;;
      ICC )
         : ${OPENMP_FLAGS=-openmp}
         ;;
      WorkShop* )
         : ${OPENMP_FLAGS=-xopenmp=parallel}
         ;;
   esac
   mt_sfx="MT"
   NCBI_FEATURE(MT)
else
   CPPFLAGS="$CPPFLAGS -DNCBI_WITHOUT_MT"
   MT_FLAG=
   THREAD_LIBS=
   NCBIATOMIC_LIB=
   OPENMP_FLAGS=
   mt_sfx=""
fi
MT_SFX="${mt_sfx}"
AC_SUBST(MT_SFX)

CFLAGS="$MT_FLAG $CFLAGS"
CXXFLAGS="$MT_FLAG $CXXFLAGS"
LDFLAGS="$MT_FLAG $LDFLAGS"

APP_LDFLAGS=
DLL_LDFLAGS=

### Should go before any test compiler runs
AC_GNU_SOURCE

case "$host_os:$compiler" in
   darwin*:GCC )
      AC_CACHE_CHECK([whether $CC supports -Wl,-rpath],
         ncbi_cv_prog_cc_wl_rpath,
         [orig_LDFLAGS=$LDFLAGS
          LDFLAGS="-Wl,-rpath,. $LDFLAGS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM([],[])],
             [ncbi_cv_prog_cc_wl_rpath=yes], [ncbi_cv_prog_cc_wl_rpath=no])
          LDFLAGS=$orig_LDFLAGS])
      ;;
   *:GCC | *:ICC )
      AC_CACHE_CHECK([whether $CC has an option to export all symbols],
         ncbi_cv_prog_cc_export_all,
         [orig_LDFLAGS=$LDFLAGS
          ncbi_cv_prog_cc_export_all=no
          for x in -Wl,--export-all-symbols -rdynamic -Wl,-E; do
             LDFLAGS="$x $orig_LDFLAGS"
             AC_LINK_IFELSE([AC_LANG_PROGRAM([],[])],
                [ncbi_cv_prog_cc_export_all=$x])
             test "x$ncbi_cv_prog_cc_export_all" = "xno"  ||  break
          done
          LDFLAGS=$orig_LDFLAGS])
      test "$ncbi_cv_prog_cc_export_all" = no  || \
         LDFLAGS="$ncbi_cv_prog_cc_export_all $LDFLAGS"
      AC_CACHE_CHECK([whether $CC supports -Wl,--{en,dis}able-new-dtags],
         ncbi_cv_prog_cc_new_dtags,
         [orig_LDFLAGS=$LDFLAGS
          LDFLAGS="-Wl,--enable-new-dtags $LDFLAGS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM([],[])],
             [ncbi_cv_prog_cc_new_dtags=yes], [ncbi_cv_prog_cc_new_dtags=no])
          LDFLAGS=$orig_LDFLAGS])
      if test "$ncbi_cv_prog_cc_new_dtags" = yes; then
          if test "$with_hard_runpath" = yes; then
             LDFLAGS="-Wl,--disable-new-dtags $LDFLAGS"
          else
             LDFLAGS="-Wl,--enable-new-dtags $LDFLAGS"
          fi
      fi
      ;;
esac

case "$compiler:$compiler_version" in
   GCC:4[[0-6]]? | ICC:1[[01]]?? )
     ncbi_cv_prog_cxx_11=no
     ncbi_cv_prog_c_99=no
     ;;
esac

AC_CACHE_CHECK([how to enable C++ '11 features in $CXX],
   ncbi_cv_prog_cxx_11,
   [orig_CXX=$CXX
    ncbi_cv_prog_cxx_11=no
    for x in -std=gnu++11 -std=gnu++0x; do
       CXX="$orig_CXX $x"
       AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
          [ncbi_cv_prog_cxx_11=$x])
          test "x$ncbi_cv_prog_cxx_11" = "xno"  ||  break
       done
       CXX=$orig_CXX])
test "$ncbi_cv_prog_cxx_11" = no  ||  CXX="$CXX $ncbi_cv_prog_cxx_11"

AC_LANG_PUSH(C)
AC_CACHE_CHECK([how to enable C '11 or at least '99 features in $CC],
   ncbi_cv_prog_c_99,
   [orig_CC=$CC
    ncbi_cv_prog_c_99=no
    for x in -xc99=all "-std=gnu11 -fgnu89-inline" \
             "-std=gnu1x -fgnu89-inline" \
             "-std=gnu99 -fgnu89-inline" \
             "-std=gnu9x -fgnu89-inline"; do
       CC="$orig_CC $x"
       AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
          [ncbi_cv_prog_c_99=$x])
          test "x$ncbi_cv_prog_c_99" = "xno"  ||  break
       done
       CC=$orig_CC])
AC_LANG_POP(C)
test "$ncbi_cv_prog_c_99" = no  ||  CC="$CC $ncbi_cv_prog_c_99"

#### Provide default environment setup for known platforms/compilers
DEPFLAGS="-M"
DEPFLAGS_POST="" # Needed for VisualAge
OBJCXX_CXXFLAGS=
OBJCXX_LIBS=
case "$host_os:$compiler" in
 solaris2\.6:GCC )
   echo "GCC compiler is not supported for:  $host"
   compiler=
   ;;

 solaris*:GCC )
   # On Solaris, GCC defaults to setting _XOPEN_SOURCE (to 500) only
   # in C++ mode.  Set it for C code as well to ensure consistent
   # header behavior, taking care to match the C standard version
   # (as enforced by <sys/feature_tests.h>).
   case "$ncbi_cv_prog_c_99" in
      no) CC="$CC -D_XOPEN_SOURCE=500" ;;
      *)  CC="$CC -D_XOPEN_SOURCE=600" ;;
   esac

   STRIP="@:"
   ;;

 linux*:ICC )
   # "incomplete type is not allowed" should be an error, not a warning!
   CFLAGS="-we70 $CFLAGS"
   CXXFLAGS="-we70 $CXXFLAGS"
   case "$compiler_version" in
      1[[1-9]]?? ) MATH_LIBS=-Wl,-lm ;; # avoid static libimf in shared libs
   esac
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
   if test "$with_symbols" = "no" -a "$with_profiling" != "yes" \
      -a "$with_strip" = "yes" ; then
      LDFLAGS="-s $LDFLAGS"
   fi
   # Recent versions default to DWARF, yielding much larger executables in
   # debug configurations; insist on STABS when the choice exists, even for
   # versions that already default to it.
   case "$with_symbols:$compiler" in
      no:* | *:WorkShop5[[0-4]] ) ;;
      * )
         CFLAGS="-xdebugformat=stabs $CFLAGS"
         CXXFLAGS="-xdebugformat=stabs $CXXFLAGS"
         ;;
   esac
   CXX_FILTER="2>&1 | \$(top_srcdir)/compilers/unix/cxx_filter.WorkShop.sh"
   LINK_FILTER="$CXX_FILTER"
   AR_FILTER="$CXX_FILTER"
   serial_ws50_rtti_kludge="rtti"
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
   CXXFLAGS="$COMMON_FLAGS -ansiW -ptused -FE:eliminate_duplicate_inline_copies -FE:template_in_elf_section -no_auto_include -LANG:std -LANG:ansi-for-init-scope -woff 1460,1521,1429,1169,1209,1107,1424 $CXXFLAGS"
   : ${CXXCPP:="$CXX -E -LANG:std"}
   LDFLAGS="-LANG:std -G0 -FE:eliminate_duplicate_inline_copies -FE:template_in_elf_section $LDFLAGS"
   CPPFLAGS="-D__LONGLONG $CPPFLAGS"
   CXX_FILTER="2>&1 | \$(top_srcdir)/compilers/unix/cxx_filter.$compiler.sh"
   ;;

 cygwin*:GCC )
   with_dll="no"
   # : ${NETWORK_LIBS:="-lws2_32"}
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
   # 1506-1108 (I) The use of keyword '__attribute__' is non-portable.
   CFLAGS="-qlanglvl=extc99 -qsuppress=1506-1108 $CFLAGS"
   # 1540-1663 (W) Incorrect assignment of a restrict qualified pointer.
   CXXFLAGS="-qrtti=all -qsuppress=1540-1663 $CXXFLAGS"
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
   case `uname -m` in
      *64 ) : ${with_64=yes} ;;
      *   ) : ${with_64=no}  ;;
   esac
   # Per <http://fink.sourceforge.net/doc/porting/shared.php>, it was
   # historically necessary to build plugins as Mach-O bundles rather than
   # dynamic libraries.  That's not such a big deal on modern Mac OS X
   # versions, so the Toolkit now defaults to building only the latter.
   CONF_dll_ext=".dylib"
   if test "$with_bundles" = yes; then
      CONF_loadable_ext=".so"
   else
      CONF_loadable_ext=".dylib"
   fi
   if $CXX -v 2>&1 | grep -q Apple; then
      CFLAGS="-fpascal-strings $CFLAGS"
      CXXFLAGS="-fpascal-strings $CXXFLAGS"
      $CXX -v 2>&1 | grep -q clang  ||  CXXFLAGS="-fno-permissive $CXXFLAGS"
      # -flat_namespace is necessary for proper handling of shared libraries
      # that don't themselves link against all their dependencies.
      LDFLAGS="-flat_namespace $LDFLAGS"

      if test "$with_ncbi_public" = yes; then
         TARGET='-mmacosx-version-min=10.5'
         sdks='/Developer/SDKs/MacOSX10.6.sdk /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk'
         if test "$with_64:${with_universal-no}" != "yes:no"; then
            sdks="/Developer/SDKs/MacOSX10.5.sdk $sdks"
         fi
         for sdk in $sdks; do
            if test -d "$sdk"; then 
               TARGET="-isysroot $sdk $TARGET"
               break
            fi
         done
         CC="$CC $TARGET"
         CXX="$CXX $TARGET"
      fi
   else
      LDFLAGS="-Wl,-flat_namespace $LDFLAGS"
   fi
   if test -f /usr/include/dlfcn.h; then
      # may be able to do without /sw, so ignore it by default
      :
   elif test -f /sw/include/dlfcn.h; then
      CPPFLAGS="-I/sw/include $CPPFLAGS"
      LDFLAGS="-L/sw/lib $LDFLAGS"
   else
      AC_MSG_WARN([No dlfcn.h in /usr/include or /sw/include.])
   fi
   case "$CPPFLAGS $LIBS" in
      *sw/*) ;;
      *)
         if test -d "$orig_NCBI/safe-sw"; then
             CPPFLAGS="-I$orig_NCBI/safe-sw/include $CPPFLAGS"
             LDFLAGS="-L$orig_NCBI/safe-sw/lib $LDFLAGS"
         fi
         ;;
   esac
   : ${with_x=${with_xpm-no}}

   # CPPFLAGS="-F/System/Library/Frameworks/CoreServices.framework/Frameworks $CPPFLAGS"
   LIBS="-Wl,-framework,ApplicationServices $LIBS"

   OBJCXX_CXXFLAGS='-x objective-c++'
   OBJCXX_LIBS='-lobjc'
   ;;
esac


#### Set makefile variables for platform-specific includes
changequote(, )dnl
COMPILER=`echo $ncbi_compiler | tr '[A-Z]' '[a-z]'`
OSTYPE=`echo $host_os | sed -e 's/[0-9.]*$//; s/-gnu[^-]*$//; y/A-Z/a-z/'`
changequote([, ])dnl


#### Check if the compiler matches one of supported compilers on this platform
if test -z "$compiler" ; then
   cat <<EOF

  Compiler $CXX $compiler_ver is not fully supported.
  Consult doc/config.html\#ref_Compilers for the list of
  fully supported platforms/compilers.
EOF
   NCBI_CAUTION([Do you still want to proceed (at your own risk)?])
   echo
   compiler="UNKNOWN"
fi


#### Set compiler prefix for non-native compilers
case "$host_os:$compiler" in
   *:GCC | *:KCC | *:ICC )
      compiler_pfx="${compiler}-"
      compiler_vpfx="${compiler}${compiler_version}-"
      ;;
esac


C_LIBS=$LIBS


ARCH_CPPFLAGS=
#### architecture settings, and extra C++ LIBS
if test "${with_universal-no}" != "no" ; then
   bit64_sfx= #"Univ"
   case "$host" in
    *-*-darwin[[89]].* | *-*-darwin[[1-9]][[0-9]]* )
      case "$with_universal" in
       yes )
          case "$with_64:$host_os" in
           yes:darwin8.* )
             AC_MSG_ERROR([Unable to build 64-bit universal binaries on $host])
             ;;
           yes:* )
             ARCH_CFLAGS="-arch ppc64 -arch x86_64"
             ARCH_CPPFLAGS="-m64"
             ;;
           * )
             ARCH_CFLAGS="-arch ppc -arch i386"
             ARCH_CPPFLAGS="-m32"
             ;;
          esac
          ;;
       * )
         ARCH_CFLAGS="-arch `echo $with_universal | sed -e 's/,/ -arch /g'`"
         ARCH_CPPFLAGS="-arch `echo $with_universal | sed -e 's/,.*//'`"
         ;;
      esac
      case "$host" in
       p*pc*-*-darwin8.*)
         # Must specify -isysroot ..., but only once; anyway, the preprocessor
         # needs to see it, and naturally can't cope with multiple -arch flags.
         # The -mmacosx-version-min flag avoids link errors under OS 10.5
         # (Darwin 9), which otherwise tries to use crt1.10.5.o despite the
         # request for a sysroot lacking that file.
         SYSROOT="-isysroot /Developer/SDKs/MacOSX10.4u.sdk -mmacosx-version-min=10.4"
         #ARCH_CFLAGS="$SYSROOT $ARCH_CFLAGS"
         CC="$CC $SYSROOT"
         CXX="$CXX $SYSROOT"
         ;;
      esac
      ;;
    * ) AC_MSG_ERROR([Do not know how to build universal binaries on $host]) ;;
   esac
   AC_DEFINE(NCBI_UNIVERSAL_BUILD, 1,
             [Define to 1 if building universal (multi-architecture) binaries.])
   with_distcc=no
elif test "$with_64" = "yes" ; then
   bit64_sfx="64"
   case "$host:$compiler" in
    sparc-sun-solaris*:WorkShop5 | sparc-sun-solaris*:KCC )
      ARCH_CFLAGS="-xtarget=ultra -xarch=v9"
      ;;
    sparc-sun-solaris*:WorkShop51 )
      ARCH_CFLAGS="-xtarget=ultra -xarch=v9"
      LIBS="-Bstatic -L$CXX_PATH/../lib -lCstd -lCrun -Bdynamic $LIBS -lc"
      ;;
    *solaris*:WorkShop59 | *solaris*:WorkShop51? )
      ARCH_CFLAGS="-m64"
      LIBS="-lCstd -lCrun $LIBS -lc"
      ;;
changequote(, )dnl
    *solaris*:WorkShop5* )
      ARCH_CFLAGS="-xtarget=generic64"
      # Redundant for programs, but necessary for dlopen-able shared libs,
      # at least in the x86 ReleaseMT configuration.
      LIBS="-lCstd -lCrun $LIBS -lc"
      ;;
changequote([, ])dnl
    mips*:GCC )
      ARCH_CFLAGS="-mips64"
      ;;
    *:GCC )
      # May not work prior to GCC 3.1.
      ARCH_CFLAGS="-m64"
      case $host_os in darwin*) ARCH_CPPFLAGS="-m64" ;; esac
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
      AC_MSG_ERROR([Do not know how to compile 64-bit with compiler $CXX $compiler_ver $host:$compiler])
      ;;
   esac
else
   bit64_sfx=
   ARCH_CFLAGS=
   if test "$with_64" = "no" ; then
      case "$host:$compiler" in
       *solaris*:WorkShop* | *solaris*:KCC )
         ARCH_CFLAGS="-xtarget=generic"
         ;;
       mips*:GCC )
         ARCH_CFLAGS="-mipsn32"
         ;;
       *:GCC )
         # May not work prior to GCC 3.1.
         ARCH_CFLAGS="-m32"
         case $host_os in darwin*) ARCH_CPPFLAGS="-m32" ;; esac
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
         AC_MSG_WARN([Do not know how to compile 32-bit with compiler $CXX $compiler_ver $host:$compiler])
         ;;
      esac
   fi
   case "$host:$compiler" in
    *solaris*:WorkShop5 )
      LIBS="-Bstatic -L$CXX_PATH/../SC5.0/lib -lm -Bdynamic $LIBS"
      MATH_LIBS=" "
      ;;
    *solaris*:WorkShop51 )
      LIBS="-Bstatic -L$CXX_PATH/../lib -lCstd -lCrun -lm -Bdynamic $LIBS -lc"
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
      LIBS="-lCstd $LIBS -lCrun -lc"
      ;;
changequote(, )dnl
    *solaris*:WorkShop5* )
      # MATH_LIBS=" "
      # Redundant for programs, but necessary for dlopen-able shared libs,
      # at least in the x86 ReleaseMT configuration.
      LIBS="-lCstd $LIBS -lCrun -lc"
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

CPPFLAGS="$ARCH_CPPFLAGS $CPPFLAGS"
CFLAGS="$ARCH_CFLAGS $CFLAGS"
CXXFLAGS="$ARCH_CFLAGS $CXXFLAGS"
LDFLAGS="$ARCH_CFLAGS $LDFLAGS"

: ${MATH_LIBS:="-lm"}

### Math lib to be always linked in
LIBS="$MATH_LIBS $LIBS"
C_LIBS="$MATH_LIBS $C_LIBS"

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
    *:GCC:3[[4-9]]* | *:GCC:[[4-9]]* | *:ICC:[[91]]* ) : ${with_lfs=yes} ;;
esac

if test "$with_lfs" = "yes" ; then
   LFSFLAGS="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"
   CPPFLAGS="$LFSFLAGS $CPPFLAGS"
fi

tabchar='	'
wschars=" $tabchar"
wsrx="[[$wschars]]"

#### Flags to enable (potentially unsafe) extra optimization.
if test "$skip_fast_flags" = no -a -z "$DEF_FAST_FLAGS" ; then
   case "$compiler:$compiler_version" in
      GCC:2* | GCC:344 )
        # GCC 2.9x sometimes experiences internal errors at high optimization;
        # GCC 3.4.4 (at least on FreeBSD) meanwhile generates crashprone code.
        DEF_FAST_FLAGS="-O2"
        ;;
      GCC:* )
        DEF_FAST_FLAGS="-O3 -finline-functions -fstrict-aliasing"
        case "$host_os:$host_cpu:$compiler_version" in
           freebsd* | solaris*:*86*:* | *:4[[5-9]]? | *:[[5-9]]?? ) ;;
           * ) DEF_FAST_FLAGS="$DEF_FAST_FLAGS -ffast-math" ;;
        esac
        if test "$with_profiling" != "yes"; then
           # incompatible with -pg
           DEF_FAST_FLAGS="$DEF_FAST_FLAGS -fomit-frame-pointer"
        fi
        ;;
      WorkShop5[[89]]* | WorkShop510 )
        # Limit optimization to -xO2 (-O now maps to -xO3) due to compiler bugs.
        subst="s/[$wsrx]-x*O[[1-9]]*//g"
        CFLAGS="`  echo \" $CFLAGS\"   | sed -e \"$subst\"` -xO2"
        CXXFLAGS="`echo \" $CXXFLAGS\" | sed -e \"$subst\"` -xO2"
        LDFLAGS="` echo \" $LDFLAGS\"  | sed -e \"$subst\"` -xO2"
        DEF_FAST_FLAGS="-xO2"
        ;;
      WorkShop* )
        DEF_FAST_FLAGS="-fast"
        case "$with_ncbi_public:$host_cpu" in
           yes:sparc* )
              DEF_FAST_FLAGS="$DEF_FAST_FLAGS -xtarget=ultra"
              ;;
           *:i?86)
              case "$compiler_ver" in
                *5.[[0-4]]*)
                  # Work around a WorkShop standard library bug.
                  DEF_FAST_FLAGS="$DEF_FAST_FLAGS -D_RWSTD_NO_TEST_AND_SET"
                  ;;
              esac
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
      ICC:[[1-8]]?? )
        DEF_FAST_FLAGS="-O2"
        ;;
      ICC:9?? )
        DEF_FAST_FLAGS="-O3 -axP -ip" # optimized for Core or P4 w/SSE3 (max)
        ;;
      ICC:10?? )
        DEF_FAST_FLAGS="-O3 -axT -ip" # Core 2 w/SSSE3; axS would cover SSE 4.1
        ;;
      ICC:* )
        # Core 2; ICC 11 goes up to SSE4.2 (i7), ICC 12 up to CORE-AVX-I
        DEF_FAST_FLAGS="-O3 -axSSSE3 -ip"
        ;;
      MIPSpro* )
        # -Ofast would be preferable, but runs into resource limits when
        # building libxblast.so. :-/
        DEF_FAST_FLAGS="-O2"
        ;;
      Compaq:* )
        DEF_FAST_FLAGS="-fast"
        ;;
      * )
        DEF_FAST_FLAGS="-O"
        ;;
   esac
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
    *:GCC | linux* )
      CFLAGS_DLL="-fPIC"
      CXXFLAGS_DLL="-fPIC"
      ;;
    solaris*:WorkShop* | irix*:MIPSpro73 | \
    solaris*:KCC | irix*:KCC )
      CFLAGS_DLL="-KPIC"
      CXXFLAGS_DLL="-KPIC"
      ;;
   esac
fi

# Flag for setting the runtime library search path
if test -z "$CONF_f_runpath" ; then
   case "$host_os:$compiler" in
    solaris* )
      CONF_f_runpath="-R"
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
    darwin* )
      test "$ncbi_cv_prog_cc_wl_rpath" = no  ||  CONF_f_runpath="-Wl,-rpath,"
      ;;
   esac
fi
case "$host_os:$ncbi_cv_prog_cc_wl_rpath" in
    linux*|solaris* )
        origin="'\$\$ORIGIN'"
        RUNPATH_ORIGIN="$CONF_f_runpath$origin"
        ;;
    darwin*:yes )
        origin='@executable_path'
        RUNPATH_ORIGIN="$CONF_f_runpath$origin"
        ;;
    * )
        origin=
        RUNPATH_ORIGIN="-DNCBI_RPO_SUFFIX_EATER="
        ;;
esac

case "$compiler:$compiler_version:$with_bin_release" in
  GCC:*:yes )
    libstdcxx=`$CXX $LDFLAGS -print-file-name=libstdc++.a`
    case "$libstdcxx" in
       /*)
          LIBS="$libstdcxx $LIBS"
          LINK=$C_LINK
          ;;
       *)
          AC_MSG_WARN([Unable to find static libstdc++ requested by --with-bin-release.])
          ;;
    esac
    ;;
  GCC:* )
    # Need to specify runpath for compiler-provided libraries
    case $compiler_version in
        30*     ) major=3 ;;
        31*     ) major=4 ;;
        3[[23]]*) major=5 ;;
        *       ) major=6 ;;
    esac
    found=false
    for f in libstdc++.so.$major libgcc_s.so.1 libstdc++.a; do
        path=`$CXX $LDFLAGS -print-file-name=$f`
        dir=`dirname "$path"`
        abs_dir=`cd "$dir" && pwd`
        case "$dir:$abs_dir" in
            *:/lib | *:/usr/lib | *:/usr/lib32 | *:/usr/lib64 ) 
               # no rpath needed
               break
               ;;
            /* )
               found=true
               break
               ;;
        esac
    done
    if test "$found" = "true" -a -n "$CONF_f_runpath"; then
        LDFLAGS="$CONF_f_runpath$dir $LDFLAGS"
        case "$host_os" in
           osf*)
              # the linker doesn't merge -rpath flags...
              CONF_f_runpath="$CONF_f_runpath$dir:"
              ;;
           solaris*)
              # avoid interference from possible older versions in /usr/sfw.
              LDFLAGS="-L$dir $LDFLAGS"
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
    # In principle, these settings should work, and avoid the need to
    # depend on Intel's libraries at runtime.
    #  APP_LDFLAGS="-static-libcxa $APP_LDFLAGS"
    #  DLL_LDFLAGS="-nodefaultlibs $DLL_LDFLAGS"
    # In practice, they can break applications that use plugins. :-/
    LDFLAGS="$CONF_f_runpath$compiler_root/lib $LDFLAGS"
    ;;
  ICC:9* )
    # -i-static moved from common LDFLAGS to accommodate those few
    # DLLs (plugins for external programs such as Python) that need to
    # link ICC's libraries dynamically.
    APP_LDFLAGS="-i-static $APP_LDFLAGS"
    DLL_LDFLAGS="-i-static -nodefaultlibs $DLL_LDFLAGS"
    # Running icpc on a bunch of object files doesn't always ensure that
    # the C++ standard library is actually linked in, and we shouldn't
    # rely on help from wrapper scripts.
    LINK="$LINK -Kc++"
    ;;
  ICC:1???:* )
    APP_LDFLAGS="-static-intel $APP_LDFLAGS"
    DLL_LDFLAGS="-static-intel -nodefaultlibs $DLL_LDFLAGS"
    # Redundant for apps, but necessary for plugins to be adequately
    # self-contained, at least on 32-bit Linux.
    LIBS="$LIBS -lstdc++"
    LINK="$LINK -Kc++"
    # Defining _GCC_NEXT_LIMITS_H ensures that <limits.h> chaining doesn't
    # stop short, as can otherwise happen. :-/
    CPPFLAGS="$CPPFLAGS -D_GCC_NEXT_LIMITS_H"
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
      NCBI_CAUTION([Do you still want to try build DLLs (at your own risk)?])
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

DLL=
DLL_LIB_SETTING='$(DLL_DLIB)'
IF_WITH_DLL=
UNLESS_WITH_DLL='# '
STATIC="-static"
USUAL_AND_DLL=both
USUAL_AND_LIB=both

case "$LIB_OR_DLL" in
   dll)
       STATIC=
       USUAL_AND_DLL=dll
       ;;
   lib)
       DLL=-dll
       DLL_LIB_SETTING='$(DLL_LIB)'
       IF_WITH_DLL='# '
       UNLESS_WITH_DLL=
       USUAL_AND_LIB=lib
       ;;
esac

AC_SUBST(DLL)
AC_SUBST(DLL_LIB_SETTING)
AC_SUBST(IF_WITH_DLL)
AC_SUBST(UNLESS_WITH_DLL)
AC_SUBST(STATIC)
AC_SUBST(USUAL_AND_DLL)
AC_SUBST(USUAL_AND_LIB)


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
fi


#### Intel compiler::  common flags and definitions
if test "$compiler" = "ICC" ; then
   if test -d /usr/local/gcc; then
      case "$compiler_version:$HOST_CPU" in
         *:i?86)               v=4.4.5 ;;
         ???:* | 1[[01]]??:* ) v=4.0.1 ;;
         *)                    v=4.4.2 ;;
      esac
      gcc=/usr/local/gcc/$v/bin/gcc
      if test -x $gcc; then
         CC="$CC -gcc-name=$gcc"
         CXX="$CXX -gcc-name=$gcc"
      fi
   fi
   if test -n "$icc_license" ; then
      icc_CC="$CC"
      icc_CXX="$CXX"
      CC="./compilers/unix/cxx_filter.ICC.sh $CC"
      CXX="./compilers/unix/cxx_filter.ICC.sh $CXX"
   fi
fi


#### How to ensure that the compiler will account for potential aliasing
case "$compiler" in
   GCC ) NO_STRICT_ALIASING=-fno-strict-aliasing ;;
   ICC ) NO_STRICT_ALIASING='-falias -ffnalias' ;;
   WorkShop* ) NO_STRICT_ALIASING='-xalias_level=any' ;;
   * ) NO_STRICT_ALIASING=
esac


#### Automatic generation of dependencies for/by the "make" utility
case "$host_os" in
   solaris* )  KeepStateTarget=".KEEP_STATE:" ;;
esac

if test "$with_autodep" = "yes" ; then
   Rules="rules_with_autodep"
elif test "$with_autodep" = "no" ; then
   Rules="rules"
elif make --version 2>/dev/null | grep 'GNU Make' >/dev/null ; then
   Rules="rules_with_autodep"
else
   Rules="rules"
fi


### Support for precompiled headers
GCCPCH="#"
if test "$compiler" = GCC -a "$with_pch" = "yes" \
     -a "${with_universal-no}" = "no"; then
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
   AC_CACHE_CHECK([whether strip loses runtime type information],
      ncbi_cv_strip_loses_rtti,
      [if test "$cross_compiling" = yes; then
          case "$host_os" in
             darwin* ) ncbi_cv_strip_loses_rtti=yes ;;
             # No problems observed elsewhere so far.
             *       ) ncbi_cv_strip_loses_rtti=no  ;;
          esac
       else
          AC_LANG_CONFTEST([AC_LANG_PROGRAM(
                               [[#include <iostream>
                                 #include <typeinfo>
                                 struct SomeStruct { };]],
                               [[std::cout << typeid(SomeStruct).name();]])])
          echo $CXX $CPPFLAGS $CXXFLAGS $LDFLAGS -o conftest$ac_exeext \
             conftest.$ac_ext >&AS_MESSAGE_LOG_FD
          $CXX $CPPFLAGS $CXXFLAGS $LDFLAGS -o conftest$ac_exeext \
             conftest.$ac_ext >&AS_MESSAGE_LOG_FD 2>&1
          $STRIP conftest$ac_exeext
          if (ulimit -c 0; ./conftest$ac_exeext) >/dev/null 2>&1; then
             ncbi_cv_strip_loses_rtti=no
          else
             ncbi_cv_strip_loses_rtti=yes
          fi
          rm -rf conftest*
       fi])
   if test "$ncbi_cv_strip_loses_rtti" = "yes"; then
      NCBI_CAUTION([Do you still want to proceed (at your own risk)?])
   fi
else
   STRIP=@:
fi


# When using ccache and distcc together, ccache needs to be primary,
# so look for distcc(.sh) first.

if test -x $orig_NCBI/bin/distcc.sh ; then
   : ${DISTCC=$orig_NCBI/bin/distcc.sh}
   : ${with_distcc=yes}
fi

if test "$with_distcc" = "yes" ; then
   AC_CHECK_PROG(DISTCC, distcc, distcc, [])
   if test -n "$DISTCC" ; then
      # Test distcc reasonably thoroughly to avoid possible gotchas.
      cat > distcctest.c << EOF
#include <stddef.h>
#if !defined(__GNUC__)  &&  !defined(offsetof)
#  define offsetof(T, F) ((size_t)((char*) &(((T*) 0)->F) - (char*) 0))
#endif
struct S { int x; };
int f() { return offsetof(struct S, x); }
EOF
      DISTCC_FALLBACK=0
      export DISTCC_FALLBACK
      AC_CACHE_CHECK([whether any distcc servers are available],
         ncbi_cv_prog_distcc_servers,
         [$DISTCC $CC -c distcctest.c 2>&AS_MESSAGE_LOG_FD
          case $? in
            # 102: bind failed
            # 103: connect failed
            # 106: bad hostspec
            # 107: I/O error
            # 109: protocol error
            # 113: access denied
            # 116: no hosts defined
            102 | 103 | 106 | 107 | 109 | 113 | 116 )
             ncbi_cv_prog_distcc_servers=no ;;
            * )
             ncbi_cv_prog_distcc_servers=yes ;;
          esac])
      if test "$ncbi_cv_prog_distcc_servers" = no; then
         AS_UNSET(DISTCC)
      fi
      if test -n "$DISTCC" ; then
         AC_CACHE_CHECK([whether $DISTCC is compatible with $CC],
            ncbi_cv_prog_distcc_vs_cc,
            [if $DISTCC $CC -c distcctest.c 2>&AS_MESSAGE_LOG_FD ; then
                ncbi_cv_prog_distcc_vs_cc=yes
             else
                ncbi_cv_prog_distcc_vs_cc=no
             fi])
         if test "$ncbi_cv_prog_distcc_vs_cc" = yes; then
            CC_WRAPPER="$DISTCC $CC_WRAPPER"
         else
            AS_UNSET(DISTCC)
         fi
      fi
      if test -n "$DISTCC" ; then
         AC_CACHE_CHECK([whether $DISTCC is compatible with $CXX],
            ncbi_cv_prog_distcc_vs_cxx,
            [mv distcctest.c distcctest.cpp
             if $DISTCC $CXX -c distcctest.cpp 2>&AS_MESSAGE_LOG_FD ; then
                ncbi_cv_prog_distcc_vs_cxx=yes
             else
                ncbi_cv_prog_distcc_vs_cxx=no
             fi])
         if test "$ncbi_cv_prog_distcc_vs_cxx" = yes; then
            CXX_WRAPPER="$DISTCC $CXX_WRAPPER"
         else
            AS_UNSET(DISTCC)
         fi
      fi
      rm -f distcctest.*
   elif test "$with_distcc" = "yes" ; then
      AC_MSG_WARN([distcc explicitly requested, but not found in PATH.])
   fi
fi

if test -z "$with_ccache"  -a  -n "$DISTCC"  -a  -z "$CCACHE_DIR" ; then
   # Using CCACHE together with DISTCC causes problems when the involved DISTCC
   # servers share the same home dir, as in that case the caches from
   # different servers can mix up rather badly, corrupting each other.
   # On the other hand, most NCBI accounts have $CCACHE_DIR automatically set
   # to a non-shared, per-machine location (such as "/tmp/.ccache-username").
   with_ccache="no"
fi

if test "$with_ccache" != "no"; then
   AC_CHECK_PROG(CCACHE, ccache, ccache, [])
   if test -n "$CCACHE" ; then
      if test -n "$CC_WRAPPER"; then
         CC_WRAPPER="CCACHE_PREFIX=$CC_WRAPPER $CCACHE"
      else
         CC_WRAPPER=$CCACHE
      fi
      if test -n "$CXX_WRAPPER"; then
         CXX_WRAPPER="CCACHE_PREFIX=$CXX_WRAPPER $CCACHE"
      else
         CXX_WRAPPER=$CCACHE
      fi
   elif test "$with_ccache" = "yes"; then
      AC_MSG_WARN([ccache explicitly requested, but not found in PATH.])
   fi
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

AC_PATH_PROG(TOUCH, touch, [], /bin:/usr/bin:$PATH)
AC_PATH_PROG(GREP, grep)
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

AC_CHECK_PROG(VALGRIND_PATH, valgrind, valgrind)

AC_PATH_PROG(LDD, ldd)
if test -n "$LDD"; then
   AC_MSG_CHECKING([whether $LDD accepts -r])
   if $LDD -r /bin/ls >/dev/null 2>&1; then
      AC_MSG_RESULT(yes)
      LDD_R="$LDD -r"
   else
      AC_MSG_RESULT(no)
   fi
fi

#### Check if "${build_root}" is defined;  provide a default one
if test -n "${with_build_root}" ; then
   build_root=${with_build_root}
fi

signature_compiler=$compiler
signature_compiler_version=$compiler_version
if test "$compiler" = "GCC"; then
   llvm_version=`$CXX -v 2>&1 | $EGREP 'clang|LLVM'`
   case "$llvm_version" in
      *clang* )
         # Take the LLVM version from both "clang version X.Y (...)" and
         # "$Vendor (LLVM|clang) version ... (...) (based on LLVM X.Y)"
         sed='s/.* \([[1-9]][[0-9.]]*\).*/\1/; s/\.//; s/^..$/&0/'
         signature_compiler=Clang
         signature_compiler_version=`echo $llvm_version | sed -e "$sed"`
         ;;
      *LLVM* )
         signature_compiler=LLVMGCC
         ;;
   esac
fi

### Possibly add the version to the build directory name, but preseve
### the original default when reconfiguring.
if test -z "${with_version}" -a -n "${ac_cv_header_stdc}"; then
   with_version=${ncbi_cv_with_version:-no}
fi
if test -z "${build_root}" ; then
   build_root="${srcdir}/${signature_compiler}"
   case "$signature_compiler:$with_version" in
      *[[0-9]]:* | *:no) ;;
      *) build_root=${build_root}${signature_compiler_version} ;;
   esac
   ncbi_cv_with_version=${with_version:-yes}
else
   : ${with_suffix:="no"}
fi


#### Release/Debug
case "${with_debug}:${with_optimization}" in
   no:*)
      release_debug=Release
      DEBUG_SFX=Release
      D_SFX=
      ;;
   yes:yes | :yes)
      release_debug=DebugOpt
      DEBUG_SFX=Debug
      D_SFX=d
      ;;
   *)
      release_debug=Debug
      DEBUG_SFX=Debug
      D_SFX=d
      ;;
esac


#### Profiling; try to turn on both function weight and basic-block counting
if test "$with_profiling" = "yes" ; then
   prof_sfx="Prof"
   case "$compiler" in
      Compaq)    PFFLAGS="-pg" ;;
      GCC)       PFFLAGS="-pg" ;;
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
if test "${with_universal-no}" = "no"; then
   AC_CHECK_SIZEOF(size_t)
   ac_cv_sizeof_size_t=`echo "$ac_cv_sizeof_size_t" | tr -d '\r'`
   NCBI_PLATFORM_BITS=`expr 8 \* $ac_cv_sizeof_size_t`
   AC_DEFINE_UNQUOTED(NCBI_PLATFORM_BITS, $NCBI_PLATFORM_BITS,
                      [Define to the architecture size.])
   if test $NCBI_PLATFORM_BITS -eq 64; then
      bit64_sfx=64
   fi
fi

if test "$bit64_sfx" = 64 -o "$with_lfs" = "yes"; then
   NCBI_FEATURE(LFS)
fi


### Intel Thread Checker
if test "${with_tcheck=no}" != "no" ; then
   if test "${release_debug}${mt_sfx}" != "DebugMT" ; then
      AC_MSG_WARN([--with-tcheck is most likely to be effective in DebugMT builds.])
   fi
   if test "$compiler" = "ICC"; then
      case "$bit64_sfx" in
        64) tcbin=bin/32e ;;
        * ) tcbin=bin/32  ;;
      esac
      if test "$with_tcheck" != yes; then
         TCHECK_BIN=$with_tcheck/$tcbin
      fi
      AC_PATH_PROG(TCHECK_CL, tcheck_cl, [], $TCHECK_BIN:$PATH:$TCHECK_FALLBACK)
      if test -n "$TCHECK_CL"; then
         CFLAGS="$CFLAGS -tcheck -O0"
         CXXFLAGS="$CXXFLAGS -tcheck -O0"
         LDFLAGS="$LDFLAGS -tcheck -O0 -L`dirname $TCHECK_CL`"
      fi
   fi
   CPPFLAGS="$CPPFLAGS -DNCBI_TCHECK"
fi


#### Add "Release"/"Debug", "MT", "DLL", "64", "_p" suffixes to "${build_root}"
if test "$with_suffix" != "no" ; then
   build_root="${build_root}-${release_debug}${mt_sfx}${dll_sfx}${bit64_sfx}${prof_sfx}"
fi


#### Add full host specs to "${build_root}"
if test "$with_hostspec" = "yes" ; then
   build_root="${build_root}--${host}"
fi

if test -n "$with_build_root_sfx" ; then
   build_root=$build_root$with_build_root_sfx
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
signature_hostname=`(hostname || uname -n) 2>/dev/null \
    | sed 's/\..*//; s/-/_/g; 1q'`
signature="${signature_compiler}_${signature_compiler_version}-${release_debug}${mt_sfx}${dll_sfx}${bit64_sfx}${prof_sfx}--${verbose_host}-${signature_hostname}"
AC_DEFINE_UNQUOTED(NCBI_SIGNATURE, "$signature",
   [Build signature: compiler-name '_' compiler-version '-' configuration
   '--' platform-name '-' hostname])


#### Check if there is "${build_root}" dir
test -d "${build_root}"  ||  mkdir -p "${build_root}"
test -d "${build_root}"  ||  AC_MSG_ERROR([Couldn't create ${build_root}])

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
         NCBI_FIX_DIR(build_root)
      fi
   ;;
   *  ) NCBI_FIX_DIR(build_root) ;;
esac
if test -n "$with_fake_root"; then
   build_root=$with_fake_root/`basename $build_root`
fi


#### Set runpath (may depend on build root)
if test -n "$with_relative_runpath"; then
   if test "$with_runpath" = "no"; then
      AC_MSG_ERROR([incompatible options: --without-runpath and --with-relative-runpath=$with_relative_runpath])
   elif test -z "$origin"; then
      AC_MSG_ERROR([Do not know how to specify relative runpaths on $host])
   else
      rp=`echo "$origin/$with_relative_runpath" | \
          sed "s,:,:$origin/,g; s,/.:,:,g; s,/.\$,,"`
      with_runpath="$rp${with_runpath+:$with_runpath}"
   fi
fi

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
   pure_runpath=`echo "${with_runpath}" | sed -e "s/{}/$root_base/g"`
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
ALLOW_UNDEF=
FORBID_UNDEF=
case "$compiler" in
 GCC | ICC )
   LINK_DLL="$LINK $ARCH_CFLAGS -shared -o" # possibly overridden below
   case "$host_os" in
      solaris* )
        # See http://realm.progsoc.uts.edu.au/~subtle/solaris28_link.html
        LINK_DLL="$LINK $ARCH_CFLAGS -shared -mimpure-text -o"
        FORBID_UNDEF="-Wl,-z,defs"
        ;;
      darwin* )
        # Per <http://fink.sourceforge.net/doc/porting/shared.php>, it was
        # historically necessary to build plugins as Mach-O bundles rather than
        # dynamic libraries.  That's not such a big deal on modern Mac OS X
        # versions, so the Toolkit now defaults to building only the latter.
        if test "$with_bundles" = yes; then
           has_dll_loadable=""
           AC_DEFINE(NCBI_USE_BUNDLES, 1,
              [Define to 1 if building plugins as bundles, as Mac OS X
               traditionally required.])
        fi
        if $CXX -v 2>&1 | egrep -q 'Apple|clang'; then
           LINK_DLL="$LINK $ARCH_CFLAGS -dynamiclib -install_name ${pure_runpath%%:*}/\$(XDLL) -o"
           LINK_LOADABLE="$LINK $ARCH_CFLAGS -bundle -o"
           ALLOW_UNDEF='-undefined suppress'
           FORBID_UNDEF='-undefined error'
        else
           LINK_DLL="$CC $ARCH_CFLAGS -nostartfiles -Wl,-dynamic -Wl,-dylib -Wl,-install_name,${pure_runpath%%:*}/\$(XDLL) -o"
           LINK_LOADABLE="$CC $ARCH_CFLAGS -nostartfiles -Wl,-dynamic -Wl,-bundle -Wl,-read_only_relocs,warning -lbundle1.o -o"
           ALLOW_UNDEF='-Wl,-undefined,suppress'
           FORBID_UNDEF='-Wl,-undefined,error'
        fi
        ;;
      linux* | *gnu* ) # | *bsd*
        case "$compiler:$compiler_version" in
           ICC:9* | ICC:1??? )
             ncbi_cv_prog_cc_wl_no_asu=no
             LIBS="$LIBS -lc"
             C_LIBS="$C_LIBS -lc"
             ;;
           ICC:* )
             ;;
           * )
             FORBID_UNDEF='-Wl,--no-undefined'
             ;;
        esac
        ;;
      irix* )
        FORBID_UNDEF='-Wl,-no_unresolved'
        ;;
      osf* )
        ALLOW_UNDEF='-Wl,-expect_unresolved,\*'
        FORBID_UNDEF='-Wl,-error_unresolved'
        ;;
   esac
   AC_CACHE_CHECK([whether $CC supports -Wl,--no-allow-shlib-undefined],
      ncbi_cv_prog_cc_wl_no_asu,
      [orig_LDFLAGS=$LDFLAGS
       LDFLAGS="$CXXFLAGS_DLL -shared -Wl,--no-allow-shlib-undefined $LDFLAGS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM([],[])],
          [ncbi_cv_prog_cc_wl_no_asu=yes], [ncbi_cv_prog_cc_wl_no_asu=no])
       LDFLAGS=$orig_LDFLAGS])
   if test "$ncbi_cv_prog_cc_wl_no_asu" = yes; then
      FORBID_UNDEF="$FORBID_UNDEF -Wl,--no-allow-shlib-undefined"
   fi
  ;;
 MIPSpro73 )
   LINK_DLL="$LINK $ARCH_CFLAGS -shared -o"
   FORBID_UNDEF="-no_unresolved"
   ;;
 Compaq )
   LINK_DLL="$LINK $ARCH_CFLAGS -shared -o"
   ALLOW_UNDEF='-expect_unresolved \*'
   FORBID_UNDEF='-error_unresolved'
   ;;
 WorkShop* )
   LINK_DLL="$LINK $ARCH_CFLAGS $MT_FLAG -G -h \$(XDLL) -o"
   FORBID_UNDEF="-z defs"
   ;;
 KCC )
   LINK_DLL="$LINK $ARCH_CFLAGS $MT_FLAG --soname \$(XDLL) -o"
   FORBID_UNDEF="-z defs"
   ;;
 VisualAge )
   # NOTE: --without-dll still recommended, as the linker doesn't seem
   # to support building shared libraries with undefined symbols.
   LINK_DLL="$LINK $ARCH_CFLAGS $MT_FLAG -qmkshrobj -o"
   # ALLOW_UNDEF="???"
   ;;
 * )
   if test "$with_dll" = "yes"  &&  test -z "$LINK_DLL" ; then
      AC_MSG_ERROR([Do not know how to build DLLs with compiler $CXX $compiler_ver])
   elif test -z "$LINK_DLL"; then
      with_dll=no
   fi
   ;;
esac

if test "$with_dll" = "no"; then # no DLLs whatsoever!
   DLL=
   FORCE_STATIC_LIB="LIB_OR_DLL = lib"
   APP_LIB_SETTING='$(STATIC_LIB)'
   APP_LIBS_SETTING='$(STATIC_LIBS)'
else
   FORCE_STATIC_LIB=
   APP_LIB_SETTING='$(LIB)'
   APP_LIBS_SETTING='$(LIBS)'
   NCBI_FEATURE(DLL)
   # Some shared libraries end up containing static libraries, so make
   # sure to use DLL flags for both, as such inclusion can otherwise
   # result in linker errors.
   DLL_LDFLAGS="$DLL_LDFLAGS $CXXFLAGS_DLL"
   CFLAGS="$CFLAGS $CFLAGS_DLL"
   CFLAGS_DLL=
   CXXFLAGS="$CXXFLAGS $CXXFLAGS_DLL"
   CXXFLAGS_DLL=
   AC_DEFINE(NCBI_DLL_SUPPORT, 1,
     [Define to 1 if building dynamic libraries at all
      (albeit not necessarily by default).])
   if test "$with_dll" = "yes"; then
      NCBI_FEATURE(DLL_BUILD)
      AC_DEFINE(NCBI_DLL_BUILD, 1,
                [Define to 1 if building dynamic libraries by default.])
      : ${with_plugin_auto_load=yes}
   fi
fi

if test "$with_plugin_auto_load" = yes; then
   AC_DEFINE(NCBI_PLUGIN_AUTO_LOAD, 1,
             [Define to 1 if the plugin manager should load DLLs by default.])
fi

# Make sure we have commands for both C and C++ preprocessors
AC_PROG_CPP
dnl AC_PROG_CXXCPP


#### Make "srcdir" absolute
case "${srcdir}" in
   /* ) ;;
   . )
      CDPATH=
      if test "${PWD}" != "" && test "`(cd ${PWD} ; sh -c pwd)`" = "`pwd`"  ;
      then
        srcdir="$PWD"
      else
        NCBI_FIX_DIR(srcdir)
      fi
   ;;
   * ) NCBI_FIX_DIR(srcdir) ;;
esac
real_srcdir=$srcdir
if test -n "$with_fake_root"; then
   srcdir=$build_root/..
fi


#### Ensure that all explicitly requested subtrees actually exist
NCBI_CHECK_SUBTREE(algo)
dnl NCBI_CHECK_SUBTREE(app)
if test "$with_app" = "no" ; then
   NoConfProjects="$NoConfProjects app"
fi

if test ! -f ${real_srcdir}/src/app/Makefile.in ; then
   if test "${with_app-no}" != "no" ; then
      AC_MSG_ERROR([--with-app:  APP sources are missing])
   fi
   with_app="no"
fi

dnl NCBI_CHECK_SUBTREE(bdb) # deferred until end, as --with-bdb does double duty
dnl NCBI_CHECK_SUBTREE(cgi) # --without-cgi deliberately unavailable for now
if test -f ${real_srcdir}/src/cgi/Makefile.in  -a \
        -d ${real_srcdir}/include/cgi; then
   NCBI_PROJECT(cgi)
else
   AC_MSG_WARN([CGI tree missing; some projects may fail to build.])
fi

NCBI_CHECK_SUBTREE(ctools)
NCBI_CHECK_SUBTREE(dbapi)
NCBI_CHECK_SUBTREE(gui)
NCBI_CHECK_SUBTREE(objects)
NCBI_CHECK_SUBTREE(serial)


#### Source, include, and build directories
ncbiconf="$build_root/inc/ncbiconf_unix.h"
ncbiconf_old="$build_root/inc/ncbiconf.h"
if test -f "$ncbiconf_old" -a ! -f "$ncbiconf"; then
   mv "$ncbiconf_old" "$ncbiconf"
fi
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
   NCBI_CAUTION([Do you want to re-configure in \"${builddir}\"?])
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
      MAC_BINCOPY="\$(top_srcdir)/scripts/common/impl/ln_mac.sh"
   fi
fi
ifdiff="${CONFIG_SHELL} \$(top_srcdir)/scripts/common/impl/if_diff.sh"
if test "$OSTYPE" = "darwin" -a -x /Developer/Tools/CpMac; then
   BINCOPY="$ifdiff \"$MAC_BINCOPY\""
else
   BINCOPY="$ifdiff \"$BINCOPY\""
fi


#### Manage the debug-vs.-optimization flags ("--with(out)-(max-)debug",
#### "--with(out)-symbols", "--with(out)-optimization")
if test "$compiler" != "MSVC" ; then
   if test "$with_debug" = "no" -o "$with_tcheck" != "no"; then
      CPPFLAGS="-DNDEBUG $CPPFLAGS"
   else
      CPPFLAGS="-D_DEBUG $CPPFLAGS"
   fi

   if test "$with_max_debug" = "yes"; then
      case "$compiler" in
         GCC )
            CPPFLAGS="-D_GLIBCXX_DEBUG $CPPFLAGS"
            CFLAGS="-fstack-check $CFLAGS"
            CXXFLAGS="-fstack-check $CXXFLAGS"
            ;;
         ICC ) 
            CPPFLAGS="-D_GLIBCXX_DEBUG $CPPFLAGS"
            CFLAGS="-debug all -fstack-security-check -ftrapuv $CFLAGS"
            CXXFLAGS="-debug all -fstack-security-check -ftrapuv $CXXFLAGS"
            ;;
         WorkShop* )
            CFLAGS="-xcheck=%all $CFLAGS"
            CXXFLAGS="-xcheck=%all $CXXFLAGS"
            ;;
      esac
      NCBI_FEATURE(MaxDebug)
   fi
changequote(, )dnl
   dbgrx1="$wsrx-g[^cx$wschars]*$wsrx"
   dbgrx2="$wsrx-gx*coff[0-9+]*$wsrx"
   optrx="$wsrx-x*O[0-9s]*$wsrx"
   NDEB_CFLAGS=`  echo " $CFLAGS "   | sed "s/$dbgrx1/ /g; s/$dbgrx2/ /g"`
   NDEB_CXXFLAGS=`echo " $CXXFLAGS " | sed "s/$dbgrx1/ /g; s/$dbgrx2/ /g"`
   NDEB_LDFLAGS=` echo " $LDFLAGS "  | sed "s/$dbgrx1/ /g; s/$dbgrx2/ /g"`
   if test "$with_symbols" = "no" ; then
      CFLAGS=$NDEB_CFLAGS
      CXXFLAGS=$NDEB_CXXFLAGS
      LDFLAGS=$NDEB_LDFLAGS
   else
      echo " $CFLAGS "  | $EGREP_Q "$dbgrx1|$dbgrx2" || CFLAGS="$CFLAGS -g"
      echo " $CXXFLAGS "| $EGREP_Q "$dbgrx1|$dbgrx2" || CXXFLAGS="$CXXFLAGS -g" 
      echo " $LDFLAGS " | $EGREP_Q "$dbgrx1|$dbgrx2" || LDFLAGS="$LDFLAGS -g"
      STRIP="@:"
      case "$compiler:$compiler_version" in
         GCC:4[89]? | GCC:[5-9]? | GCC:???? )
            # GCC 4.8 defaults to DWARF 4, which Totalview for one
            # can't handle; roll back to version 3 by default.
            echo " $USER_CFLAGS " | $EGREP_Q "$dbgrx1|$dbgrx2" || \
               CFLAGS="$NDEB_CFLAGS -gdwarf-3"
            echo " $USER_CXXFLAGS " | $EGREP_Q "$dbgrx1|$dbgrx2" || \
               CXXFLAGS="$NDEB_CXXFLAGS -gdwarf-3"
            echo " $USER_LDFLAGS " | $EGREP_Q "$dbgrx1|$dbgrx2" || \
               LDFLAGS="$NDEB_LDFLAGS -gdwarf-3"
            ;;
      esac
   fi

   NOPT_CFLAGS=`  echo " $CFLAGS "    | sed "s/$optrx/ /g"`
   NOPT_CXXFLAGS=`echo " $CXXFLAGS "  | sed "s/$optrx/ /g"`
   NOPT_LDFLAGS=` echo " $LDFLAGS "   | sed "s/$optrx/ /g"`
   if test "${with_tcheck-no}" != "no"; then
      # Suppress warnings when building with ICC.
      NOPT_CFLAGS="$NOPT_CFLAGS -O0"
      NOPT_CXXFLAGS="$NOPT_CXXFLAGS -O0"
      NOPT_LDFLAGS="$NOPT_LDFLAGS -O0"
   fi
   if test "$with_optimization" = "no" ; then
      CFLAGS="$NOPT_CFLAGS"
      CXXFLAGS="$NOPT_CXXFLAGS"
      LDFLAGS="$NOPT_LDFLAGS"
      FAST_CFLAGS="$CFLAGS"
      FAST_CXXFLAGS="$CXXFLAGS"
      FAST_LDFLAGS="$LDFLAGS"
   else
      if echo " $CFLAGS "   | grep -v "$optrx" >/dev/null ; then
         CFLAGS="$CFLAGS -O" ; fi
      if echo " $CXXFLAGS " | grep -v "$optrx" >/dev/null ; then
         CXXFLAGS="$CXXFLAGS -O" ; fi
      if echo " $LDFLAGS "  | grep -v "$optrx" >/dev/null ; then
         LDFLAGS="$LDFLAGS -O" ; fi
      if test "$skip_fast_flags" = yes ; then
         FAST_CFLAGS="$CFLAGS"
         FAST_CXXFLAGS="$CXXFLAGS"
         FAST_LDFLAGS="$LDFLAGS"
      else
         FAST_CFLAGS="$NOPT_CFLAGS $FAST_CFLAGS"
         FAST_CXXFLAGS="$NOPT_CXXFLAGS $FAST_CXXFLAGS"
         FAST_LDFLAGS="$NOPT_LDFLAGS $FAST_LDFLAGS"
      fi
   fi
changequote([, ])dnl
fi



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
   NCBI_FEATURE(MSWin)
elif test "$x_have_unistd" = "yes" ; then
   AC_DEFINE(NCBI_OS_UNIX, 1,  [Define to 1 on Unix.])
   AC_DEFINE(NCBI_OS, "UNIX",  [Operating system name])

   UNIX_SRC='$(UNIX_SRC)'
   UNIX_USR_PROJ='$(UNIX_USR_PROJ)'
   NCBI_FEATURE(unix)
   NCBI_FEATURE(WinMain) # to accommodate a PTB-specific (ab)use of REQUIRES

   # Extra defines for variants we care about
   case "$OSTYPE" in
     aix)
        AC_DEFINE(NCBI_OS_AIX,     1, [Define to 1 on AIX.])
        NCBI_FEATURE(AIX)
        ;;
     *bsd)
        AC_DEFINE(NCBI_OS_BSD,     1, [Define to 1 on *BSD.])
        NCBI_FEATURE(BSD)
        ;;
     cygwin)
        AC_DEFINE(NCBI_OS_CYGWIN,  1, [Define to 1 on Cygwin.])
        NCBI_FEATURE(Cygwin)
        if test "$with_mt" = yes; then NCBI_FEATURE(CygwinMT); fi
        ;;
     darwin)
        AC_DEFINE(NCBI_OS_DARWIN,  1, [Define to 1 on Mac OS X.])
        NCBI_FEATURE(Darwin)
        if false; then NCBI_FEATURE(XCODE); fi
        ;;
     irix)
        AC_DEFINE(NCBI_OS_IRIX,    1, [Define to 1 on IRIX.])
        NCBI_FEATURE(IRIX)
        ;;
     linux)
        AC_DEFINE(NCBI_OS_LINUX,   1, [Define to 1 on Linux.])
        NCBI_FEATURE(Linux)
        ;;
     osf)
        AC_DEFINE(NCBI_OS_OSF1,    1, [Define to 1 on Tru64 Unix.])
        NCBI_FEATURE(OSF)
        ;;
     solaris)
        AC_DEFINE(NCBI_OS_SOLARIS, 1, [Define to 1 on Solaris.])
        NCBI_FEATURE(Solaris)
        ;;
   esac
else
dnl Classic Mac OS, no longer supported; the following line is
dnl commented out to avoid cluttering config.h.in.
dnl AC_DEFINE(NCBI_OS_MAC, 1, [Define to 1 on MacOS.])
   AC_DEFINE(NCBI_OS, "MAC", [Operating system name])
   NCBI_FEATURE(MacOS)
fi

case "$host_os" in
   cygwin* | osf* )
      AC_DEFINE(NETDB_REENTRANT, 1,
                [Define to 1 if the BSD-style netdb interface is reentrant.])
      ;;
esac

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
AC_CHECK_HEADERS(atomic.h dlfcn.h ieeefp.h poll.h sys/epoll.h sys/mount.h \
                 sys/sockio.h sys/statvfs.h sys/sysinfo.h sys/vfs.h)
case "$host_os" in
   cygwin*) AC_CHECK_HEADERS(winternl.h) ;;
esac
# for FreeTDS 0.6x
AC_CHECK_HEADERS(arpa/inet.h errno.h malloc.h netdb.h \
                 netinet/in.h netinet/tcp.h paths.h signal.h \
                 sys/ioctl.h sys/select.h sys/socket.h sys/time.h wchar.h)
AC_HEADER_TIME

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

## getpwuid_r() -- 2 major variants: 4-arg, 5-arg (POSIX).
## (Alternate 4-arg interface ignored due to rarity.)

AC_CACHE_CHECK([for getpwuid_r], ncbi_cv_func_getpwuid_r,
   [ncbi_cv_func_getpwuid_r="no"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <unistd.h>
         #include <pwd.h>]],
       [[struct passwd bpw;
         char buf[1024];
         char *dir = getpwuid_r(getuid(), &bpw, buf, sizeof(buf))->pw_dir;
       ]])],
       [ncbi_cv_func_getpwuid_r=4])
    if test "$ncbi_cv_func_getpwuid_r" = "no" ; then
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <unistd.h>
         #include <pwd.h>]],
       [[struct passwd *pw, bpw;
         char buf[1024];
         getpwuid_r(getuid(), &bpw, buf, sizeof(buf), &pw);
       ]])],
       [ncbi_cv_func_getpwuid_r=5])
    fi])

if test "$ncbi_cv_func_getpwuid_r" != "no" ; then
   AC_DEFINE_UNQUOTED(NCBI_HAVE_GETPWUID_R, $ncbi_cv_func_getpwuid_r,
    [If you have the `getpwuid_r' function, define to the number of
     arguments it takes (normally 4 or 5).])
fi

# Perform a full-fledged check in C++ to be sure of an actual declaration.
# Accommodate the old Solaris interface that returns char* rather than int. :-/
AC_CACHE_CHECK([for getlogin_r], ncbi_cv_func_getlogin_r,
   [ncbi_cv_func_getlogin_r="no"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#include <unistd.h>]],
       [[char buf[64]; /* return */ getlogin_r(buf, 64);]])],
       [ncbi_cv_func_getlogin_r=yes], [ncbi_cv_func_getlogin_r=no])])
if test $ncbi_cv_func_getlogin_r = yes; then
   AC_DEFINE(HAVE_GETLOGIN_R, 1,
      [Define to 1 if you have the `getlogin_r' function])
fi

AC_LANG_PUSH(C)
# some of these can lose in C++ due to the way autoconf works
AC_CHECK_FUNCS(localtime_r pthread_setconcurrency pthread_atfork)
if test "$ac_cv_func_localtime_r" = "yes" ; then
   AC_CACHE_CHECK(return type of localtime_r, ac_cv_func_which_localtime_r,
      [AC_COMPILE_IFELSE(
          [AC_LANG_PROGRAM(
             [[#include <unistd.h>
               #include <time.h>]],
             [[struct tm mytm;
               time_t t;
               int y = localtime_r(&t, &mytm)->tm_year;]])],
          ac_cv_func_which_localtime_r=struct,
          ac_cv_func_which_localtime_r=int)])
   if test $ac_cv_func_which_localtime_r = struct; then
      AC_DEFINE(HAVE_FUNC_LOCALTIME_R_TM, 1,
         [Define to 1 if your localtime_r returns a struct tm*.])
   else
     AC_DEFINE(HAVE_FUNC_LOCALTIME_R_INT, 1,
        [Define to 1 if your localtime_r returns a int.])
   fi
fi

AC_CACHE_CHECK([whether pthread mutexes are available],
   ncbi_cv_func_pthread_mutex,
   [AC_LINK_IFELSE([AC_LANG_PROGRAM(
       [[#include <pthread.h>
         pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;]],
       [[pthread_mutex_lock(&mutex); pthread_mutex_unlock(&mutex);]])],
      [ncbi_cv_func_pthread_mutex=yes], [ncbi_cv_func_pthread_mutex=no])])
if test $ncbi_cv_func_pthread_mutex=yes; then
   AC_DEFINE(HAVE_PTHREAD_MUTEX, 1,
      [Define to 1 if pthread mutexes are available.])
fi


### Check for other standard library functions

AC_FUNC_VPRINTF
AC_CHECK_FUNCS(euidaccess atoll basename lchown fseeko getgrouplist \
               getpagesize getpass getpassphrase getpwuid getrusage \
               gettimeofday getuid memrchr readpassphrase readv usleep \
               asprintf vasprintf vsnprintf select statfs statvfs \
               strcasecmp strlcat strlcpy strdup strndup strtok_r \
               sysmp timegm utimes lutimes writev)
AC_LANG_POP(C)

AC_TYPE_SIGNAL
test "$ac_cv_func_select" = yes  ||  ncbi_cv_func_select_updates_timeout=no
AC_CACHE_CHECK([whether select updates the timeout when interrupted],
               ncbi_cv_func_select_updates_timeout,
   [AC_RUN_IFELSE([AC_LANG_PROGRAM(
       [[#include <signal.h>
         #include <sys/types.h>
         #ifdef HAVE_SYS_SELECT_H
         #  include <sys/select.h>
         #endif
         #ifdef HAVE_SYS_TIME_H
         #  include <sys/time.h>
         #endif
         #ifdef HAVE_UNISTD_H
         #  include <unistd.h>
         #endif
         RETSIGTYPE handler(int sig) { alarm(0); }]],
       [[struct timeval timeout = { 5, 0 };
         signal(SIGALRM, &handler);
         alarm(1);
         select(0, NULL, NULL, NULL, &timeout);
         return timeout.tv_sec == 5;]])],
       [ncbi_cv_func_select_updates_timeout=yes],
       [ncbi_cv_func_select_updates_timeout=no],
       [case "$host_os" in
          linux) ncbi_cv_func_select_updates_timeout=yes ;;
          *) ncbi_cv_func_select_updates_timeout=no ;;
        esac])])
if test "$ncbi_cv_func_select_updates_timeout" = yes; then
   AC_DEFINE(SELECT_UPDATES_TIMEOUT, 1,
      [Define to 1 if the `select' function updates its timeout when
       interrupted by a signal.])
fi

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
   AC_DEFINE_UNQUOTED(NCBI_HAVE_READDIR_R, $ncbi_cv_func_readdir_r,
    [If you have the `readdir_r' function, define to the number of
     arguments it takes (normally 2 or 3).])
fi

### Check for typedefs, structures, and compiler features.
AC_C_CONST

### Check for C standard types and sizes
if test "${with_universal-no}" = "no"; then
   AC_C_BIGENDIAN
   AC_C_CHAR_UNSIGNED
   AC_CHECK_SIZEOF(char)
   AC_CHECK_SIZEOF(double)
   AC_CHECK_SIZEOF(float)
   AC_CHECK_SIZEOF(int)
   AC_CHECK_SIZEOF(long)
   AC_CHECK_SIZEOF(long double)
   AC_CHECK_SIZEOF(long long)
   AC_CHECK_SIZEOF(short)
   AC_CHECK_SIZEOF(void*)
   AC_CHECK_SIZEOF(__int64)
fi
AC_CHECK_TYPES([intptr_t, uintptr_t])

AC_CHECK_MEMBER(struct sockaddr_in.sin_len,
   [AC_DEFINE(HAVE_SIN_LEN, 1,
      [Define to 1 if `sin_len' is a member of `struct sockaddr_in'.])],
   [], [#include <sys/types.h>
        #include <netinet/in.h>])

AC_CHECK_MEMBERS([struct tm.tm_zone, struct tm.__tm_zone], [], [],
   [#include <time.h>])


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

AC_LANG_PUSH(C)

AC_CACHE_CHECK([whether the preprocessor supports C99-style variadic macros],
   ncbi_cv_cpp_std_varargs,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#define foo(...) 1
         #if foo(x,y,z) != 1
         #error failed
         #endif]], [])],
       [ncbi_cv_cpp_std_varargs=yes], [ncbi_cv_cpp_std_varargs=no])])
if test "$ncbi_cv_cpp_std_varargs" = yes; then
   AC_DEFINE(HAVE_CPP_STD_VARARGS, 1,
      [Define to 1 if the preprocessor supports C99-style variadic macros.])
fi

AC_CACHE_CHECK([whether the preprocessor supports GNU-style variadic macros],
   ncbi_cv_cpp_gnu_varargs,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[#define foo(args...) 1
         #if foo(x,y,z) != 1
         #error failed
         #endif]], [])],
       [ncbi_cv_cpp_gnu_varargs=yes], [ncbi_cv_cpp_gnu_varargs=no])])
if test "$ncbi_cv_cpp_gnu_varargs" = yes; then
   AC_DEFINE(HAVE_CPP_GNU_VARARGS, 1,
      [Define to 1 if the preprocessor supports GNU-style variadic macros.])
fi

AC_LANG_POP(C)

# Avoid using unsupported (and noisily ignored) attributes.
case "$compiler:$compiler_version" in
   GCC:2* | GCC:3[[01]]* )
      ncbi_cv_c_deprecation_syntax=none
      ncbi_cv_c_forceinline=inline
      ;;
   VisualAge:* )
      ncbi_cv_c_forceinline=inline
      ;;
   WorkShop* )
      ncbi_cv_c_deprecation_syntax=none
      ncbi_cv_c_forceinline=inline
      ncbi_cv_c_noreturn=none
      ;;
esac

AC_CACHE_CHECK([syntax for marking deprecated functions],
   ncbi_cv_c_deprecation_syntax,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[void f(void) __attribute__((__deprecated__)); void f(void) { }]],
       [[f();]])],
      [ncbi_cv_c_deprecation_syntax='__attribute__((__deprecated__))'],
      [ncbi_cv_c_deprecation_syntax=none])])
depr=$ncbi_cv_c_deprecation_syntax
test "$depr" = "none"  &&  depr=""
AC_DEFINE_UNQUOTED(NCBI_DEPRECATED, $depr,
   [Define to whatever syntax, if any, your compiler supports for marking
    functions as deprecated.])

AC_LANG_PUSH(C)
AC_CACHE_CHECK([how to force inlining],
   ncbi_cv_c_forceinline,
   [ncbi_cv_c_forceinline=no
    for fi in __always_inline "inline __attribute__((always_inline))" \
       forceinline __forceinline__ __forceinline inline __inline__; do
       test "$ncbi_cv_c_forceinline" = "no" || break
       AC_COMPILE_IFELSE([AC_LANG_SOURCE([$fi int f(void) { return 0; }])],
          [ncbi_cv_c_forceinline=$fi], [])
    done])
forceinline=$ncbi_cv_c_forceinline
test "$forceinline" = "no"  &&  forceinline=""
AC_DEFINE_UNQUOTED(NCBI_FORCEINLINE, $forceinline,
   [Define to whatever syntax your compiler supports for marking functions
    as to be inlined even if they might not otherwise be.])

AC_CACHE_CHECK([whether the C compiler supports __attribute__((destructor))],
   ncbi_cv_c_attribute_destructor,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[void f(void) __attribute__((destructor)); void f(void) { }]],
       [[f();]])],
      [ncbi_cv_c_attribute_destructor=yes],
      [ncbi_cv_c_attribute_destructor=no])])
if test $ncbi_cv_c_attribute_destructor = yes; then
   AC_DEFINE(HAVE_ATTRIBUTE_DESTRUCTOR, 1,
      [Define to 1 if your C compiler supports __attribute__((destructor))])
fi

case "$compiler:$compiler_version $CFLAGS $CXXFLAGS" in
   GCC:[[4-9]]??\ *\ -fvisibility*|ICC:*\ -fvisibility* ) ;;
   * ) ncbi_cv_c_attribute_visibility_default=unnecessary ;;
esac

AC_CACHE_CHECK([whether $CC supports __attribute__((visibility("default")))],
   ncbi_cv_c_attribute_visibility_default,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[__attribute__((visibility("default"))) void f(void); void f(void) {}]],
       [[f();]])],
      [ncbi_cv_c_attribute_visibility_default=yes],
      [ncbi_cv_c_attribute_visibility_default=no])])
if test $ncbi_cv_c_attribute_visibility_default = yes; then
   AC_DEFINE(HAVE_ATTRIBUTE_VISIBILITY_DEFAULT, 1,
      [Define to 1 if your compiler supports __attribute__((visibility("default")))])
fi

AC_CACHE_CHECK([whether the C compiler supports C99 restrict],
   ncbi_cv_c_restrict,
   [ncbi_cv_c_restrict=no
    for restrict in __restrict__ __restrict restrict; do
       test "$ncbi_cv_c_restrict" = "no" || break
       AC_COMPILE_IFELSE([AC_LANG_SOURCE([void f(int * $restrict p);])],
          [ncbi_cv_c_restrict=$restrict], [])
    done])
restrict=$ncbi_cv_c_restrict
test "$restrict" = "no"  &&  restrict=""
AC_DEFINE_UNQUOTED(NCBI_RESTRICT_C, $restrict,
   [Define to whatever syntax, if any, your C compiler supports for marking
    pointers as restricted in the C99 sense.])
if test -n "$restrict"; then
   AC_DEFINE(HAVE_RESTRICT_C, 1,
      [Define to 1 if your C compiler supports some variant of the C99
       `restrict' keyword.])
fi
AC_LANG_POP(C)

AC_CACHE_CHECK([whether the C++ compiler supports C99 restrict],
   ncbi_cv_cxx_restrict,
   [ncbi_cv_cxx_restrict=no
    for restrict in __restrict__ __restrict restrict; do
       test "$ncbi_cv_cxx_restrict" = "no" || break
       AC_COMPILE_IFELSE([AC_LANG_SOURCE([void f(int * $restrict p);])],
          [ncbi_cv_cxx_restrict=$restrict], [])
    done])
restrict=$ncbi_cv_cxx_restrict
test "$restrict" = "no"  &&  restrict=""
AC_DEFINE_UNQUOTED(NCBI_RESTRICT_CXX, $restrict,
   [Define to whatever syntax, if any, your C++ compiler supports for marking
    pointers as restricted in the C99 sense.])
if test -n "$restrict"; then
   AC_DEFINE(HAVE_RESTRICT_CXX, 1,
      [Define to 1 if your C++ compiler supports some variant of the C99
       `restrict' keyword.])
fi

AC_CACHE_CHECK([syntax for marking functions that never return],
   ncbi_cv_c_noreturn,
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[void f(void) __attribute__((__noreturn__)); void f(void) { }]],
       [[f();]])],
      [ncbi_cv_c_noreturn='__attribute__((__noreturn__))'],
      [ncbi_cv_c_noreturn=none])])
noret=$ncbi_cv_c_noreturn
test "$noret" = "none"  &&  noret=""
AC_DEFINE_UNQUOTED(NCBI_NORETURN, $noret,
   [Define to whatever syntax, if any, your compiler supports for marking
    functions that never return.])

AC_CACHE_CHECK([syntax for declaring thread-local variables],
   ncbi_cv_c_tls_var,
   [ncbi_cv_c_tls_var=none
    for tls_var in __thread '__declspec(thread)' _Thread_local; do
       test "$ncbi_cv_c_tls_var" = "none" || break
       AC_COMPILE_IFELSE([AC_LANG_PROGRAM([$tls_var int i;],
          [static $tls_var int j;])],
          [ncbi_cv_c_tls_var=$tls_var], [])
    done])
tls_var=$ncbi_cv_c_tls_var
if test "$tls_var" != "none"; then
   AC_DEFINE_UNQUOTED(NCBI_TLS_VAR, $tls_var,
      [Define to whatever syntax your compiler supports for declaring
       thread-local variables, or leave undefined if it doesn't.])
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

AC_CACHE_CHECK([whether unaligned reads are permissible],
               ncbi_cv_sys_unaligned_reads,
   [AC_RUN_IFELSE([AC_LANG_PROGRAM([[ #include <stdio.h> ]],
       [[ unsigned int a[2] = { 0x01234567, 0x89abcdef };
          fprintf(stderr, "%x\n", *(unsigned int*)((char*)a + 1)); ]])],
       [ncbi_cv_sys_unaligned_reads=yes],
       [ncbi_cv_sys_unaligned_reads=no],
       [case "$host_cpu" in
          i?86|x86_64|powerpc*|ppc*) ncbi_cv_sys_unaligned_reads=yes ;;
          *)                         ncbi_cv_sys_unaligned_reads=no  ;;
        esac])])
if test "$ncbi_cv_sys_unaligned_reads" = yes; then
   AC_DEFINE(HAVE_UNALIGNED_READS, 1,
             [Define to 1 if your system permits reading integers from
              unaligned addresses.])
fi

### Check for the availability of other packages
### --------------------------------------------


AC_MSG_CHECKING([whether in-house NCBI resources are available])
if test -n "$orig_NCBI" -a -f "$orig_NCBI/.ncbirc" \
    -a -d /am/coremake_builds/test_data; \
then
   NCBI_FEATURE(in-house-resources)
   AC_MSG_RESULT(yes)
else
   AC_MSG_RESULT(no)
fi

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

orig_LIBS=$LIBS
LIBS="$NETWORK_LIBS $orig_LIBS"
AC_CHECK_FUNCS(inet_ntoa_r)
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
AC_CHECK_FUNCS(freehostent gethostent_r getipnodebyaddr getipnodebyname \
               inet_ntop)
LIBS=$orig_LIBS


### Save original compiler/linker flags
orig_CPPFLAGS="$CPPFLAGS"
orig_LDFLAGS="$LDFLAGS"
orig_LIBS="$LIBS"


### Check for math functions that aren't always available
AC_CHECK_FUNCS(erf)

NCBI_CHECK_LIBS(KSTAT, kstat, kstat_open)
NCBI_CHECK_LIBS(RPCSVC, rpcsvc, rstat)
AC_LANG_PUSH(C)
# Force C for this test because Sun WorkShop otherwise complains about
# multiple extern "C" declarations of setkey.
NCBI_CHECK_LIBS(CRYPT, crypt crypt_i, setkey)
AC_LANG_POP(C)
case "$host_os:$compiler" in
     solaris*:GCC ) ac_cv_search_dlopen="-ldl" ;;
     darwin*      ) ac_cv_search_iconv="-liconv" ;;
esac
NCBI_CHECK_LIBS(DL, dl, dlopen)
# libc's stubs aren't always sufficient...
case "$host_os:$libnsl" in
     solaris2.[[89]]:* ) orig_LIBS="$DL_LIBS $orig_LIBS" ;;
     solaris*:-lnsl    ) NETWORK_LIBS="$NETWORK_LIBS $DL_LIBS" ;;
esac
dnl NCBI_CHECK_LIBS(UUID, uuid, uuid_generate)
AC_SEARCH_LIBS(uuid_generate, uuid,
   [NCBI_PACKAGE(UUID)
    test "x$ac_cv_search_uuid_generate" = "xnone required" || \
       UUID_LIBS=$ac_cv_search_uuid_generate])
LIBS=$orig_LIBS
dnl NCBI_CHECK_LIBS(FUSE, fuse, fuse_loop)
AC_SEARCH_LIBS(fuse_loop, fuse, [NCBI_PACKAGE(FUSE)])
LIBS=$orig_LIBS
NCBI_CHECK_LIBS(RT, rt posix4, clock_gettime)
NCBI_CHECK_LIBS(DEMANGLE, demangle, cplus_demangle)
# Add to (orig_)LIBS if present for the sake of corelib and the
# following call to AC_CHECK_FUNCS.
if test -n "$DEMANGLE_LIBS$RT_LIBS"; then
    LIBS="$DEMANGLE_LIBS $RT_LIBS $LIBS"
    orig_LIBS="$DEMANGLE_LIBS $RT_LIBS $orig_LIBS"
fi
AC_CHECK_FUNCS([nanosleep sched_yield])

dnl NCBI_CHECK_LIBS(ICONV, iconv, iconv)
# We need to include <iconv.h>, since some implementations rename the symbols.
# This logic adapted from Bruno Haible's iconv.m4.
AC_CACHE_CHECK(for library containing iconv, ac_cv_search_iconv, [
   ac_cv_search_iconv=no
   AC_LINK_IFELSE(
      [AC_LANG_PROGRAM(
         [[#include <stdlib.h>
           #include <iconv.h>]],
         [[iconv_t cd = iconv_open("","");
           iconv(cd,NULL,NULL,NULL,NULL);
           iconv_close(cd);]])],
      [ac_cv_search_iconv="none required"])
   if test "$ac_cv_search_iconv" != "none required"; then
      for l in iconv; do
         LIBS="-l$l $orig_LIBS"
         AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
                [[#include <stdlib.h>
                  #include <iconv.h>]],
                [[iconv_t cd = iconv_open("","");
                  iconv(cd,NULL,NULL,NULL,NULL);
                  iconv_close(cd);]])],
            [ac_cv_search_iconv=-l$l])
         LIBS="$orig_LIBS"
      done
   fi])
if test "x$ac_cv_search_iconv" != xno; then
   AC_DEFINE(HAVE_LIBICONV, 1,
   [Define to 1 if ICONV is available, either in its own library or as part
    of the standard libraries.])
   AC_MSG_CHECKING([for iconv declaration])
   AC_CACHE_VAL(am_cv_proto_iconv, [
      AC_COMPILE_IFELSE([[
#include <stdlib.h>
#include <iconv.h>
extern
#ifdef __cplusplus
"C"
#endif
#if defined(__STDC__) || defined(__cplusplus)
size_t iconv (iconv_t cd, char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);
#else
size_t iconv();
#endif
]], am_cv_proto_iconv_arg1="", am_cv_proto_iconv_arg1="const")
      am_cv_proto_iconv="extern size_t iconv (iconv_t cd, $am_cv_proto_iconv_arg1 char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);"])
   am_cv_proto_iconv=`echo "[$]am_cv_proto_iconv" | tr -s ' ' | sed -e 's/( /(/'`
   AC_MSG_RESULT([$]{ac_t:-
       }[$]am_cv_proto_iconv)
   AC_DEFINE_UNQUOTED(ICONV_CONST, $am_cv_proto_iconv_arg1,
     [Define as const if the declaration of iconv() needs const.])
   if test "x$ac_cv_search_iconv" != "xnone required"; then
      ICONV_LIBS=$ac_cv_search_iconv
   fi
fi


### Third-party libraries

usr_lib=['[/usr]*/lib/*[amdsprcv]*[23469]*']
no_usr_lib="s,-L$usr_lib ,,g;"

NCBI_CHECK_THIRD_PARTY_LIB(z,
 [[AC_LANG_PROGRAM([#include <zlib.h>],
      [[gzFile fp; char buf[1024]; int n = gzread(fp, buf, sizeof(buf))]])]])
if test -z "$Z_LIBS"; then
   zlocal=util/compress/zlib
   AC_MSG_NOTICE([using local zlib copy in $zlocal])
   Z_PATH="<$zlocal>"
   Z_INCLUDE="-I\$(includedir)/$zlocal -I\$(includedir0)/$zlocal"
   # Z_LIBS="-lz"
   Z_LIB="z"
   # AC_DEFINE(USE_LOCAL_ZLIB, 1, [Define to 1 if using a local copy of zlib.])
   NCBI_PACKAGE(Z)
   NCBI_PACKAGE(LocalZ)
fi

NCBI_CHECK_THIRD_PARTY_LIB(bz2,
 [[AC_LANG_PROGRAM([#include <bzlib.h>],
      [[BZFILE* fp; char buf[1024]; int err;
        int n = BZ2_bzRead(&err, fp, buf, sizeof(buf))]])]])
if test -z "$BZ2_LIBS"; then
   bzlocal=util/compress/bzip2
   AC_MSG_NOTICE([using local bzlib copy in $bzlocal])
   BZ2_PATH="<$bzlocal>"
   BZ2_INCLUDE="-I\$(includedir)/$bzlocal -I\$(includedir0)/$bzlocal"
   # BZ2_LIBS="-lbz2"
   BZ2_LIB="bz2"
   AC_DEFINE(USE_LOCAL_BZLIB, 1, [Define to 1 if using a local copy of bzlib.])
   NCBI_PACKAGE(BZ2)
   NCBI_PACKAGE(LocalBZ2)
fi

if test -d "$LZO_PATH"; then
   NCBI_FIX_DIR(LZO_PATH)
fi
NCBI_CHECK_THIRD_PARTY_LIB_EX(lzo, LZO, lzo2,
 [[AC_LANG_PROGRAM([#include <lzo/lzo1x.h>],
      [[lzo_uint32 c = lzo_crc32(0, (const unsigned char*)"foo", 3);]])]])

if test -z "$PCRE_PATH"  &&  pcre-config --version >/dev/null 2>&1; then
    p=`pcre-config --prefix`
    test "x$p" = "x/usr"  ||  PCRE_PATH=$p
fi
NCBI_CHECK_THIRD_PARTY_LIB(pcre,
 [[AC_LANG_PROGRAM([#include <pcre.h>],
   [[const char*s[]={"x"}; pcre* p; pcre_extra* x = pcre_study(p, 1, s);]])]])
if test -z "$PCRE_LIBS"; then
   pcrelocal=util/regexp
   AC_MSG_NOTICE([using local PCRE copy in $pcrelocal])
   PCRE_PATH="<$pcrelocal>"
   PCRE_INCLUDE="-I\$(includedir)/$pcrelocal -I\$(includedir0)/$pcrelocal"
   # PCRE_LIBS="-lregexp"
   # PCREPOSIX_LIBS="-lregexp"
   PCRE_LIB="regexp"
   AC_DEFINE(USE_LOCAL_PCRE, 1, [Define to 1 if using a local copy of PCRE.])
   NCBI_PACKAGE(PCRE)
   NCBI_PACKAGE(LocalPCRE)
else
   PCREPOSIX_LIBS=`echo "$PCRE_LIBS" | sed -e 's/-lpcre/-lpcreposix -lpcre/'`
fi

## SSL/TLS libraries
case "$with_gcrypt" in
   no )       ac_cv_path_LIBGCRYPT_CONFIG=no ;;
   yes | '' )                                ;;
   * )        GCRYPT_PATH=$with_gcrypt       ;;
esac
if test -d "$GCRYPT_PATH"; then
   NCBI_FIX_DIR(GCRYPT_PATH)
fi
AC_PATH_PROG(LIBGCRYPT_CONFIG, libgcrypt-config, [], [${GCRYPT_BIN-$GCRYPT_PATH/${compiler_vpfx}${DEBUG_SFX}${mt_sfx}${bit64_sfx}/bin}:$GCRYPT_PATH/bin${bit64_sfx}:$GCRYPT_PATH/bin:$PATH])

if test "x$with_gcrypt" != xno; then
   if test -x "$LIBGCRYPT_CONFIG"; then
      if test -z "$GCRYPT_PATH"; then
         p=`libgcrypt-config --prefix`
         test "x$p" = "x/usr"  ||  GCRYPT_PATH=$p
      fi
      NCBI_RPATHIFY_OUTPUT(GCRYPT_LIBPATH,
        [$LIBGCRYPT_CONFIG --exec-prefix], [s,^,-L,; s,$,/lib,;])
      GCRYPT_INCLUDE=`$LIBGCRYPT_CONFIG --cflags`
      GCRYPT_CONFIG_LIBS="`$LIBGCRYPT_CONFIG --libs` $Z_LIBS"
   fi
else
   AS_UNSET(GCRYPT_CONFIG_LIBS)
fi
NCBI_CHECK_THIRD_PARTY_LIB(gcrypt,
 [[AC_LANG_PROGRAM([[#include <gcrypt.h>
                     #include <pthread.h>
                     #include <errno.h>
                     GCRY_THREAD_OPTION_PTHREAD_IMPL;]],
   [[gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);]])]],
 $GCRYPT_CONFIG_LIBS)
if test "x$with_gcrypt" != xno -a -n "$GCRYPT_CONFIG_LIBS"; then
   NCBI_RPATHIFY_OUTPUT(GCRYPT_LIBS, [echo $GCRYPT_CONFIG_LIBS], [$no_usr_lib])
fi

case "$with_gnutls" in
   no )       ac_cv_path_LIBGNUTLS_CONFIG=no ;;
   yes | '' )                                ;;
   * )        GNUTLS_PATH=$with_gnutls       ;;
esac
if test -d "$GNUTLS_PATH"; then
   NCBI_FIX_DIR(GNUTLS_PATH)
fi
AC_PATH_PROG(LIBGNUTLS_CONFIG, libgnutls-config, [], [${GNUTLS_BIN-$GNUTLS_PATH/${compiler_vpfx}${DEBUG_SFX}${mt_sfx}${bit64_sfx}/bin}:$GNUTLS_PATH/bin${bit64_sfx}:$GNUTLS_PATH/bin:$PATH])

if test "x$with_gnutls" != xno; then
   if test -x "$LIBGNUTLS_CONFIG"; then
      if test -z "$GNUTLS_PATH"; then
         p=`libgnutls-config --prefix`
         test "x$p" = "x/usr"  ||  GNUTLS_PATH=$p
      fi
   else
      LIBGNUTLS_CONFIG="eval PKG_CONFIG_PATH=\"$GNUTLS_PATH/lib/pkgconfig\" pkg-config gnutls"
      $LIBGNUTLS_CONFIG --exists >/dev/null 2>&1 ||  LIBGNUTLS_CONFIG=no
   fi
   case "$LIBGNUTLS_CONFIG" in
       eval\ *)
           NCBI_RPATHIFY_OUTPUT(GNUTLS_LIBPATH,
             [$LIBGNUTLS_CONFIG --libs-only-L])
           ;;
       no)
           ;;
       *)
           NCBI_RPATHIFY_OUTPUT(GNUTLS_LIBPATH,
             [$LIBGNUTLS_CONFIG --exec-prefix], [s,^,-L,; s,$,/lib,;])
           ;;
   esac

   if test "x$LIBGNUTLS_CONFIG" != xno; then
      GNUTLS_INCLUDE=`$LIBGNUTLS_CONFIG --cflags`
      GNUTLS_CONFIG_LIBS="`$LIBGNUTLS_CONFIG --libs` $Z_LIBS"
   fi
else
   AS_UNSET(GNUTLS_CONFIG_LIBS)
fi
NCBI_CHECK_THIRD_PARTY_LIB(gnutls,
 [[AC_LANG_PROGRAM([[#include <gnutls/gnutls.h>]],
   [[gnutls_session_t s; gnutls_init(&s, GNUTLS_CLIENT);]])]],
 $GNUTLS_CONFIG_LIBS $GCRYPT_LIBS)
if test "x$with_gnutls" != xno -a -n "$GNUTLS_CONFIG_LIBS"; then
   NCBI_RPATHIFY_OUTPUT(GNUTLS_LIBS, [echo $GNUTLS_CONFIG_LIBS], [$no_usr_lib])
   case "$GNUTLS_INCLUDE" in
       *$GCRYPT_INCLUDE* ) ;;
       *                 ) GNUTLS_INCLUDE="$GNUTLS_INCLUDE $GCRYPT_INCLUDE" ;;
   esac
   case "$GNUTLS_LIBS" in
       *\ -lgcrypt* ) ;;
       *            ) GNUTLS_LIBS="$GNUTLS_LIBS $GCRYPT_LIBS" ;;
   esac
fi

NCBI_CHECK_THIRD_PARTY_LIB_EX(openssl, OPENSSL, ssl,
 [[AC_LANG_PROGRAM([[#include <openssl/ssl.h>]],
   [[SSL_library_init();]])]],
 -lcrypto)

OPENSSL_STATIC_LIBS=$OPENSSL_LIBS
for d in `echo " $OPENSSL_LIBS" | fmt -w 1 | sed -ne 's/^ *-L//p'` \
   /usr/local/lib${bit64_sfx} /usr/local/lib /usr/lib${bit64_sfx} /usr/lib; do
   if test -f $d/libssl.a -a -f $d/libcrypto.a; then
      OPENSSL_STATIC_LIBS="$d/libssl.a $d/libcrypto.a"
      break
   elif test -f $d/libssl.so -o -f $d/libssl.dylib; then
      break
   fi
done

if test -n "$GNUTLS_LIBS"; then
   TLS_INCLUDE=$GNUTLS_INCLUDE
   TLS_LIBS=$GNUTLS_LIBS
else
   TLS_INCLUDE=$OPENSSL_INCLUDE
   TLS_LIBS=$OPENSSL_LIBS
fi


case "$with_krb5" in
   no )       ac_cv_path_KRB5_CONFIG=no    ;;
   yes | '' ) : ${KRB5_PATH=/usr/kerberos} ;;
   * )        KRB5_PATH=$with_krb5         ;;
esac
AC_PATH_PROG(KRB5_CONFIG, krb5-config, [], [${KRB5_BIN-$KRB5_PATH/bin}:$PATH])

if test "x$with_krb5" != xno; then
   if test -x "$KRB5_CONFIG"; then
      KRB5_PATH=`$KRB5_CONFIG --prefix`
      KRB5_INCLUDE=`$KRB5_CONFIG --cflags | \
         sed -e 's/$/ /; s,-I/usr/include ,,'`
      KRB5_CONFIG_LIBS=`$KRB5_CONFIG --libs gssapi | \
         sed -e 's/^/ /; s/ -[[^Ll]][[^ ]]*//g'`
   fi
else
   dnl AS_UNSET(KRB5_CONFIG_LIBS)
   KRB5_CONFIG_LIBS=$KRB5_LIBS
fi
NCBI_CHECK_THIRD_PARTY_LIB_EX(krb5, KRB5, gssapi_krb5,
 [[AC_LANG_PROGRAM([[#include <time.h>
extern "C" {
#include <gssapi/gssapi_krb5.h>
}]],
   [[OM_uint32 min_stat;
     gss_buffer_desc buf;
     gss_release_buffer(&min_stat, &buf);]])]],
 $KRB5_CONFIG_LIBS)
if test "x$with_krb5" != xno; then
   NCBI_RPATHIFY_OUTPUT(KRB5_LIBS, [echo $KRB5_CONFIG_LIBS], [$no_usr_lib])
fi


NCBI_CHECK_THIRD_PARTY_LIB(curl,
   [AC_LANG_PROGRAM([#include <curl/curl.h>],
       [char* v = curl_version();])])


## SYBASE libraries

NETWORK_PURE_LIBS="$NETWORK_LIBS"
ncbi_xreader_pubseqos=
UNLESS_PUBSEQOS=
if test "$with_sybase" != "no" ; then
   sybase_lib=lib$bit64_sfx
   resolve_sybase=true
   default_pointer=default-${NCBI_PLATFORM_BITS}bit
   : ${with_sybase_new:="no"}
   case "$host_os" in
      linux* )
         : ${with_sybase_local:=yes}
         ;;
      * )
         if test -d "$net_sybase_root" ; then
            : ${with_sybase_local:=no}
         else
            : ${with_sybase_local:=yes}
         fi
         ;;
   esac

   if test "$with_sybase_local" != "no" ; then
      if test "$with_sybase_local" = "yes" ; then
         if test -n "$SYBASE_PATH" ; then
            with_sybase_local=$SYBASE_PATH
         else
            if test "$with_sybase_new" = "yes" ; then
               with_sybase_local=$local_sybase_root/new
            elif test -f "$local_sybase_root/$default_pointer" ; then
               with_sybase_local=`cat $local_sybase_root/$default_pointer`
               resolve_sybase=false
            else
               with_sybase_local=$local_sybase_root/current
            fi
            if test -d "$with_sybase_local$NCBI_PLATFORM_BITS" ; then
               with_sybase_local=$with_sybase_local$NCBI_PLATFORM_BITS
            fi
         fi # default paths
      fi # no command-line path
      if test -d "$with_sybase_local" ; then
         SYBASE_PATH=$with_sybase_local
         SYBASE_LCL_PATH=
      else
         with_sybase_local=no
      fi
   fi

   if test "$with_sybase_local" = "no" ; then
      if test "$with_sybase_new" = "yes" ; then
         if test -d "$net_sybase_root/new$NCBI_PLATFORM_BITS" ; then
            SYBASE_LCL_PATH=$local_sybase_root/new$NCBI_PLATFORM_BITS
            SYBASE_PATH=$net_sybase_root/new$NCBI_PLATFORM_BITS
         elif test -d "$net_sybase_root/new" ; then
            SYBASE_LCL_PATH=$local_sybase_root/new
            SYBASE_PATH=$net_sybase_root/new
         else
            with_sybase_new=no
         fi
      fi

      if test -z "$SYBASE_PATH" ; then
         if test -f "$net_sybase_root/$default_pointer" ; then
            SYBASE_LCL_PATH=
            SYBASE_PATH=`cat $local_sybase_root/$default_pointer`
            resolve_sybase=false
         elif test -d "$net_sybase_root/current$NCBI_PLATFORM_BITS" ; then
            SYBASE_LCL_PATH=$local_sybase_root/current$NCBI_PLATFORM_BITS
            SYBASE_PATH=$net_sybase_root/current$NCBI_PLATFORM_BITS
         elif test -d "$net_sybase_root/current" ; then
            SYBASE_LCL_PATH=$local_sybase_root/current
            SYBASE_PATH=$net_sybase_root/current
         else
            SYBASE_LCL_PATH=
            SYBASE_PATH=$SYBASE
         fi
      fi
   fi

   # Lock in the version we found, ignoring symlinks further up
   case "$SYBASE_PATH" in
      */[[1-9]]* ) resolve_sybase=false ;;
   esac
   if test -d "$SYBASE_PATH"  &&  $resolve_sybase; then
      # SYBASE_PATH_=`cd $SYBASE_PATH && /bin/pwd`
      SYBASE_PATH_=`ls -ld $SYBASE_PATH | sed -ne 's/.* -> //p'`
      case "$SYBASE_PATH_" in
         '' ) ;;
         /* )
            SYBASE_VERSION=`basename $SYBASE_PATH_`
            SYBASE_PATH=`dirname $SYBASE_PATH`/$SYBASE_VERSION
            test -d "$SYBASE_PATH"  ||  SYBASE_PATH=$SYBASE_PATH_
            ;;
         * )
            orig_parent=`dirname $SYBASE_PATH`
            SYBASE_VERSION=`basename $SYBASE_PATH_`
            SYBASE_PATH=$orig_parent/$SYBASE_VERSION
            test -d "$SYBASE_PATH"  ||  SYBASE_PATH=$orig_parent/$SYBASE_PATH_
            ;;
      esac
   fi

   if test -d "$SYBASE_LCL_PATH"  &&  $resolve_sybase ; then
      # SYBASE_LCL_PATH_=`cd $SYBASE_LCL_PATH && /bin/pwd`
      SYBASE_LCL_PATH_=`ls -ld $SYBASE_LCL_PATH | sed -ne 's/.* -> //p'`
      case "$SYBASE_LCL_PATH_" in
         '' ) ;;
         /* )
            SYBASE_LCL_VERSION=`basename $SYBASE_LCL_PATH_`
            SYBASE_LCL_PATH=`dirname $SYBASE_LCL_PATH`/$SYBASE_LCL_VERSION
            test -d "$SYBASE_LCL_PATH"  ||  SYBASE_LCL_PATH=$SYBASE_LCL_PATH_
            ;;
         * )
            orig_parent=`dirname $SYBASE_LCL_PATH`
            SYBASE_LCL_VERSION=`basename $SYBASE_LCL_PATH_`
            SYBASE_LCL_PATH=$orig_parent/$SYBASE_LCL_VERSION
            test -d "$SYBASE_LCL_PATH"  || \
               SYBASE_LCL_PATH=$orig_parent/$SYBASE_LCL_PATH_
            ;;
      esac
   elif test -n "$SYBASE_LCL_PATH" ; then
      SYBASE_LCL_PATH=`dirname $SYBASE_LCL_PATH`/$SYBASE_VERSION
   fi

   if test -n "$SYBASE_VERSION" -a -n "$SYBASE_LCL_VERSION" \
        -a "$SYBASE_VERSION" != "$SYBASE_LCL_VERSION" ; then
      AC_MSG_WARN([ignoring inconsistent local Sybase symlink: $SYBASE_LCL_VERSION vs. $SYBASE_VERSION])
      SYBASE_LCL_PATH=`dirname $SYBASE_LCL_PATH`/$SYBASE_VERSION
   fi

   if test -n "$SYBASE" ; then
      if test -d "$net_sybase_root" -o -d "$local_sybase_root" ; then
         case $SYBASE in
            $net_sybase_root/* | $local_sybase_root/* ) ;;
            * ) AC_MSG_WARN([ignoring non-standard SYBASE setting: $SYBASE]) ;;
            # Ignored regardless, but standard values are not so concerning.
         esac
      fi
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
   test -f "$SYBASE_LIBPATH/libtli.so" || sybtli=

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
      NCBI_RPATHIFY(SYBASE_LPATH, $SYBASE_LIBPATH)
   else
      SYBASE_LCL_LIBPATH="$SYBASE_LCL_PATH/${sybase_lib}"
      NCBI_RPATHIFY(SYBASE_LPATH, $SYBASE_LCL_LIBPATH $SYBASE_LIBPATH)
      SYBASE_INCLUDE="-I$SYBASE_LCL_PATH/include $SYBASE_INCLUDE"
   fi
   SYBASE_LIBS="$SYBASE_LPATH $SYBASE_LIBLIST"
   SYBASE_DBLIBS="$SYBASE_LPATH $SYBASE_DBLIBS"
   SYBASE_DLLS="$SYBASE_DLLLIST"

   AC_CACHE_CHECK([for Sybase${SYBASE_PATH:+ in $SYBASE_PATH}],
      ncbi_cv_lib_sybase,
      [CPPFLAGS="$SYBASE_INCLUDE $orig_CPPFLAGS"
       LIBS="$SYBASE_LIBS $SYBASE_DLLS $SYBASE_NETWORK_LIBS $DL_LIBS $orig_LIBS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <ctpublic.h>]],
          [[CS_RETCODE i = ct_init(0,0);  i = cs_ctx_global(0,0); ]])],
          [ncbi_cv_lib_sybase=yes], [ncbi_cv_lib_sybase=no])])

   if test "$ncbi_cv_lib_sybase" = yes; then
      NCBI_PACKAGE(Sybase)
      AC_DEFINE(HAVE_LIBSYBASE, 1,
                [Define to 1 if SYBASE libraries are available.])
      if test "$with_mt" = "yes" -a "$ncbi_cv_lib_sybase_r" = "yes"; then
         AC_DEFINE(HAVE_SYBASE_REENTRANT, 1,
                   [Define to 1 if SYBASE has reentrant libraries.])
      fi
      if test "$with_dbapi" != no -a -f ${real_srcdir}/src/objtools/data_loaders/genbank/pubseq/Makefile.ncbi_xreader_pubseqos.lib ; then
         AC_CACHE_CHECK([for PubSeq service availability], ncbi_cv_db_pubseq,
            [if grep PUBSEQ_OS $SYBASE_PATH/interfaces >/dev/null; then
                ncbi_cv_db_pubseq=yes
             else
                ncbi_cv_db_pubseq=no
             fi])
         if test "$ncbi_cv_db_pubseq" = yes; then
            AC_DEFINE(HAVE_PUBSEQ_OS, 1,
                      [Define to 1 if the PUBSEQ service is available.])
            NCBI_FEATURE(PubSeqOS)
            ncbi_xreader_pubseqos=ncbi_xreader_pubseqos
            ncbi_xreader_pubseqos2=ncbi_xreader_pubseqos2
            UNLESS_PUBSEQOS=\#
         fi
      fi

      SYBASE_DBLIBS="$SYBASE_LPATH -lsybdb${bit64_sfx}"
      AC_CACHE_CHECK([for Sybase DBLib], ncbi_cv_lib_sybdb,
         [CPPFLAGS="$SYBASE_INCLUDE $orig_CPPFLAGS"
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
         NCBI_PACKAGE(DBLib)
         AC_DEFINE(HAVE_LIBSYBDB, 1,
                   [Define to 1 if SYBASE DBLib is available.])
      else
         SYBASE_DBLIBS=
      fi
      # apply the Sybase patch to $NETWORK_LIBS
      if test "$sybase_sol8pc_patch" = "yes" ; then
         NETWORK_LIBS="$SYBASE_LPATH $SYBASE_NETWORK_LIBS"
      fi
   else
      NCBI_MISSING_PACKAGE(sybase)
   fi
fi # with_sybase != no?

if test "$with_sybase" = no; then
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
   ftds64=ftds64
   try_local=yes
   case "$with_ftds" in
      64 | yes | '' )
         ftds_ver=64
         ftds64=ftds
         ;;
      * )
         FTDS_PATH=$with_ftds
         try_local=no
         ;;
   esac
   : ${FTDS_CTLIBS:="-lct -ltds"}
   NCBI_RPATHIFY(FTDS_CTLIBS,   $FTDS_PATH/lib,      [ ]$FTDS_CTLIBS)
   FTDS_INCLUDE="-I$FTDS_PATH/include"
   NCBI_LOCAL_FTDS(64)
   if test -n "$freetds" ; then
      FTDS_LIB="\$(FTDS${ftds_ver}_LIB)"
      FTDS_LIBS="\$(FTDS${ftds_ver}_LIBS)"
      FTDS_INCLUDE="\$(FTDS${ftds_ver}_INCLUDE)"
      if test "$with_ftds_renamed" != "no" ; then
          AC_DEFINE(NCBI_FTDS_RENAME_SYBDB, 1,
                    [Rename DBLIB symbols in FTDS to avoid name clash with Sybase DBLIB.])
      fi
      AC_MSG_NOTICE([Using bundled FreeTDS (version $ftds_ver) from $FTDS_PATH])
   else
      FTDS_LIBS="$FTDS_CTLIBS"
      AC_CACHE_CHECK([for FreeTDS${FTDS_PATH:+ in $FTDS_PATH}],
         ncbi_cv_lib_freetds,
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
         # Don't panic when using partial trees lacking dbapi/driver/ftdsN.
         NCBI_MISSING_PACKAGE(ftds)
      fi
   fi
fi
if test "$with_ftds" = "no" ; then
   FTDS_PATH=
   FTDS_INCLUDE=
   FTDS_LIBS=
   FTDS_LIB=
   FTDS64_INCLUDE=
   FTDS64_LIBS=
   FTDS64_LIB=
   FTDS64_CTLIB_INCLUDE=
   FTDS64_CTLIB_LIBS=
   FTDS64_CTLIB_LIB=
   freetds=
else
   AC_DEFINE(HAVE_LIBFTDS, 1,
             [Define to 1 if FreeTDS libraries are available.])
   NCBI_PACKAGE(FreeTDS)
fi
AC_SUBST(FTDS64_INCLUDE)
AC_SUBST(FTDS64_LIBS)
AC_SUBST(FTDS64_LIB)
AC_SUBST(FTDS64_CTLIB_INCLUDE)
AC_SUBST(FTDS64_CTLIB_LIBS)
AC_SUBST(FTDS64_CTLIB_LIB)
AC_SUBST(FTDS_INCLUDE)
AC_SUBST(FTDS_LIBS)
AC_SUBST(FTDS_LIB)
AC_SUBST(freetds)
AC_SUBST(ftds64)


## MySQL
if test "$with_mysql" != "no" ; then
   case "$with_mysql" in
      yes | "" ) ;;
      *        ) MYSQL_PATH=$with_mysql ;;
   esac
   : ${MYSQL_BINPATH=$MYSQL_PATH/bin}
   AC_PATH_PROG(mysql_config, mysql_config, [], [$MYSQL_BINPATH:$PATH])
   if test -n "$mysql_config" ; then
      : ${MYSQL_BINPATH=`dirname $mysql_config`}
      # Kill off single quotes, due to later requoting
      : ${MYSQL_INCLUDE=`$mysql_config --include | tr -d \'`}
      NCBI_RPATHIFY_OUTPUT_COND(MYSQL_LIBS, $mysql_config --libs${mt_sfx:+_r},
         [s/'//g; $no_usr_lib])
      AC_CACHE_CHECK([whether MySQL works], ncbi_cv_lib_mysql,
         [CPPFLAGS="$MYSQL_INCLUDE $orig_CPPFLAGS"
          LIBS="$MYSQL_LIBS $orig_LIBS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <mysql.h>]],
          [[MYSQL *p;  p = mysql_init(p);]])],
          [ncbi_cv_lib_mysql=yes], [ncbi_cv_lib_mysql=no])])
      if test "$ncbi_cv_lib_mysql" = no; then
         NCBI_MISSING_PACKAGE(mysql)
      fi
   else
      NCBI_MISSING_PACKAGE(mysql)
   fi
fi
if test "$with_mysql" = "no" ; then
   MYSQL_PATH="No_MYSQL"
   MYSQL_INCLUDE=
   MYSQL_LIBS=
else
   NCBI_PACKAGE(MySQL)
   AC_DEFINE(HAVE_MYSQL, 1, [Define to 1 if MySQL is available.])
fi


## Berkeley DB library
if test "$with_bdb" != "no" ; then
   case "$with_bdb" in
      yes | "" )
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
   if test -d "$BERKELEYDB_PATH"; then
      NCBI_FIX_DIR(BERKELEYDB_PATH)
      : ${BERKELEYDB_INCLUDE:="-I$BERKELEYDB_PATH/include"}
   fi
   if test -z "${BERKELEYDB_LIBPATH}"; then
      for d in "${compiler_vpfx}${DEBUG_SFX}" "${compiler_pfx}${DEBUG_SFX}" \
               "${DEBUG_SFX}" lib; do
         for sfx in "$mt_sfx$bit64_sfx" "$bit64_sfx$mt_sfx" "$bit64_sfx" \
                    "$mt_sfx" ""; do
            BERKELEYDB_LIBPATH=${BERKELEYDB_PATH}/$d$sfx
            test -d "${BERKELEYDB_LIBPATH}"  &&  break
         done
         test -d "${BERKELEYDB_LIBPATH}"  &&  break
      done
   fi
   if test -d "$BERKELEYDB_LIBPATH"; then
      NCBI_FIX_DIR(BERKELEYDB_LIBPATH)
      NCBI_RPATHIFY_COND(BERKELEYDB_LIBS:, ${BERKELEYDB_LIBPATH}, [ -ldb])
   else
      : ${BERKELEYDB_LIBS:="-ldb"}
   fi
   if test -f "${BERKELEYDB_LIBPATH}/libdb-static.a"; then
      BERKELEYDB_STATIC_LIBS="-L${BERKELEYDB_LIBPATH} -ldb-static"
   else
      BERKELEYDB_STATIC_LIBS=${BERKELEYDB_LIBS}
   fi
   AC_CACHE_CHECK(
      [for Berkeley DB libraries${BERKELEYDB_PATH:+ in $BERKELEYDB_PATH}],
      ncbi_cv_lib_berkeley_db,
      [CPPFLAGS="$BERKELEYDB_INCLUDE $orig_CPPFLAGS"
       LIBS="$BERKELEYDB_LIBS $orig_LIBS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <db.h>]],
          [[ DB* dbp;  db_create(&dbp, NULL, 0);  dbp->app_private = 0; ]])],
          [ncbi_cv_lib_berkeley_db=yes], [ncbi_cv_lib_berkeley_db=no])])
   if test "$ncbi_cv_lib_berkeley_db" = "no"; then
      NCBI_MISSING_PACKAGE(bdb)
   else
      AC_CACHE_CHECK([Berkeley DB version (4.3 or newer required)],
         ncbi_cv_lib_berkeley_db_version,
         [AC_LANG_CONFTEST([AC_LANG_SOURCE([[
#include <db.h>
ncbi_cv_lib_berkeley_db_version=DB_VERSION_MAJOR.DB_VERSION_MINOR.DB_VERSION_PATCH
          ]])])
          eval "$ac_cpp $BERKELEYDB_INCLUDE conftest.$ac_ext" \
             2>&AS_MESSAGE_LOG_FD | grep '^ncbi_cv_' \
             | tr -d "$wschars" > conftest.sh
          . ./conftest.sh
          rm -f contest*
         ])
      case "$ncbi_cv_lib_berkeley_db_version" in
         1.* | 2.* | 3.* | 4.[[0-2]].* )
            AC_MSG_WARN(
               [Your Berkeley DB version is too old to use.  (Needed >= 4.3.)])
            if test -n "$with_bdb"; then
               AC_MSG_ERROR([--with-bdb:  Berkeley DB library is too old])
            fi
            with_bdb=no
            ;;
         4.[[3-6]].* ) ;;
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
   BERKELEYDB_STATIC_LIBS=
   BERKELEYDB_CXX_LIBS=
   BERKELEYDB_CXX_STATIC_LIBS=
else
   NCBI_PACKAGE(BerkeleyDB)
   AC_DEFINE(HAVE_BERKELEY_DB, 1,
             [Define to 1 if Berkeley DB libraries are available.])
   if test -d "$BERKELEYDB_LIBPATH"; then
      NCBI_RPATHIFY_COND(BERKELEYDB_CXX_LIBS:, ${BERKELEYDB_LIBPATH},
          [ -ldb_cxx -ldb])
   else
      : ${BERKELEYDB_CXX_LIBS:="-ldb_cxx -ldb"}
   fi
   if test -f "${BERKELEYDB_LIBPATH}/libdb_cxx-static.a"; then
      BERKELEYDB_CXX_STATIC_LIBS="-L${BERKELEYDB_LIBPATH} -ldb_cxx-static -ldb-static"
   else
      BERKELEYDB_CXX_STATIC_LIBS=${BERKELEYDB_CXX_LIBS}
   fi
   AC_CACHE_CHECK([for native Berkeley DB C++ wrappers (optional)],
      ncbi_cv_lib_berkeley_db_cxx,
      [CPPFLAGS="$BERKELEYDB_INCLUDE $orig_CPPFLAGS"
       LIBS="$BERKELEYDB_CXX_LIBS $orig_LIBS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <db_cxx.h>]],
          [[DbEnv env(0);  Db db(&env, 0);  db.set_app_private(NULL);]])],
          [ncbi_cv_lib_berkeley_db_cxx=yes],
          [ncbi_cv_lib_berkeley_db_cxx=no])])
   if test "$ncbi_cv_lib_berkeley_db_cxx" = "yes"; then
      NCBI_PACKAGE(BerkeleyDB++)
      AC_DEFINE(HAVE_BERKELEY_DB_CXX, 1,
                [Define to 1 if the Berkeley `db_cxx' library is available.])
   else
      BERKELEYDB_CXX_LIBS=
      BERKELEYDB_CXX_STATIC_LIBS=
   fi
fi


## ODBC
case "$with_odbc" in
   yes | ""   ) : ${ODBC_PATH:=dbapi/driver/odbc/unix_odbc} ;;
   internal | no ) ODBC_PATH=dbapi/driver/odbc/unix_odbc       ;;
   *          ) ODBC_PATH=$with_odbc                        ;;
esac

case "$ODBC_PATH" in
 dbapi/* ) ;;
 *)
   ODBC_INCLUDE="-I$ODBC_PATH/include"
   ODBC_LIBPATH="${ODBC_PATH}/lib"
   NCBI_RPATHIFY(ODBC_LIBS, ${ODBC_LIBPATH}, [ -lodbc -lodbcinst])
   CPPFLAGS="$ODBC_INCLUDE $orig_CPPFLAGS"
   LIBS="$ODBC_LIBS $orig_LIBS"
   AC_CACHE_CHECK([for ODBC libraries${ODBC_PATH:+ in $ODBC_PATH}],
      ncbi_cv_lib_odbc,
      [AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <sql.h>]],
          [[SQLHDBC hdbc;  SQLCHAR* cp = (SQLCHAR*) "x";
            SQLRETURN x = SQLConnect(hdbc, cp, 1, cp, 2, cp, 3);]])],
          [ncbi_cv_lib_odbc=yes], [ncbi_cv_lib_odbc=no])])
   if test "$ncbi_cv_lib_odbc" = "no"; then
      dnl ODBC_PATH=dbapi/driver/odbc/unix_odbc
      dnl AC_MSG_WARN([Falling back to internal FreeTDS-specific ODBC implementation.])
      NCBI_MISSING_PACKAGE(odbc)
   else
      # odbcss.h isn't self-contained :-/
      AC_CHECK_HEADERS(odbcss.h, [], [], [#include <sql.h>])

      AC_CHECK_FUNCS(SQLGetPrivateProfileString)
      AC_CHECK_TYPES(SQLLEN,,,[#include <sqltypes.h>])

      AC_CACHE_CHECK([whether SQLColAttribute's last argument is an SQLLEN *],
         ncbi_cv_func_sqlcolattribute_sqllen,
         [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
            [[#include <sql.h>>
              SQLRETURN SQL_API SQLColAttribute
                (SQLHSTMT sh, SQLUSMALLINT cn, SQLUSMALLINT fi, SQLPOINTER ca,
                 SQLSMALLINT bl, SQLSMALLINT *sl, SQLLEN *na);]],
            [[]])],
            ncbi_cv_func_sqlcolattribute_sqllen=yes,
            ncbi_cv_func_sqlcolattribute_sqllen=no)])
      if test $ncbi_cv_func_sqlcolattribute_sqllen = yes; then
         AC_DEFINE(NCBI_SQLCOLATTRIBUTE_SQLLEN, 1,
            [Define to 1 if SQLColAttribute's last argument is an SQLLEN *])
      fi
   fi
   ;;
esac

case "$ODBC_PATH" in
 dbapi/*)
   # ODBC_PATH="No_ODBC"
   # Point to a local copy of unixODBC's headers, required by FreeTDS 0.64+.
   ODBC_INCLUDE="-I\$(includedir)/$ODBC_PATH -I\$(includedir0)/$ODBC_PATH"
   ODBC_LIBS=
   dnl AC_DEFINE(HAVE_SQLGETPRIVATEPROFILESTRING, 1)
   AC_DEFINE(HAVE_SQLLEN, 1)
   ;;
 *)
   NCBI_PACKAGE(ODBC)
   AC_DEFINE(HAVE_ODBC, 1, [Define to 1 if ODBC libraries are available.])
   ;;
esac

## Python
if test "$with_python" != "no" ; then
   case "$with_python" in
      yes | "" ) ;;
      */*      ) PYTHON_PATH=$with_python ;;
   esac
   NCBI_CHECK_PYTHON()
   NCBI_CHECK_PYTHON(2.3)
   : ${PYTHON_PATH:="/usr/local/python-2.4.4"}
   NCBI_CHECK_PYTHON(2.4)
   NCBI_CHECK_PYTHON(2.5)
   if test "$ncbi_cv_lib_python" = "no"; then
      # Favor python 2.4 as the only version configure historically
      # recognized, but otherwise prefer newer versions.
      if test "$ncbi_cv_lib_python24" = "yes"; then
         PYTHON_INCLUDE=$PYTHON24_INCLUDE
         PYTHON_LIBS=$PYTHON24_LIBS
      elif test "$ncbi_cv_lib_python25" = "yes"; then
         PYTHON_INCLUDE=$PYTHON25_INCLUDE
         PYTHON_LIBS=$PYTHON25_LIBS
      elif test "$ncbi_cv_lib_python23" = "yes"; then
         PYTHON_INCLUDE=$PYTHON23_INCLUDE
         PYTHON_LIBS=$PYTHON23_LIBS
      fi
      if test -n "$PYTHON_LIBS"; then
         NCBI_PACKAGE(PYTHON)
         AC_DEFINE(HAVE_PYTHON, 1)
      else
         NCBI_MISSING_PACKAGE(python)
      fi
   fi
fi

## Perl
if test "$with_perl" != "no" ; then
   case "$with_perl" in
      yes | "" ) ;;
      */*      ) PERL_PATH=$with_perl ;;
   esac
   AC_PATH_PROG(PERL, perl, [], [$PERL_PATH/bin:$PATH])
   if test -x "$PERL"; then
      PERL_ARCHLIB=`$PERL -MConfig -e 'print \$Config{archlibexp};'`
      PERL_FLAGS=`$PERL -MConfig -e 'print join " ", grep /^-[[DI]]/, split /\\s+/, \$Config{cppflags};'`
      PERL_INCLUDE="-I$PERL_ARCHLIB/CORE $PERL_FLAGS"
      PERL_DEPS=`$PERL -MConfig -e 'print \$Config{libs};'`
      NCBI_RPATHIFY(PERL_LIBS, $PERL_ARCHLIB/CORE, [ -lperl $PERL_DEPS])
      CPPFLAGS="$PERL_INCLUDE $orig_CPPFLAGS"
      LIBS="$PERL_LIBS $orig_LIBS"
      AC_CACHE_CHECK([for usable Perl libraries], [ncbi_cv_lib_perl],
         [AC_LINK_IFELSE([AC_LANG_PROGRAM(
                [[
#include <EXTERN.h>
#include <perl.h>]],
                [[PerlInterpreter* p = perl_alloc();]])],
             [ncbi_cv_lib_perl=yes], [ncbi_cv_lib_perl=no])])
   fi
   if test "$ncbi_cv_lib_perl" = yes; then
      NCBI_PACKAGE(PERL)
      dnl AC_DEFINE(HAVE_PERL, 1)
   else
      NCBI_MISSING_PACKAGE(perl)
      PERL_INCLUDE=
      PERL_LIBS=
   fi
fi

## Java
if test "${with_jni-no}" != "no"; then
   case "$with_jni:${JAVAC+set}" in
      yes: | : )
         if test -d "$JAVA_HOME"; then
            JAVAC=$JAVA_HOME/bin/javac
         else
            JAVAC=javac
         fi
         ;;
      * )
         JAVAC=$with_jni/bin/javac
         ;;
   esac
   AX_JNI_INCLUDE_DIR
   JDK_INCLUDE=''
   sep=''
   for x in $JNI_INCLUDE_DIRS; do
      JDK_INCLUDE=$JDK_INCLUDE$sep-I$x
      sep=' '
   done
   AC_MSG_NOTICE([setting JDK_INCLUDE = $JDK_INCLUDE])
   CPPFLAGS="$JDK_INCLUDE $orig_CPPFLAGS"
   AC_CHECK_HEADER(jni.h,
      [AC_DEFINE(NCBI_JNI, 1,
          [Define to 1 if building Java Native Interface bindings.])
       NCBI_FEATURE([JDK Ncbi-JNI])
       JDK_PATH=$_JTOPDIR
       ncbi_java=ncbi_java],
      [JDK_INCLUDE=''
       JDK_PATH=''
       ncbi_java=''])
fi


## Boost
if test "$with_boost" != "no"; then
   if test "${with_boost-yes}" != yes; then
      BOOST_PATH=$with_boost
   fi
   if test -d "$BOOST_PATH"; then
      NCBI_FIX_DIR(BOOST_PATH)
      : ${BOOST_INCLUDE=-I$BOOST_PATH/include}
   fi
   BOOST_LIBPATH_=$BOOST_PATH/lib
   NCBI_RPATHIFY_COND(BOOST_LIBPATH, $BOOST_LIBPATH_)
   test -d "$BOOST_PATH"  ||  BOOST_LIBPATH_=/usr/lib
   boost_path_digits=`basename $BOOST_PATH | sed -e 's/-ncbi[[0-9]]*$//; s/.0$//' | tr -cd 0123456789`
   if test -n "$boost_path_digits"; then
      boost_version_digits=`echo $ncbi_cv_lib_boost_version | sed -e 's/.0$//' | tr -cd 0123456789`
      if test $boost_path_digits != "$boost_version_digits"; then
         # presumably stale, particularly if boost_version WASN'T cached;
         # uncache all relevant settings
         AS_UNSET(ncbi_cv_lib_boost_version)
         AS_UNSET(ncbi_cv_lib_boost_regex)
         AS_UNSET(ncbi_cv_lib_boost_spirit)
         AS_UNSET(ncbi_cv_lib_boost_system)
         AS_UNSET(ncbi_cv_lib_boost_test)
         AS_UNSET(ncbi_cv_lib_boost_thread)
      fi
   fi
      AC_CACHE_CHECK([Boost version],
         ncbi_cv_lib_boost_version,
         [AC_LANG_CONFTEST([AC_LANG_SOURCE([[
#include <boost/version.hpp>
ncbi_cv_lib_boost_version=BOOST_LIB_VERSION
          ]])])
          eval "$ac_cpp $BOOST_INCLUDE conftest.$ac_ext" \
             2>&AS_MESSAGE_LOG_FD | grep '^ncbi_cv_' \
             | tr -d "$wschars" > conftest.sh
          . ./conftest.sh
          rm -f contest*
         ])
   if test -d `echo $BOOST_INCLUDE | sed -e 's/^-I//'`/boost-${ncbi_cv_lib_boost_version}/boost; then
      BOOST_INCLUDE=$BOOST_INCLUDE/boost-${ncbi_cv_lib_boost_version}
   fi
   case "$ncbi_compiler" in
      MIPSPRO)
         BOOST_INCLUDE="$BOOST_INCLUDE $BOOST_INCLUDE/boost/compatibility/cpp_c_headers"
         ;;
      WORKSHOP)
         # Boost.Test's macros yield a *lot* of spurious "should not initialize
         # a non-const reference with a temporary" warnings, to the point of
         # overwhelming the compiler in some cases; turn them off altogether
         # when using Boost at all.
         BOOST_INCLUDE="$BOOST_INCLUDE -erroff=reftotemp"
         ;;
   esac
   case "$ncbi_cv_lib_boost_version" in
      0_* | 1_[[0-9]] | 1_[[0-9]]_* | 1_[[0-2]][[0-9]] | 1_[[0-2]][[0-9]]_* ) ;;
      1_3[[0-4]] | 1_3[[0-4]]_* )
         AC_MSG_WARN(
            [Your Boost version is too old to use.  (Needed >= 1.35.)])
         if test -n "$with_boost"; then
            AC_MSG_ERROR([--with-boost:  Boost library is too old])
         fi
         with_boost=no
         ;;
      1_3[[5-9]] | 1_3[[5-9]]_* | 1_4* | 1_5[[0-3]] | 1_5[[0-3]]_* ) ;;
      '' ) with_boost=no ;;
      * )
         AC_MSG_WARN(
            [Untested Boost version; may prove incompatible.])
         AC_MSG_WARN(
            [If so, please re-run this script with the flag --without-boost.])
         ;;
   esac
   AC_MSG_CHECKING([for Boost library name tag])
   case "$with_boost_tag" in
      yes | "" )
         case "$host_os:$ncbi_compiler" in
            darwin*:GCC) BOOST_TAG="-xgcc" ;;
            *:GCC)       BOOST_TAG="-gcc" ;;
            *:KCC)       BOOST_TAG="-kcc" ;;
            linux*:ICC)  BOOST_TAG="-il"  ;;
            *:WORKSHOP)  BOOST_TAG="-sw"  ;;
            *:MIPSPRO)   BOOST_TAG="-mp"  ;;
            *:VISUALAGE) BOOST_TAG="-xlc" ;;
            *:COMPAQ)    BOOST_TAG="-tru" ;;
            *:MSVC)      BOOST_TAG="-vc"  ;;
         esac
         case "$BOOST_TAG" in
            -*gcc|-il ) boost_comp_vers=`echo $compiler_version | cut -c1-2` ;;
            *         ) boost_comp_vers='' ;;
         esac
         test "$with_mt"    = "yes"  &&  BOOST_MT=-mt
         test "$with_debug" = "yes"  &&  BOOST_D=-d
         test "$bit64_sfx"  = "64"   &&  BOOST_64=-64
         found=no
         for base in "$BOOST_TAG$boost_comp_vers" "$BOOST_TAG" ''; do
           for bvers in "-$ncbi_cv_lib_boost_version" ''; do
             for b64 in $BOOST_64 ''; do
               for bmt in "$BOOST_MT" '-mt' ''; do
                 for bd in "$BOOST_D" '' '-d'; do
                   bsfx=$cvers$bmt$bd$bvers$b64
                   if ls $BOOST_LIBPATH_/libboost*$base$bsfx.* >/dev/null 2>&1;
                   then
                     found=yes
                     break
                   fi
                 done
                 test "$found" = yes  &&  break
               done
               test "$found" = yes  &&  break
             done
             test "$found" = yes  &&  break
           done
           test "$found" = yes  &&  break
         done

         if test "$found" = no; then
            base=$BOOST_TAG
            cvers=$boost_comp_vers
            bvers=-$ncbi_cv_lib_boost_version
            b64=$BOOST_64
            bmt=$BOOST_MT
            bd=$BOOST_D
            bsfx=$cvers$bmt$bd$bvers$b64
         fi

         BOOST_TAG=$base$bsfx
         AC_MSG_RESULT([$BOOST_TAG (autodetected)])
         ;;
      no )
         BOOST_TAG=""
         AC_MSG_RESULT([none (by explicit request)])
         ;;
      *)
         BOOST_TAG=$with_boost_tag
         AC_MSG_RESULT([$BOOST_TAG (by explicit request)])
         ;;
   esac

   test "$with_mt" = yes  || \
      BOOST_INCLUDE="$BOOST_INCLUDE -DBOOST_DISABLE_THREADS"

   BOOST_REGEX_LIBS=-lboost_regex${BOOST_TAG}
   if test -f "$BOOST_LIBPATH_/libboost_regex${BOOST_TAG}-static.a"; then
       BOOST_REGEX_STATIC_LIBS=-lboost_regex${BOOST_TAG}-static
   else
       BOOST_REGEX_STATIC_LIBS=$BOOST_REGEX_LIBS
   fi

   in_path=${BOOST_PATH:+ in $BOOST_PATH}

   AC_CACHE_CHECK([for Boost.Regex$in_path], ncbi_cv_lib_boost_regex,
      CPPFLAGS="$BOOST_INCLUDE $orig_CPPFLAGS"
      LIBS="$BOOST_LIBPATH $BOOST_REGEX_LIBS $RT_LIBS $orig_LIBS"
      [AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([[#include <boost/regex.hpp>]],
              [[throw boost::regex_error(boost::regex_constants::error_stack);]])],
          [ncbi_cv_lib_boost_regex=yes], [ncbi_cv_lib_boost_regex=no])])

   AC_CACHE_CHECK([for Boost.Spirit$in_path], ncbi_cv_lib_boost_spirit,
      CPPFLAGS="$BOOST_INCLUDE $orig_CPPFLAGS"
      LIBS="$RT_LIBS $orig_LIBS"
      [AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([[#include <boost/spirit.hpp>]],
              [[boost::spirit::parse("123", boost::spirit::int_p);]])],
          [ncbi_cv_lib_boost_spirit=yes], [ncbi_cv_lib_boost_spirit=no])])

   BOOST_SYSTEM_LIBS=-lboost_system${BOOST_TAG}
   if test -f "$BOOST_LIBPATH_/libboost_system${BOOST_TAG}-static.a"; then
       BOOST_SYSTEM_STATIC_LIBS=-lboost_system${BOOST_TAG}-static
   else
       BOOST_SYSTEM_STATIC_LIBS=$BOOST_SYSTEM_LIBS
   fi

   AC_CACHE_CHECK([for Boost.System$in_path], ncbi_cv_lib_boost_system,
      CPPFLAGS="$BOOST_INCLUDE $orig_CPPFLAGS"
      LIBS="$BOOST_LIBPATH $BOOST_SYSTEM_LIBS $RT_LIBS $orig_LIBS"
      [AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([[#include <boost/system/error_code.hpp>]],
              [[const char * s = boost::system::system_category().name();]])],
          [ncbi_cv_lib_boost_system=yes],
          [ncbi_cv_lib_boost_system=no
           BOOST_SYSTEM_LIBS=
           BOOST_SYSTEM_STATIC_LIBS=])])

   boost_fs_lib=-lboost_filesystem${BOOST_TAG}
   if test -f "$BOOST_LIBPATH_/libboost_filesystem${BOOST_TAG}-static.a"; then
       boost_fs_static_lib=-lboost_filesystem${BOOST_TAG}-static
   else
       boost_fs_static_lib=$boost_fs_lib
   fi
   BOOST_FILESYSTEM_LIBS="$boost_fs_lib $BOOST_SYSTEM_LIBS"
   BOOST_FILESYSTEM_STATIC_LIBS="$boost_fs_static_lib $BOOST_SYSTEM_STATIC_LIBS"

   AC_CACHE_CHECK([for Boost.Filesystem$in_path], ncbi_cv_lib_boost_filesystem,
      CPPFLAGS="$BOOST_INCLUDE $orig_CPPFLAGS"
      LIBS="$BOOST_LIBPATH $BOOST_FILESYSTEM_LIBS $RT_LIBS $orig_LIBS"
      [AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([[#include <boost/filesystem.hpp>]],
              [[return boost::filesystem::portable_name("foo");]])],
          [ncbi_cv_lib_boost_filesystem=yes],
          [ncbi_cv_lib_boost_filesystem=no])])

   BOOST_TEST_PEM_LIBS=-lboost_prg_exec_monitor${BOOST_TAG}
   if test -f "$BOOST_LIBPATH_/libboost_prg_exec_monitor${BOOST_TAG}-static.a"; then
       BOOST_TEST_PEM_STATIC_LIBS=-lboost_prg_exec_monitor${BOOST_TAG}-static
   else
       BOOST_TEST_PEM_STATIC_LIBS=$BOOST_TEST_PEM_LIBS
   fi

   BOOST_TEST_TEM_LIBS=-lboost_test_exec_monitor${BOOST_TAG}
   if test -f "$BOOST_LIBPATH_/libboost_test_exec_monitor${BOOST_TAG}-static.a"; then
       BOOST_TEST_TEM_STATIC_LIBS=-lboost_test_exec_monitor${BOOST_TAG}-static
   else
       BOOST_TEST_TEM_STATIC_LIBS=$BOOST_TEST_TEM_LIBS
   fi

   BOOST_TEST_UTF_LIBS=-lboost_unit_test_framework${BOOST_TAG}
   if test -f "$BOOST_LIBPATH_/libboost_unit_test_framework${BOOST_TAG}-static.a"; then
       BOOST_TEST_UTF_STATIC_LIBS=-lboost_unit_test_framework${BOOST_TAG}-static
   else
       BOOST_TEST_UTF_STATIC_LIBS=$BOOST_TEST_UTF_LIBS
   dnl AC_DEFINE([NCBI_BOOST_TEST_DLL], 1,
   dnl   [Define to 1 if unable to locate an appropriate
   dnl    libboost_unit_test_framework${BOOST_TAG}-static.a.])
   fi

   BOOST_TEST_PEM_LIBS=$BOOST_TEST_PEM_STATIC_LIBS
   BOOST_TEST_TEM_LIBS=$BOOST_TEST_TEM_STATIC_LIBS
   BOOST_TEST_UTF_LIBS=$BOOST_TEST_UTF_STATIC_LIBS

   AC_CACHE_CHECK([for Boost.Test$in_path], ncbi_cv_lib_boost_test,
      CPPFLAGS="$BOOST_INCLUDE $orig_CPPFLAGS"
      LIBS="$RT_LIBS $orig_LIBS"
      found=
      for mode in included external; do
        if test "$mode" = external; then
          CPPFLAGS="-DNCBI_BOOST_NOT_INCLUDED $CPPFLAGS"
          LIBS="$BOOST_LIBPATH $BOOST_TEST_UTF_LIBS $LIBS"
        fi
        [AC_LINK_IFELSE(
           [AC_LANG_SOURCE(
              [[#ifdef NCBI_BOOST_NOT_INCLUDED
                #  include <boost/test/unit_test.hpp>
                #else
                #  include <boost/test/included/unit_test.hpp>
                #endif
                using boost::unit_test::test_suite;
                static void s_Test1(void) { BOOST_CHECK(1 == 1); }
                test_suite* init_unit_test_suite(int, char*[])
                {
                    test_suite* test = BOOST_TEST_SUITE("foo");
                    test->add(BOOST_TEST_CASE(&s_Test1), 0);
                    return test;
                }]])],
           [found="$found:$mode"])]
      done
      [case "$found" in
        :included:external ) ncbi_cv_lib_boost_test=yes           ;;
        :included          ) ncbi_cv_lib_boost_test=included-only ;;
        :external          ) ncbi_cv_lib_boost_test=external-only ;;
        ''                 ) ncbi_cv_lib_boost_test=no            ;;
      esac])

   BOOST_THREAD_LIBS="-lboost_thread${BOOST_TAG} $BOOST_SYSTEM_LIBS"
   if test -f "$BOOST_LIBPATH_/libboost_thread${BOOST_TAG}-static.a"; then
       BOOST_THREAD_STATIC_LIBS="-lboost_thread${BOOST_TAG}-static $BOOST_SYSTEM_STATIC_LIBS"
   else
       BOOST_THREAD_STATIC_LIBS=$BOOST_THREAD_LIBS
   fi

   AC_CACHE_CHECK([for Boost.Threads$in_path], ncbi_cv_lib_boost_thread,
      CPPFLAGS="$BOOST_INCLUDE $orig_CPPFLAGS"
      LIBS="$BOOST_LIBPATH $BOOST_THREAD_LIBS $RT_LIBS $orig_LIBS"
      [AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([[#include <boost/thread.hpp>]],
              [[boost::mutex m; boost::mutex::scoped_lock l(m);]])],
          [ncbi_cv_lib_boost_thread=yes], [ncbi_cv_lib_boost_thread=no])])
else
   BOOST_INCLUDE=
   BOOST_LIBPATH=
   BOOST_TAG=
   ncbi_cv_lib_boost_filesystem=no
   ncbi_cv_lib_boost_regex=no
   ncbi_cv_lib_boost_spirit=no
   ncbi_cv_lib_boost_system=no
   ncbi_cv_lib_boost_test=no
   ncbi_cv_lib_boost_thread=no
fi

if test "$ncbi_cv_lib_boost_filesystem" != "no"; then
   dnl AC_DEFINE(HAVE_BOOST_FILESYSTEM, 1,
   dnl           [Define to 1 if the `Boost.Filesystem' library is available.])
   NCBI_PACKAGE(Boost.Filesystem)
else
   boost_fs_lib=
   boost_fs_static_lib=
   BOOST_FILESYSTEM_LIBS=
   BOOST_FILESYSTEM_STATIC_LIBS=
fi

if test "$ncbi_cv_lib_boost_regex" != "no"; then
   AC_DEFINE(HAVE_BOOST_REGEX, 1,
             [Define to 1 if the `Boost.Regex' library is available.])
   NCBI_PACKAGE(Boost.Regex)
else
   BOOST_REGEX_LIBS=
   BOOST_REGEX_STATIC_LIBS=
fi

if test "$ncbi_cv_lib_boost_spirit" != "no"; then
   AC_DEFINE(HAVE_BOOST_SPIRIT, 1,
             [Define to 1 if the `Boost.Spirit' headers are available.])
   NCBI_PACKAGE(Boost.Spirit)
fi

if test "$ncbi_cv_lib_boost_system" != "no"; then
   dnl AC_DEFINE(HAVE_BOOST_SYSTEM, 1,
   dnl           [Define to 1 if the `Boost.System' library is available.])
   NCBI_PACKAGE(Boost.System)
# BOOST_SYSTEM(_STATIC)_LIBS already conditionally cleared above
fi

if test "$ncbi_cv_lib_boost_test" != "no"; then
   AC_DEFINE(HAVE_BOOST_TEST, 1,
             [Define to 1 if the `Boost.Test' libraries are available.])
   if test "$ncbi_cv_lib_boost_test" != "included-only"; then
      NCBI_PACKAGE(Boost.Test)
   fi
   if test "$ncbi_cv_lib_boost_test" != "external-only"; then
      NCBI_PACKAGE(Boost.Test.Included)
   fi
else
   BOOST_TEST_PEM_LIBS=
   BOOST_TEST_PEM_STATIC_LIBS=
   BOOST_TEST_TEM_LIBS=
   BOOST_TEST_TEM_STATIC_LIBS=
   BOOST_TEST_UTF_LIBS=
   BOOST_TEST_UTF_STATIC_LIBS=
fi

if test "$ncbi_cv_lib_boost_thread" != "no"; then
   AC_DEFINE(HAVE_BOOST_THREAD, 1,
             [Define to 1 if the `Boost.Threads' library is available.])
   NCBI_PACKAGE(Boost.Threads)
else
   BOOST_THREAD_LIBS=
   BOOST_THREAD_STATIC_LIBS=
fi

case "$with_boost:$ncbi_cv_lib_boost_test" in
   :* | no:* | *:yes | *:included-only ) ;; # no problem
   * ) AC_MSG_ERROR([Boost explicitly requested, but Boost.Test.Included unavailable.]) ;;
esac


## NCBI C Toolkit
if test "$with_ncbi_c" != "no" ; then
   if test "${with_ncbi_c-yes}" != yes; then
      NCBI_C_PATH=$with_ncbi_c
   fi
   : ${NCBI_C_PATH="$NCBI"}
   if test "$ncbi_compiler" = ICC -a -d "$NCBI_C_PATH/ncbi_icc"; then
      NCBI_C_PATH=$NCBI_C_PATH/ncbi_icc
   fi

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
   AC_CACHE_CHECK([for the NCBI C toolkit${NCBI_C_PATH:+ in $NCBI_C_PATH}],
      ncbi_cv_lib_ctools,
      [CPPFLAGS="$NCBI_C_INCLUDE $orig_CPPFLAGS"
       LIBS="$NCBI_C_LIBPATH $NCBI_C_ncbi $NETWORK_LIBS $orig_LIBS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <ncbi.h>]],
          [[ Nlm_Boolean b = Nlm_GetArgs("fake", -1, 0); ]])],
          [ncbi_cv_lib_ctools=yes], [ncbi_cv_lib_ctools=no])])
   if test "$ncbi_cv_lib_ctools" = no; then
      NCBI_MISSING_PACKAGE(ncbi-c)
   fi
fi
if test "$with_ncbi_c" = "no" ; then
   if test "$with_ctools" = "yes"; then
      AC_MSG_ERROR([--with-ctools:  NCBI C Toolkit is missing])
   fi
   NCBI_C_PATH="No_NCBI_CToolkit"
   NCBI_C_INCLUDE=
   NCBI_C_LIBPATH=
   NCBI_C_ncbi=
else
   NCBI_PACKAGE(C-Toolkit)
   AC_DEFINE(HAVE_NCBI_C, 1, [Define to 1 if the NCBI C toolkit is available.])
fi

# X11
LIBS="$orig_LIBS"
CPPFLAGS="$orig_CPPFLAGS"
# Look in /usr/X11R6/lib64 if necessary
xlib=`ls -d /usr/X11R6/lib$bit64_sfx/libX11.* 2>/dev/null | sed -ne 1p`
if test -f "$xlib" -a "${x_libraries-NONE}" = NONE \
  -a "${x_includes-NONE}" = NONE; then
   base=`basename $xlib`
   cmp -s /usr/X11R6/lib/$base $xlib  || \
      cmp -s /usr/lib/$base $xlib  || \
      cmp -s /usr/lib$bit64_sfx/$base $xlib  || \
      x_libraries=/usr/X11R6/lib$bit64_sfx x_includes=/usr/X11R6/include
fi
AC_PATH_XTRA
x_libraries=`echo "$x_libraries" | sed -e "s,^$usr_lib\$,,"`
X_LIBS=`echo "$X_LIBS " | sed -e "$no_usr_lib"`
AC_CHECK_LIB(Xext, XextCreateExtension, Xext="-lXext", Xext=,
   [$X_PRE_LIBS -lX11 $X_EXTRA_LIBS])
AC_CHECK_LIB(Xt, XtMainLoop, Xt="-lXt", Xt=,
   [$Xext $X_PRE_LIBS -lX11 $X_EXTRA_LIBS])
AC_CHECK_LIB(Xmu, XmuMakeAtom, Xmu="-lXmu", Xmu=,
   [$Xt $Xext $X_PRE_LIBS -lX11 $X_EXTRA_LIBS])
LDFLAGS=$orig_LDFLAGS
X_ALL_LIBS="$Xmu $Xt $Xext $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"

## OpenGL
if test "$with_opengl" != "no"; then
   if test "${with_opengl-yes}" != yes; then
      OPENGL_PATH=$with_opengl
   fi
   if test -d "$OPENGL_PATH"; then
      NCBI_FIX_DIR(OPENGL_PATH)
   fi
   case "$OSTYPE" in
      darwin) # Use native interface
         OPENGL_LIBS="-framework AGL -framework OpenGL"
         ;;
      # cygwin) ... ;;
      *) # Default -- assume X-based
         LIBS="$orig_LIBS"
         CPPFLAGS="$orig_CPPFLAGS"
         LDFLAGS="$orig_LDFLAGS $X_LIBS"
         if test -d "$OPENGL_PATH/include/GL"; then
            OPENGL_INCLUDE="-I$OPENGL_PATH/include $X_CFLAGS"
         else
            OPENGL_INCLUDE=$X_CFLAGS
         fi
         if test -n "$OPENGL_PATH" -a -d "$OPENGL_PATH/lib${bit64_sfx}"; then
            NCBI_RPATHIFY(OPENGL_LIBPATH, $OPENGL_PATH/lib${bit64_sfx},
                          [ ]$X_LIBS)
         elif test -f /usr/lib/mesa/libGL.so; then
            NCBI_RPATHIFY(OPENGL_LIBPATH, /usr/lib/mesa, [ ]$X_LIBS)
         else
            OPENGL_LIBPATH=$X_LIBS
         fi
         OPENGL_LIBS="$OPENGL_LIBPATH -lGLU -lGL $X_ALL_LIBS"
         ;;
   esac
   case "${OPENGL_PATH}" in
      */*) where=" in $OPENGL_PATH" ;;
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
   OPENGL_STATIC_LIBS=
   OSMESA_INCLUDE=
   OSMESA_LIBS=
   OSMESA_STATIC_LIBS=
   GLUT_INCLUDE=
   GLUT_LIBS=
   GLEW_INCLUDE=
   GLEW_LIBS=
   GLEW_STATIC_LIBS=
   NCBI_MISSING_PACKAGE(opengl)
else
   NCBI_PACKAGE(OpenGL)
   AC_DEFINE(HAVE_OPENGL, 1, [Define to 1 if you have OpenGL (-lGL).])
   if test -f "$OPENGL_PATH/lib${bit64_sfx}/libGLU-static.a"; then
      OPENGL_STATIC_LIBS="$OPENGL_LIBPATH -lGLU-static -lGL-static $X_ALL_LIBS"
   else
      OPENGL_STATIC_LIBS=$OPENGL_LIBS
   fi
   : ${OSMESA_PATH=$OPENGL_PATH}
   if test "$with_mesa" != "no"; then
      if test "${with_mesa-yes}" != "yes"; then
         OSMESA_PATH=$with_mesa
      fi
      if test -d "$OSMESA_PATH"; then
         OSMESA_INCLUDE=-I$OSMESA_PATH/include
         if test "$OSMESA_PATH" != "$OPENGL_PATH"; then
            NCBI_RPATHIFY(OSMESA_LDFLAGS, $OSMESA_PATH/lib${bit64_sfx})
         fi
      fi
      LDFLAGS="$orig_LDFLAGS $OSMESA_LDFLAGS $OPENGL_LIBPATH"
      AC_CHECK_LIB(OSMesa, OSMesaCreateContext,
         [NCBI_PACKAGE(MESA);
          OSMESA_LIBS="$OSMESA_LDFLAGS $OPENGL_LIBPATH -lOSMesa -lGLU -lGL $X_ALL_LIBS"
          AC_DEFINE(HAVE_LIBOSMESA, 1, [Define to 1 if you have libOSMesa.])],
         [OSMESA_LIBS=],
         $OPENGL_LIBS)
   fi
   if test -z "$OSMESA_LIBS"; then
      NCBI_MISSING_PACKAGE(mesa)
   elif test -f "$OSMESA_PATH/lib${bit64_sfx}/libOSMesa-static.a"; then
      OSMESA_STATIC_LIBS="$OSMESA_LDFLAGS $OPENGL_LIBPATH -lOSMesa-static -lGLU-static -lGL-static $X_ALL_LIBS"
   else
      OSMESA_STATIC_LIBS=$OSMESA_LIBS
   fi
   if test "$with_glut" != "no"; then
      if test "${with_glut-yes}" != "yes"; then
         GLUT_PATH=$with_glut
      fi
      if test -d "GLUT_PATH"; then
         GLUT_INCLUDE=-I$GLUT_PATH/include
         NCBI_RPATHIFY(GLUT_LDFLAGS, $GLUT_PATH/lib${bit64_sfx})
      fi
      LDFLAGS="$orig_LDFLAGS $GLUT_LDFLAGS $OPENGL_LIBPATH"
      AC_CHECK_LIB(glut, glutInit,
         [NCBI_PACKAGE(GLUT);
          GLUT_LIBS="$GLUT_LDFLAGS $OPENGL_LIBPATH -lglut -lGLU -lGL $X_ALL_LIBS"
          AC_DEFINE(HAVE_LIBGLUT, 1, [Define to 1 if you have libglut.])],
         [GLUT_LIBS=],
         $OPENGL_LIBS)
      if test -z "$GLUT_LIBS"; then
         NCBI_MISSING_PACKAGE(glut)
      fi
   fi
   if test "$with_glew" != "no"; then
      if test "${with_glew-yes}" != "yes"; then
         GLEW_PATH=$with_glew
      fi
      if test -d "$GLEW_PATH"; then
         if test -d "$GLEW_PATH/$compiler_vpfx$DEBUG_SFX$bit64_sfx"; then
            GLEW_PATH=$GLEW_PATH/$compiler_vpfx$DEBUG_SFX$bit64_sfx
         elif test -d "$GLEW_PATH/$compiler_pfx$DEBUG_SFX$bit64_sfx"; then
            GLEW_PATH=$GLEW_PATH/$compiler_pfx$DEBUG_SFX$bit64_sfx
         elif test -d "$GLEW_PATH/$DEBUG_SFX$bit64_sfx"; then
            GLEW_PATH=$GLEW_PATH/$DEBUG_SFX$bit64_sfx
         fi
         NCBI_FIX_DIR(GLEW_PATH)
         GLEW_LIBDIR=$GLEW_PATH/lib
         if test -n "$bit64_sfx" -a -d $GLEW_LIBDIR$bit64_sfx; then
            if test -d $GLEW_LIBDIR; then
               for x in $GLEW_LIBDIR/libGLEW*; do
                  if cmp -s "$x" "$GLEW_LIBDIR$bit64_sfx/`basename \"$x\"`"; then
                     :
                  else
                     GLEW_LIBDIR=$GLEW_LIBDIR$bit64_sfx
                     break
                  fi
               done
            else
               GLEW_LIBDIR=$GLEW_LIBDIR$bit64_sfx
            fi
         fi
         glew_config="eval PKG_CONFIG_PATH=\"$GLEW_LIBDIR/pkgconfig\" pkg-config"
      else
         glew_config=pkg-config
      fi
      if pkg-config --version >/dev/null 2>&1; then
         if $glew_config glewmx --exists >/dev/null 2>&1 \
           && test -n "`$glew_config glewmx --libs 2>/dev/null`"; then
            glew_config="$glew_config glewmx"
         elif $glew_config glew --exists >/dev/null 2>&1 \
           && test -n "`$glew_config glew --libs 2>/dev/null`"; then
            glew_config="$glew_config glew"
         else
            glew_config=
         fi
         if test -n "$glew_config"; then
            GLEW_INCLUDE=`$glew_config --cflags | sed 's:/GL *$::'`
            GLEW_LDFLAGS=`$glew_config --libs-only-L`
            glew_libname=`$glew_config --libs-only-l | sed -e 's/-l//'`
         fi
      fi
      if test -z "$glew_libname" -a -d "$GLEW_PATH"; then
         GLEW_INCLUDE=-I$GLEW_PATH/include
         GLEW_LDFLAGS=-L$GLEW_LIBDIR
      fi
      LDFLAGS="$orig_LDFLAGS $GLEW_LDFLAGS"
      LIBS="$OPENGL_LIBS $DL_LIBS $orig_LIBS"
      NCBI_CHECK_LIBS(GLEW, [$glew_libname GLEWmx GLEW], glewGetExtension)
      if test "$ac_cv_search_glewGetExtension" = no; then
         GLEW_INCLUDE=
         GLEW_LIBS=
         NCBI_MISSING_PACKAGE(glew)
         NCBI_MISSING_PACKAGE(glew-mx)
      else
         NCBI_RPATHIFY_OUTPUT_COND(tmp_LIBS, [echo $GLEW_LDFLAGS $GLEW_LIBS])
         GLEW_LIBS=$tmp_LIBS
         NCBI_PACKAGE(GLEW)
         CPPFLAGS="$orig_CPPFLAGS $GLEW_INCLUDE $OPENGL_INCLUDE"
         LIBS="$GLEW_LIBS $OPENGL_LIBS $DL_LIBS $orig_LIBS"
         AC_CACHE_CHECK([for GLEW multi-context (MX) support (recommended)],
            ncbi_cv_lib_glew_mx,
            [AC_LINK_IFELSE([AC_LANG_PROGRAM(
               [[#define GLEW_MX 1
                 #include <GL/glew.h>]],
               [[glewContextInit(NULL);]])],
             [ncbi_cv_lib_glew_mx=yes], [ncbi_cv_lib_glew_mx=no])])
          if test $ncbi_cv_lib_glew_mx = yes; then
             GLEW_INCLUDE="$GLEW_INCLUDE -DGLEW_MX"
          else
             NCBI_MISSING_PACKAGE(glew-mx)
          fi
          : ${GLEW_STATIC_LIBS=$GLEW_LIBDIR/libGLEW-static.a}
          test -f "$GLEW_STATIC_LIBS"  ||  GLEW_STATIC_LIBS=$GLEW_LIBS
      fi
   fi
fi

LDFLAGS=$orig_LDFLAGS


## wxWidgets
# Insist on single-byte builds by default, per various projects' expectations.
: ${with_wxwidgets_ucs=no}
if test "$with_wxwidgets" != "no" ; then
   case "$with_wxwidgets" in
      yes | "" ) ;;
      *        ) WXWIDGETS_PATH=$with_wxwidgets ;;
   esac
   if test -d "$WXWIDGETS_PATH/${compiler_vpfx}${DEBUG_SFX}${mt_sfx}${bit64_sfx}/lib" \
        -a -z "$WXWIDGETS_ARCH_PATH"; then
      WXWIDGETS_ARCH_PATH="$WXWIDGETS_PATH/${compiler_vpfx}${DEBUG_SFX}${mt_sfx}${bit64_sfx}"
   elif test -d "$WXWIDGETS_PATH/${compiler_pfx}${DEBUG_SFX}${mt_sfx}${bit64_sfx}/lib" \
        -a -z "$WXWIDGETS_ARCH_PATH"; then
      WXWIDGETS_ARCH_PATH="$WXWIDGETS_PATH/${compiler_pfx}${DEBUG_SFX}${mt_sfx}${bit64_sfx}"
   else
      WXWIDGETS_ARCH_PATH="$WXWIDGETS_PATH"
   fi
   WXWIDGETS_BINPATH=${WXWIDGETS_BINPATH:="$WXWIDGETS_ARCH_PATH/bin"}
   WXWIDGETS_LIBPATH=${WXWIDGETS_LIBPATH:="$WXWIDGETS_ARCH_PATH/lib"}
   WXWIDGETS_INCLUDE=
   AS_UNSET(wxconf)
   AC_PATH_PROG(wxconf, wx-config, [], $WXWIDGETS_BINPATH:$PATH)
   if test -x "$wxconf"; then
      WXWIDGETS_ARCH_PATH=`$wxconf --exec-prefix`
      if test -n "$with_wxwidgets_ucs"; then
         wxcfflags=--unicode=$with_wxwidgets_ucs
      else
         wxcfflags=
      fi
      for x in `"$wxconf" $wxcfflags --libs --static 2>&AS_MESSAGE_LOG_FD`; do
         case "$x" in
            -L*)
                d=`echo $x | sed -e 's/^-L//'`
                pcd=$d/pkgconfig
                if test -d "$pcd"; then
                   PKG_CONFIG_PATH="${PKG_CONFIG_PATH+$PKG_CONFIG_PATH:}$pcd"
                fi
                ;;
         esac
      done
      export PKG_CONFIG_PATH
   else
      AS_UNSET(wxconf)
   fi
   if test -x "$wxconf" \
     &&  "$wxconf" $wxcfflags --list 2>/dev/null \
       | grep 'Default config is gtk2' >/dev/null; then
      gtkconf="pkg-config gtk+-2.0"
   elif test -x "$WXWIDGETS_BINPATH/gtk-config"; then
      gtkconf=$WXWIDGETS_BINPATH/gtk-config
   elif gtk-config --version >/dev/null 2>&1; then
      gtkconf=gtk-config
   else
      # May work without gtk-config (e.g., on Mac OS X)
      gtkconf=:
   fi
   case "$host_os" in
      darwin* )
         baseflags=""
         basesed='s/-isysroot [[^ ]]*//g; s/-arch [[^ ]]*//g'
         if test -n "$SYSROOT"; then
            deps=`"$wxconf" $wxcfflags --libs --static 2>/dev/null \
                  | sed -e 's/.*\.a//'`
            libsed="$basesed; s/\$/ $deps/"
         else
            libsed=$basesed
         fi
         ;;
      * )
         baseflags=`$gtkconf --cflags 2>/dev/null`
         basesed=$no_usr_lib
         libsed=$basesed
         ;;
   esac
   AC_CACHE_CHECK(
      [for wxWidgets${WXWIDGETS_ARCH_PATH:+ in $WXWIDGETS_ARCH_PATH}],
      ncbi_cv_lib_wxwidgets,
      [if test -x "$wxconf" ; then
          WXWIDGETS_INCLUDE="$baseflags `"$wxconf" $wxcfflags --cflags`"
          NCBI_RPATHIFY_OUTPUT(WXWIDGETS_LIBS, ["$wxconf" $wxcfflags --libs],
             [$libsed; s/ -lm / /g;])

          CPPFLAGS="$WXWIDGETS_INCLUDE $orig_CPPFLAGS"
          LIBS="$WXWIDGETS_LIBS $orig_LIBS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM(
             [[#include <wx/wx.h>]],
             [[ wxExit(); ]])],
             [ncbi_cv_lib_wxwidgets=yes], [ncbi_cv_lib_wxwidgets=no])
       else
          ncbi_cv_lib_wxwidgets=no
       fi])
   if test "$ncbi_cv_lib_wxwidgets" = "no"; then
      NCBI_MISSING_PACKAGE(wxwidgets)
   fi
fi

if test "$with_wxwidgets" = "no" ; then
   WXWIDGETS_LIBPATH="No_wxWidgets"
   WXWIDGETS_INCLUDE=
   WXWIDGETS_LIBS=
   WXWIDGETS_STATIC_LIBS=
   WXWIDGETS_GL_LIBS=
   WXWIDGETS_GL_STATIC_LIBS=
   WXWIDGETS_POST_LINK=:
else
   NCBI_PACKAGE(wxWidgets)
   case "`"$wxconf" $wxcfflags --version`" in
      2.[[89]].* | 2.[[0-9]][[0-9]]* )
         NCBI_PACKAGE(wx2.8)
         wxlibs=std,richtext,aui
         ;;
      *)
         wxlibs=std
         ;;
   esac
   # The "yes" may have been cached; get the actual settings again if needed
   if test -z "$WXWIDGETS_INCLUDE"; then
      WXWIDGETS_INCLUDE="$baseflags `"$wxconf" $wxcfflags --cflags`"
   fi
   NCBI_RPATHIFY_OUTPUT(WXWIDGETS_LIBS, [$wxconf $wxcfflags --libs $wxlibs],
      [$libsed; s/ -lm / /g;])
   WXWIDGETS_STATIC_LIBS=`"$wxconf" $wxcfflags --libs --static $wxlibs \
      2>/dev/null | sed -e "$basesed; s/ -lm / /g;"`
   if test -n "$WXWIDGETS_STATIC_LIBS"; then
      # Allow direct use of underlying libraries with strict linkers
      WXWIDGETS_LIBS="$WXWIDGETS_LIBS `echo $WXWIDGETS_STATIC_LIBS | sed -e 's/.*\.a *//'`"
   else
      WXWIDGETS_STATIC_LIBS=$WXWIDGETS_LIBS
   fi
   WXWIDGETS_GL_LIBS=`"$wxconf" $wxcfflags --libs gl | sed -e "$basesed"`
   WXWIDGETS_GL_STATIC_LIBS=`"$wxconf" $wxcfflags --libs gl --static \
      2>/dev/null | sed -e "$basesed; s|-lGLU* -lGLU*|$OPENGL_STATIC_LIBS|"`
   : ${WXWIDGETS_GL_STATIC_LIBS:=$WXWIDGETS_GL_LIBS}
   WXWIDGETS_POST_LINK=`"$wxconf" $wxcfflags --rezflags 2>/dev/null`
   test -n "$WXWIDGETS_POST_LINK"  ||  WXWIDGETS_POST_LINK=:
   AC_DEFINE(HAVE_WXWIDGETS, 1, [Define to 1 if wxWidgets is available.])
fi


## In-house Fast-CGI library
if test "$with_fastcgi" != "no" ; then
   case "$with_fastcgi" in
      yes | "" ) ;;
      */*      ) FASTCGI_PATH=$with_fastcgi              ;;
      *        ) FASTCGI_PATH=$NCBI/fcgi-${with_fastcgi} ;;
   esac
   if test -d "$FASTCGI_PATH"; then
      NCBI_FIX_DIR(FASTCGI_PATH)
   fi
   FASTCGI_INCLUDE="-I$FASTCGI_PATH/include${bit64_sfx}"
   if test "$with_debug" = "no" ; then
      FASTCGI_LIBDIR="$FASTCGI_PATH/lib${bit64_sfx}"
   else
      FASTCGI_LIBDIR="$FASTCGI_PATH/altlib${bit64_sfx}"
   fi
   NCBI_RPATHIFY(FASTCGI_LIBS, $FASTCGI_LIBDIR, [ -lfcgi $NETWORK_LIBS])
   FASTCGI_OBJS="fcgibuf"
   LIBS="$FASTCGI_LIBS $orig_LIBS"
   AC_CACHE_CHECK([for FastCGI libraries${FASTCGI_PATH:+ in $FASTCGI_PATH}],
      ncbi_cv_lib_fcgi,
      [CPPFLAGS="$FASTCGI_INCLUDE $orig_CPPFLAGS"
       AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <fcgiapp.h>]],
          [[ (void) FCGX_IsCGI(); ]])],
          [ncbi_cv_lib_fcgi=yes], [ncbi_cv_lib_fcgi=no])])
   if test "$ncbi_cv_lib_fcgi" = "no"; then
      NCBI_MISSING_PACKAGE(fastcgi)
   fi
   AC_CHECK_FUNCS(FCGX_Accept_r)
fi
if test "$with_fastcgi" = "no" ; then
   FASTCGI_PATH="No_FastCgi"
   FASTCGI_INCLUDE=
   FASTCGI_LIBS=
   FASTCGI_OBJS=
else
   NCBI_PACKAGE(Fast-CGI)
   AC_DEFINE(HAVE_LIBFASTCGI, 1,
             [Define to 1 if FastCGI libraries are available.])
fi


## NCBI SSS libraries
if test "$with_sybase" = "no" ; then
   NCBI_MISSING_PACKAGE(sssdb)
fi

case "$host_os:$compiler:$compiler_version" in
   linux*:GCC:*)
      : ${with_included_sss=yes}
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
      NCBI_PACKAGE(LocalSSS)
      if test -f "$local_sss/../msgmail2/asn/msgmail.asn"; then
         NCBI_PACKAGE(LocalMSGMAIL2)
      fi
   else
      with_included_sss=no
   fi
   if test "${with_sss-yes}" = yes; then
      NCBI_SSS_PATH=${NCBI_SSS_PATH:="$NCBI/sss/BUILD"}
   else
      NCBI_SSS_PATH=$with_sss
   fi
   NCBI_SSS_INCLUDE=${NCBI_SSS_INCLUDE:="$NCBI_SSS_PATH/include"}
   if test -z "$NCBI_SSS_LIBPATH" ; then
      NCBI_SSS_LIBPATH="${NCBI_SSS_PATH}/lib/${compiler_vpfx}${DEBUG_SFX}${bit64_sfx}"
      if test "$with_mt" = "yes" ; then
         NCBI_SSS_LIBPATH="${NCBI_SSS_LIBPATH}mt"
      fi
   fi
   AC_MSG_CHECKING(
      [for NCBI SSS directories${NCBI_SSS_PATH:+ in $NCBI_SSS_PATH}])
   if test "$with_included_sss" = "yes"; then
      AC_MSG_RESULT([yes]) # duh
   elif test ! -d "${NCBI_SSS_LIBPATH}"  -o  ! -d "${NCBI_SSS_INCLUDE}" ; then
      AC_MSG_RESULT([no])
      NCBI_MISSING_PACKAGE(sss)
   else
      AC_MSG_RESULT([yes])
   fi
fi

if test "$with_sss" = "no" ; then
   NCBI_MISSING_PACKAGE(sssutils)
   NCBI_MISSING_PACKAGE(sssdb)
   NCBI_SSS_INCLUDE=
   NCBI_SSS_LIBPATH=
   LIBSSSUTILS=
   LIBSSSDB=
else
   if test "$with_included_sss" = "yes"; then
      NCBI_SSS_LIBPATH= # Not needed
      ncbi_cv_lib_sssutils=yes
      ncbi_cv_lib_sssdb=yes
      sssutils=sssutils
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
         NCBI_PACKAGE(SSSUTILS)
         if test -f "$local_sss/../ncbils2/asn/login.asn"; then
            if test "$with_included_sss" = yes  || \
               grep vformat $NCBI_SSS_INCLUDE/String.hpp >/dev/null 2>&1; then
               NCBI_PACKAGE(LocalNCBILS) # temporary compatibility measure
               NCBI_PACKAGE(NCBILS2)
            fi
         fi
      else
         NCBI_MISSING_PACKAGE(sssutils)
         NCBI_MISSING_PACKAGE(sss)
         LIBSSSUTILS=""
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
         NCBI_PACKAGE(SSSDB)
      else
         NCBI_MISSING_PACKAGE(sssdb)
         NCBI_MISSING_PACKAGE(sss)
         LIBSSSDB=""
      fi
   fi
fi


# SP
if test "$with_sp" != "no" ; then
   if test -n "$SP_INCLUDE"; then
      SP_GENERIC="$SP_INCLUDE/../generic"
   else
      SP_INCLUDE="$SP_PATH/include"
      SP_GENERIC="$SP_PATH/generic"
   fi
   if test -z "$SP_LIBPATH" ; then
      SP_LIBPATH="${SP_PATH}/${compiler_vpfx}${DEBUG_SFX}${mt_sfx}${bit64_sfx}"
   fi
   AC_MSG_CHECKING([for SP directories${SP_PATH:+ in $SP_PATH}])
   if test ! -d "${SP_LIBPATH}"  -o  ! -d "${SP_INCLUDE}" ; then
      AC_MSG_RESULT([no])
      NCBI_MISSING_PACKAGE(sp)
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
      NCBI_MISSING_PACKAGE(sp)
   fi
fi

if test "$with_sp" = "no" ; then
   SP_INCLUDE=
   SP_LIBS=
else
   NCBI_PACKAGE(SP)
   AC_DEFINE(HAVE_LIBSP, 1, [Define to 1 if the SP SGML library is available.])
fi


## ORBacus CORBA
if test "$with_orbacus" != no; then
   if test "$with_orbacus" != yes; then
       ORBACUS_PATH=$with_orbacus
   fi
   if test -d "$ORBACUS_PATH"; then
      NCBI_FIX_DIR(ORBACUS_PATH)
   fi
   fullpath=${ORBACUS_PATH}/${DEBUG_SFX}${mt_sfx}${bit64_sfx}
   if test -f ${fullpath}/inc/OB/Config.h ; then
      : ${ORBACUS_INCLUDE="-I$ORBACUS_PATH/include -I$fullpath/inc"}
      NCBI_RPATHIFY_COND(ORBACUS_LIBPATH, $fullpath/lib)
   elif test -f ${ORBACUS_PATH}/include/OB/Config.h ; then
     : ${ORBACUS_INCLUDE="-I$ORBACUS_PATH/include"}
     NCBI_RPATHIFY_COND(ORBACUS_LIBPATH, $ORBACUS_PATH/lib)
   fi
   case "$host_os:$with_mt" in
       linux*:no) LIBJTC="-lJTC -pthread" ;;
       *)         LIBJTC="-lJTC"          ;;
   esac

   case "$with_mt:$ORBACUS_LIBPATH" in
      yes:* | *:-L*MT/lib* )
         LIBOB="-lOB $LIBJTC"
         LIBIMR="-lIMR"
         ;;
      * )
         LIBOB="-lOB"
         LIBIMR=
         ;;
   esac
   AC_CACHE_CHECK([for ORBacus${ORBACUS_PATH:+ in $ORBACUS_PATH}],
      ncbi_cv_lib_orbacus,
      [CPPFLAGS="$ORBACUS_INCLUDE $orig_CPPFLAGS"
       LIBS="$ORBACUS_LIBPATH $LIBOB $NETWORK_LIBS $DL_LIBS $orig_LIBS"
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
      NCBI_MISSING_PACKAGE(orbacus)
   fi
fi
if test "$with_orbacus" = no; then
   ORBACUS_PATH=No_ORBacus
   ORBACUS_INCLUDE=
   ORBACUS_LIBPATH=
   LIBOB=
   LIBIMR=
else
   NCBI_PACKAGE(ORBacus)
   AC_DEFINE(HAVE_ORBACUS, 1,
             [Define to 1 if the ORBacus CORBA package is available.])
fi
# Sigh... without this, the linker pulls in the contents of the cache
# and then gripes about missing JTC stuff.
test -d SunWS_cache  &&  rm -r SunWS_cache


## International Components for Unicode (ICU)
if test -z "$ICU_PATH"  &&  icu-config --version >/dev/null 2>&1; then
    ICU_PATH=`icu-config --prefix`
fi

if test "$with_icu" != "no" ; then
   case "$with_icu" in
      yes | "" ) ;;
      *        ) ICU_PATH=$with_icu ;;
   esac
   if test -d "$ICU_PATH/${compiler_vpfx}${DEBUG_SFX}${bit64_sfx}${mt_sfx}/lib" \
        -a -z "$ICU_ARCH_PATH"; then
      ICU_ARCH_PATH="$ICU_PATH/${compiler_vpfx}${DEBUG_SFX}${bit64_sfx}${mt_sfx}"
   elif test -d "$ICU_PATH/${compiler_pfx}${DEBUG_SFX}${bit64_sfx}${mt_sfx}/lib" \
        -a -z "$ICU_ARCH_PATH"; then
      ICU_ARCH_PATH="$ICU_PATH/${compiler_pfx}${DEBUG_SFX}${bit64_sfx}${mt_sfx}"
   else
      ICU_ARCH_PATH="$ICU_PATH"
   fi
   ICU_BINPATH=${ICU_BINPATH:="$ICU_ARCH_PATH/bin"}
   ICU_LIBPATH=${ICU_LIBPATH:="$ICU_ARCH_PATH/lib"}
   ICU_INCLUDE=
   AC_CACHE_CHECK([for ICU${ICU_ARCH_PATH:+ in $ICU_ARCH_PATH}],
      ncbi_cv_lib_icu,
      [ICU_CONFIG=`$ICU_BINPATH/icu-config --bindir 2>/dev/null`/icu-config
       if test -x "$ICU_CONFIG" ; then
          ICU_INCLUDE=`$ICU_CONFIG --cppflags-searchpath`
          NCBI_RPATHIFY_OUTPUT(ICU_LIBS, $ICU_CONFIG --ldflags-searchpath)
          ICU_LIBS="$ICU_LIBS `$ICU_CONFIG --ldflags-libsonly`"

          CPPFLAGS="$ICU_INCLUDE $orig_CPPFLAGS"
          LIBS="$ICU_LIBS $orig_LIBS"
          AC_LINK_IFELSE([AC_LANG_PROGRAM(
            [[#include <unicode/unistr.h>]],
            [[int32_t i = UNICODE_STRING_SIMPLE("foo").indexOf('f');]])],
            [ncbi_cv_lib_icu=yes], [ncbi_cv_lib_icu=no])
       else
          ncbi_cv_lib_icu=no
       fi])
   if test "$ncbi_cv_lib_icu" = "no"; then
      NCBI_MISSING_PACKAGE(icu)
   fi
fi

if test "$with_icu" = "no" ; then
   ICU_PATH="No_ICU"
   ICU_INCLUDE=
   ICU_LIBS=
   ICU_STATIC_LIBS=
else
   NCBI_PACKAGE(ICU)
   ICU_CONFIG="`$ICU_BINPATH/icu-config --bindir`/icu-config"
   # The "yes" may have been cached; get the actual settings again if needed
   if test -z "$ICU_INCLUDE"; then
      ICU_INCLUDE=`$ICU_CONFIG --cppflags-searchpath`
      NCBI_RPATHIFY_OUTPUT(ICU_LIBS, $ICU_CONFIG --ldflags-searchpath)
      ICU_LIBS="$ICU_LIBS `$ICU_CONFIG --ldflags-libsonly`"
   fi
   ICU_STATIC_LIBS=`$ICU_CONFIG --ldflags-libsonly | sed -e 's/-licu/-lsicu/g'`
   ICU_STATIC_LIBS="`$ICU_CONFIG --ldflags-searchpath` $ICU_STATIC_LIBS"
   AC_DEFINE(HAVE_ICU, 1, [Define to 1 if ICU libraries are available.])
fi


### XML/XSL libraries
NCBI_CHECK_THIRD_PARTY_LIB(expat,
 AC_LANG_PROGRAM([#include <expat.h>],
    [XML_Parser parser = XML_ParserCreate("utf-8");]))
if test -f "${EXPAT_PATH}/lib${bit64_sfx}/libexpat-static.a"; then
   EXPAT_STATIC_LIBS="-L${EXPAT_PATH}/lib${bit64_sfx} -lexpat-static"
else
   EXPAT_STATIC_LIBS=${EXPAT_LIBS}
fi
vpath="$SABLOT_PATH/${compiler_vpfx}build"
test -d "$vpath"  &&  SABLOT_PATH=$vpath
if test -d "$SABLOT_PATH"; then
   NCBI_FIX_DIR(SABLOT_PATH)
fi
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

# test -d "$LIBXML_PATH" || LIBXML_PATH=`xml2-config --prefix 2>/dev/null`
if test "$with_libxml" != "no"; then
   case "$with_libxml" in
      yes | "" ) ;;
      *        ) LIBXML_PATH=$with_libxml ;;
   esac
   for pfx in "$compiler_vpfx" "$compiler_pfx" "" ; do
      vpath=$LIBXML_PATH/$pfx${DEBUG_SFX}${mt_sfx}${bit64_sfx}
      if test -d "$vpath" ; then
         LIBXML_PATH=$vpath
         break
      fi
   done
   if test -d "$LIBXML_PATH"; then
      NCBI_FIX_DIR(LIBXML_PATH)
      in_path=" in $LIBXML_PATH"
      if test -n "$LIBXML_INCLUDE"; then
         :
      elif test -d "$LIBXML_PATH/include/libxml2/libxml"; then
         LIBXML_INCLUDE="-I$LIBXML_PATH/include/libxml2 -I$LIBXML_PATH/include"
      elif test -d "$LIBXML_PATH/include/libxml"; then
         LIBXML_INCLUDE="-I$LIBXML_PATH/include";
      fi
      if test -d "$LIBXML_PATH/lib${bit64_sfx}"; then
         LIBXML_LIBPATH_="$LIBXML_PATH/lib${bit64_sfx}"
      elif test -d "$LIBXML_PATH/lib"; then
         LIBXML_LIBPATH_="$LIBXML_PATH/lib"
      fi
      NCBI_RPATHIFY_COND(LIBXML_LIBPATH, $LIBXML_LIBPATH_)
      LIBXML_LIBS="$LIBXML_LIBPATH -lxml2"
   else
      LIBXML_INCLUDE="-I/usr/include/libxml2"
      LIBXML_LIBS="-lxml2"
      in_path=
   fi
   LIBXML_LIBS="$LIBXML_LIBS $ICONV_LIBS"
   AC_CACHE_CHECK([for libxml2$in_path], ncbi_cv_lib_libxml,
      CPPFLAGS="$LIBXML_INCLUDE $orig_CPPFLAGS"
      LIBS="$LIBXML_LIBS $orig_LIBS"
      [AC_LINK_IFELSE(
         [AC_LANG_PROGRAM(
             [#include <libxml/parser.h>],
             [xmlDocPtr doc = xmlParseFile("foo.xml");])],
      [ncbi_cv_lib_libxml=yes], [ncbi_cv_lib_libxml=no])])
   if test "$ncbi_cv_lib_libxml" = "no"; then
      NCBI_MISSING_PACKAGE(libxml)
   fi
fi
if test "$with_libxml" = "no"; then
   LIBXML_PATH="No_LIBXML"
   LIBXML_INCLUDE=
   LIBXML_LIBPATH_=
   LIBXML_LIBS=
else
   NCBI_PACKAGE(LIBXML)
   LIBXML_INCLUDE="$LIBXML_INCLUDE"
   AC_DEFINE(HAVE_LIBXML, 1, [Define to 1 if libxml2 is available.])
fi
AC_SUBST(LIBXML_INCLUDE)
AC_SUBST(LIBXML_LIBS)

if test -f "${LIBXML_LIBPATH_}/libxml2-static.a"; then
   LIBXML_STATIC_LIBS="-L${LIBXML_LIBPATH_} -lxml2-static"
else
   LIBXML_STATIC_LIBS=${LIBXML_LIBS}
fi

: ${LIBXSLT_PATH=$LIBXML_PATH}
NCBI_CHECK_THIRD_PARTY_LIB_EX(libxslt, LIBXSLT, xslt,
 [AC_LANG_PROGRAM([#include <libxslt/xslt.h>], [xsltInit()])],
 [], [$LIBXML_LIBS $Z_LIBS], [$LIBXML_INCLUDE])

# XXX - they might be in different directories in some setups
if test -f "${LIBXML_LIBPATH_}/libxslt-static.a"; then
   LIBXSLT_STATIC_LIBS="-L${LIBXML_LIBPATH_} -lxslt-static"
else
   LIBXSLT_STATIC_LIBS=${LIBXSLT_LIBS}
fi

AC_PATH_PROG(XSLTPROC, xsltproc, :,
   [$LIBXSLT_PATH/bin:`dirname $LIBXML_LIBPATH_ 2>/dev/null`/bin:$PATH])

: ${LIBEXSLT_PATH=$LIBXSLT_PATH}
NCBI_CHECK_THIRD_PARTY_LIB_EX(libexslt, LIBEXSLT, exslt,
 [AC_LANG_PROGRAM([#include <libexslt/exslt.h>], [exsltRegisterAll()])],
 [], [$LIBXSLT_LIBS $LIBXML_LIBS $Z_LIBS], [$LIBXSLT_INCLUDE])

# XXX - they might be in different directories in some setups
if test -f "${LIBXML_LIBPATH_}/libexslt-static.a"; then
   LIBEXSLT_STATIC_LIBS="-L${LIBXML_LIBPATH_} -lexslt-static"
else
   LIBEXSLT_STATIC_LIBS=${LIBEXSLT_LIBS}
fi

if test "$with_xerces" != no; then
   if test "${with_xerces-yes}" != yes; then
      XERCES_PATH=$with_xerces
   fi
   if test -d "$XERCES_PATH"; then
      NCBI_FIX_DIR(XERCES_PATH)
   fi
   vpath="$XERCES_PATH/${compiler_vpfx}${DEBUG_SFX}${bit64_sfx}${mt_sfx}"
   if test -d "$vpath/lib"; then
      XERCES_PATH=$vpath
      NCBI_FIX_DIR(XERCES_PATH)
   elif test -d "$vpath"; then
      : ${XERCES_LIBPATH=$vpath}
      NCBI_FIX_DIR(XERCES_LIBPATH)
   else
      vpath="$XERCES_PATH/${compiler_pfx}${DEBUG_SFX}${bit64_sfx}${mt_sfx}"
      if test -d "$vpath/lib"; then
         XERCES_PATH=$vpath
         NCBI_FIX_DIR(XERCES_PATH)
      elif test -d "$vpath"; then
         XERCES_LIBPATH=$vpath
         NCBI_FIX_DIR(XERCES_LIBPATH)
      fi
   fi
   if test -d "$XERCES_PATH"; then
      in_path=" in $XERCES_PATH"
      : ${XERCES_INCLUDE=-I$XERCES_PATH/include}
      : ${XERCES_LIBPATH=$XERCES_PATH/lib}
   else
      in_path=''
   fi
   NCBI_RPATHIFY(XERCES_LIBPATH_, $XERCES_LIBPATH)
   XERCES_LIBS="$XERCES_LIBPATH_ -lxerces-c"
   AC_CACHE_CHECK([for Xerces-C++$in_path], ncbi_cv_lib_xerces_c,
      [CPPFLAGS="$XERCES_INCLUDE $orig_CPPFLAGS"
       LIBS="$XERCES_LIBS $ICU_LIBS $orig_LIBS"
       AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([[#include <xercesc/dom/DOM.hpp>]],
             [[xercesc::DOMImplementation* impl
               = xercesc::DOMImplementationRegistry::getDOMImplementation
                 (xercesc::XMLString::transcode("XML 1.0"));]])],
          ncbi_cv_lib_xerces_c=yes, ncbi_cv_lib_xerces_c=no)])
   if test $ncbi_cv_lib_xerces_c = yes; then
      if test -f "$XERCES_LIBPATH/libxerces-c-static.a"; then
         XERCES_STATIC_LIBS="-L$XERCES_LIBPATH -lxerces-c-static $CURL_LIBS"
      else
         XERCES_STATIC_LIBS=$XERCES_LIBS
      fi
   else
      NCBI_MISSING_PACKAGE(xerces)
   fi
fi
if test "$with_xerces" != no; then
   NCBI_PACKAGE(Xerces)
   AC_DEFINE(HAVE_XERCES, 1, [Define to 1 if Xerces-C++ is available.])
else
   XERCES_INCLUDE=
   XERCES_LIBS=
   XERCES_STATIC_LIBS=
fi

if test "$with_xalan" != no; then
   if test "${with_xalan-yes}" != yes; then
      XALAN_PATH=$with_xalan
   fi
   if test -d "$XALAN_PATH"; then
      NCBI_FIX_DIR(XALAN_PATH)
   fi
   vpath="$XALAN_PATH/${compiler_vpfx}${DEBUG_SFX}${bit64_sfx}${mt_sfx}"
   if test -d "$vpath/lib"; then
      XALAN_PATH=$vpath
      NCBI_FIX_DIR(XALAN_PATH)
   elif test -d "$vpath"; then
      : ${XALAN_LIBPATH=$vpath}
      NCBI_FIX_DIR(XALAN_LIBPATH)
   else
      vpath="$XALAN_PATH/${compiler_pfx}${DEBUG_SFX}${bit64_sfx}${mt_sfx}"
      if test -d "$vpath/lib"; then
         XALAN_PATH=$vpath
         NCBI_FIX_DIR(XALAN_PATH)
      elif test -d "$vpath"; then
         XALAN_LIBPATH=$vpath
         NCBI_FIX_DIR(XALAN_LIBPATH)
      fi
   fi
   if test -d "$XALAN_PATH"; then
      in_path=" in $XALAN_PATH"
      : ${XALAN_INCLUDE=-I$XALAN_PATH/include}
      : ${XALAN_LIBPATH=$XALAN_PATH/lib}
   else
      in_path=''
   fi
   NCBI_RPATHIFY(XALAN_LIBPATH_, $XALAN_LIBPATH)
   XALAN_LIBS="$XALAN_LIBPATH_ -lxalan-c -lxalanMsg"
   AC_CACHE_CHECK([for Xalan-C++$in_path], ncbi_cv_lib_xalan_c,
      [CPPFLAGS="$XALAN_INCLUDE $XERCES_INCLUDE $orig_CPPFLAGS"
       LIBS="$XALAN_LIBS $XERCES_LIBS $ICU_LIBS $orig_LIBS"
       AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([[#include <xalanc/XalanTransformer/XalanTransformer.hpp>]],
             [[xalanc::XalanTransformer::initialize();]])],
          ncbi_cv_lib_xalan_c=yes, ncbi_cv_lib_xalan_c=no)])
   if test $ncbi_cv_lib_xalan_c = yes; then
      if test -f "$XALAN_LIBPATH/libxalan-c-static.a"; then
         XALAN_STATIC_LIBS="-L$XALAN_LIBPATH -lxalan-c-static -lxalanMsg-static"
      else
         XALAN_STATIC_LIBS=$XALAN_LIBS
      fi
   else
      NCBI_MISSING_PACKAGE(xalan)
   fi
fi
if test "$with_xalan" != no; then
   NCBI_PACKAGE(Xalan)
   AC_DEFINE(HAVE_XALAN, 1, [Define to 1 if Xalan-C++ is available.])
else
   XALAN_INCLUDE=
   XALAN_LIBS=
   XALAN_STATIC_LIBS=
fi

test -d SunWS_cache  &&  rm -r SunWS_cache

if test "$with_zorba" != no; then
   if test "${with_zorba-yes}" != yes; then
      ZORBA_PATH=$with_zorba
   fi
   if test -d "$ZORBA_PATH"; then
      NCBI_FIX_DIR(ZORBA_PATH)
   fi
   vpath="$ZORBA_PATH/${compiler_vpfx}${DEBUG_SFX}${mt_sfx}${bit64_sfx}"
   if test -d "$vpath/lib"; then
      ZORBA_PATH=$vpath
      NCBI_FIX_DIR(ZORBA_PATH)
   elif test -d "$vpath"; then
      : ${ZORBA_LIBPATH=$vpath}
      NCBI_FIX_DIR(ZORBA_LIBPATH)
   else
      vpath="$ZORBA_PATH/${compiler_pfx}${DEBUG_SFX}${mt_sfx}${bit64_sfx}"
      if test -d "$vpath/lib"; then
         ZORBA_PATH=$vpath
         NCBI_FIX_DIR(ZORBA_PATH)
      elif test -d "$vpath"; then
         ZORBA_LIBPATH=$vpath
         NCBI_FIX_DIR(ZORBA_LIBPATH)
      fi
   fi
   ZORBA_CONFIG="$ZORBA_PATH/bin/zorba-config"
   if test -x "$ZORBA_CONFIG"; then
      ZORBA_PATH=`$ZORBA_CONFIG --prefix`
      : ${ZORBA_INCLUDE=`$ZORBA_CONFIG --cppflags`}
   fi
   if test -d "$ZORBA_PATH"; then
      in_path=" in $ZORBA_PATH"
      : ${ZORBA_INCLUDE=-I$ZORBA_PATH/include}
      : ${ZORBA_LIBPATH=$ZORBA_PATH/lib}
   else
      in_path=''
   fi
   if test -x "$ZORBA_CONFIG"; then
      ZORBA_LIBS=`$ZORBA_CONFIG --libs`
   else
      NCBI_RPATHIFY(ZORBA_LIBPATH_, $ZORBA_LIBPATH)
      ZORBA_LIBS="$ZORBA_LIBPATH_ -lzorba_simplestore"
   fi
   AC_CACHE_CHECK([for Zorba$in_path], ncbi_cv_lib_zorba,
      [CPPFLAGS="$ZORBA_INCLUDE $orig_CPPFLAGS"
       LIBS="$ZORBA_LIBS $orig_LIBS"
       AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([[#include <zorba/zorba.h>]],
             [[zorba::Version v = zorba::Zorba::version();]])],
          ncbi_cv_lib_zorba=yes, ncbi_cv_lib_zorba=no)])
   if test $ncbi_cv_lib_zorba = yes; then
      if test -x "$ZORBA_CONFIG"; then
         ZORBA_STATIC_LIBS=`$ZORBA_CONFIG --static-libs`
      elif test -f "$ZORBA_LIBPATH/libzorba_simplestore-static.a"; then
         ZORBA_STATIC_LIBS="-L$ZORBA_LIBPATH -lzorba_simplestore-static $XERCES_STATIC_LIBS $ICU_STATIC_LIBS $LIBXSLT_STATIC_LIBS $LIBXML_STATIC_LIBS"
      else
         ZORBA_STATIC_LIBS=$ZORBA_LIBS
      fi
   else
      NCBI_MISSING_PACKAGE(zorba)
   fi
fi
if test "$with_zorba" != no; then
   NCBI_PACKAGE(Zorba)
   AC_DEFINE(HAVE_ZORBA, 1, [Define to 1 if Zorba is available.])
else
   ZORBA_INCLUDE=
   ZORBA_LIBS=
   ZORBA_STATIC_LIBS=
fi

### SQLite
case "$with_sqlite3" in
   yes | '' ) ;;
   *        ) SQLITE3_PATH=$with_sqlite3 ;;
esac

if test -d "$SQLITE3_PATH"; then
   NCBI_FIX_DIR(SQLITE3_PATH)
fi
if test -d "$SQLITE3_PATH/${compiler_pfx}${DEBUG_SFX}${MT_SFX}${bit64_sfx}/lib"; then
   SQLITE3_LIBDIR=$SQLITE3_PATH/${compiler_pfx}${DEBUG_SFX}${MT_SFX}${bit64_sfx}/lib
elif test -d $SQLITE3_PATH/lib${bit64_sfx}; then
   SQLITE3_LIBDIR=$SQLITE3_PATH/lib${bit64_sfx}
else
   SQLITE3_LIBDIR=$SQLITE3_PATH/lib
fi
NCBI_RPATHIFY(SQLITE3_LIBPATH, $SQLITE3_LIBDIR)
NCBI_CHECK_THIRD_PARTY_LIB(sqlite3,
 AC_LANG_PROGRAM([#include <sqlite3.h>],
    [sqlite3_pcache_methods m;
     int status = sqlite3_config(SQLITE_CONFIG_GETPCACHE, &m);]))
if test -n "$SQLITE3_LIBS"; then
   CPPFLAGS="$SQLITE3_INCLUDE $orig_CPPFLAGS"
   AC_CHECK_HEADER(sqlite3async.h,
      [AC_DEFINE(HAVE_SQLITE3ASYNC_H, 1,
          [Define to 1 if you have the <sqlite3async.h> header file.])
       NCBI_PACKAGE(SQLITE3ASYNC)])
   LIBS="$SQLITE3_LIBS $orig_LIBS"
   AC_CHECK_FUNCS(sqlite3_unlock_notify)
fi
if test -n "$SQLITE3_LIBS" -a -f "$SQLITE3_LIBDIR/libsqlite3-static.a"; then
   SQLITE3_STATIC_LIBS="-L$SQLITE3_LIBDIR -lsqlite3-static"
else
   SQLITE3_STATIC_LIBS=$SQLITE3_LIBS
fi


### OEChem
# somewhat kludgish, as we now wanto to add in oeiupac and oedepict,
# which depend on oechem....
NCBI_CHECK_THIRD_PARTY_LIB(oechem,
 AC_LANG_PROGRAM([#include <oechem.h>],
    [OEChem::OEMol mol; OEChem::OEConfBase* c = mol.GetActive();]),
  -loeiupac -loedepict -loechem -loesystem -loeplatform -lz, $NETWORK_LIBS)
if test -n "$OECHEM_LIBS"; then
   OECHEM_LIBS=`echo $OECHEM_LIBS | sed -e 's/-loechem \(.*-loechem .*\)/\1/'`
fi
test -d SunWS_cache  &&  rm -r SunWS_cache


### Sun/Univa Grid Engine
case "$with_sge" in
   yes | "" ) ;;
   *        ) SGE_PATH=$with_sge ;;
esac
if test -d "$SGE_PATH" -a -z "$SGE_LIBPATH"; then
   case "$host:$NCBI_PLATFORM_BITS" in
      i?86-*-darwin*      ) SGE_PLATFORMS='darwin-x86'          ;;
      i?86-*-linux*       ) SGE_PLATFORMS='lx24-x86 lx-x86'     ;;
      i?86-*-solaris*:32  ) SGE_PLATFORMS='sol-x86'             ;;
      i?86-*-solaris*:64  ) SGE_PLATFORMS='sol-amd64'           ;;
      powerpc-*-darwin*   ) SGE_PLATFORMS='darwin-ppc'          ;;
      sparc-*-solaris*:32 ) SGE_PLATFORMS='sol-sparc'           ;;
      sparc-*-solaris*:64 ) SGE_PLATFORMS='sol-sparc64'         ;;
      x86_64-*-linux*     ) SGE_PLATFORMS='lx24-amd64 lx-amd64' ;;
   esac
   for x in $SGE_PLATFORMS; do
      if test -d "$SGE_PATH/lib/$x"; then
         SGE_LIBPATH="$SGE_PATH/lib/$x"
         break
      fi
   done
   NCBI_RPATHIFY(SGE_LIBPATH, $SGE_LIBPATH)
fi
NCBI_CHECK_THIRD_PARTY_LIB_EX(sge, SGE, drmaa,
  [[AC_LANG_PROGRAM([[#include <sys/types.h>
                      #include <drmaa.h>]],
     [[char buf[1024]; drmaa_init("SGE", buf, sizeof(buf));]])]])


### muParser
case "$with_muparser" in
   yes | '' ) ;;
   *        ) MUPARSER_PATH=$with_muparser ;;
esac

if test -d "$MUPARSER_PATH"; then
   NCBI_FIX_DIR(MUPARSER_PATH)
fi
if test -d "$MUPARSER_PATH/${compiler_vpfx}${DEBUG_SFX}${bit64_sfx}/lib"; then
   MUPARSER_LIBPATH=-L$MUPARSER_PATH/${compiler_vpfx}${DEBUG_SFX}${bit64_sfx}/lib
elif test -d "$MUPARSER_PATH/${compiler_pfx}${DEBUG_SFX}${bit64_sfx}/lib"; then
   MUPARSER_LIBPATH=-L$MUPARSER_PATH/${compiler_pfx}${DEBUG_SFX}${bit64_sfx}/lib
fi

NCBI_CHECK_THIRD_PARTY_LIB(muparser,
  [[AC_LANG_PROGRAM([[#include <muParser.h>]],
     [[mu::Parser parser;]])]])


### hdf5
case "$with_hdf5" in
   yes | '' ) ;;
   *        ) HDF5_PATH=$with_hdf5 ;;
esac

if test -d "$HDF5_PATH/${compiler_vpfx}${DEBUG_SFX}${bit64_sfx}"; then
   HDF5_PATH=$HDF5_PATH/${compiler_vpfx}${DEBUG_SFX}${bit64_sfx}
elif test -d "$HDF5_PATH/${compiler_pfx}${DEBUG_SFX}${bit64_sfx}"; then
   HDF5_PATH=$HDF5_PATH/${compiler_pfx}${DEBUG_SFX}${bit64_sfx}
fi
if test -d "$HDF5_PATH"; then
   NCBI_FIX_DIR(HDF5_PATH)
fi

NCBI_CHECK_THIRD_PARTY_LIB_EX(hdf5, HDF5, hdf5_cpp,
  [[AC_LANG_PROGRAM([[#include <H5Cpp.h>]],
     [[H5::H5Library::open();]])]],
  [-lhdf5])


### Image libraries
# : ${JPEG_PATH=/usr/sfw}
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

# : ${PNG_PATH=/usr/sfw}
NCBI_CHECK_THIRD_PARTY_LIB(png,
 AC_LANG_PROGRAM([#include <png.h>],
    [png_structp png_ptr
      = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
 ]), $Z_LIBS, [], $Z_INCLUDE)

# : ${TIFF_PATH=/usr/sfw}
NCBI_CHECK_THIRD_PARTY_LIB(tiff,
 AC_LANG_PROGRAM([#include <tiffio.h>],
    [TIFF* tiff = TIFFOpen("foo", "r");]), $Z_LIBS, $JPEG_LIBS)
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

case "$x_libraries" in */*) : ${XPM_PATH=`dirname "$x_libraries"`} ;; esac
NCBI_CHECK_THIRD_PARTY_LIB(Xpm,
 AC_LANG_PROGRAM([#include <X11/xpm.h>],
    [XpmImage image; XpmInfo info;
     XpmReadFileToXpmImage("foo", &image, &info);]), $X_LIBS -lX11, [],
    $X_CFLAGS)
# The use of X_CFLAGS is probably redundant, but shouldn't hurt.

## FreeType and FTGL
if test "$with_freetype" != "no" ; then
   : ${FREETYPE_BINPATH=$FREETYPE_PATH/bin}
   AC_PATH_PROG(freetype_config, freetype-config, [],
                [$FREETYPE_BINPATH:$PATH])
   if test -n "$freetype_config" ; then
      : ${FREETYPE_BINPATH=`dirname $freetype_config`}
      : ${FREETYPE_INCLUDE=`$freetype_config --cflags`}
      NCBI_RPATHIFY_OUTPUT_COND(FREETYPE_LIBS, $freetype_config --libs,
         [$no_usr_lib])
      case "$host_os:$bit64_sfx" in
         solaris*:64 )
            ftprefix=`$freetype_config --exec-prefix`
            if test -d "$ftprefix/lib/64"; then
               FREETYPE_LIBS=`echo $FREETYPE_LIBS \
                   | sed -e "s,$ftprefix/lib ,$ftprefix/lib/64 ,g"`
            fi
            ;;
      esac
      AC_CACHE_CHECK([whether FREETYPE_INCLUDE needs doctoring],
                     ncbi_cv_lib_freetype_fix_include,
         [CPPFLAGS="$FREETYPE_INCLUDE $orig_CPPFLAGS"
          AC_COMPILE_IFELSE(
             [AC_LANG_PROGRAM(
                 [[#include <ft2build.h>]], [])],
             [ncbi_cv_lib_freetype_fix_include=no],
             [ncbi_cv_lib_freetype_fix_include=yes])])
      if test "$ncbi_cv_lib_freetype_fix_include" = yes; then
         FREETYPE_INCLUDE=`echo "$FREETYPE_INCLUDE" |\
             sed -e 's:\(-I[[^ ]]*\)\(/freetype2\):\1 \1\2:g'`
      fi
      AC_CACHE_CHECK([whether FreeType works], ncbi_cv_lib_freetype,
         [CPPFLAGS="$FREETYPE_INCLUDE $orig_CPPFLAGS"
          LIBS="$FREETYPE_LIBS $orig_LIBS"
          AC_LINK_IFELSE(
             [AC_LANG_PROGRAM(
                 [[#include <ft2build.h>
                   #include FT_FREETYPE_H]],
                 [[FT_Select_Charmap(NULL, ft_encoding_unicode);]])],
             [ncbi_cv_lib_freetype=yes], [ncbi_cv_lib_freetype=no])])
      if test "$ncbi_cv_lib_freetype" = no; then
         NCBI_MISSING_PACKAGE(freetype)
      fi
   else
      NCBI_MISSING_PACKAGE(freetype)
   fi
fi
if test "$with_freetype" = "no" ; then
   FREETYPE_PATH="No_FREETYPE"
   FREETYPE_INCLUDE=
   FREETYPE_LIBS=
else
   NCBI_PACKAGE(FreeType)
   AC_DEFINE(HAVE_FREETYPE, 1, [Define to 1 if FreeType is available.])
fi

case "$with_ftgl" in
   yes | '' ) ;;
   *        ) FTGL_PATH=$with_ftgl ;;
esac

if test -d "$FTGL_PATH"; then
   NCBI_FIX_DIR(FTGL_PATH)
fi
if test -d "$FTGL_PATH/${compiler_vpfx}${DEBUG_SFX}${bit64_sfx}/lib"; then
   FTGL_LIBDIR=$FTGL_PATH/${compiler_vpfx}${DEBUG_SFX}${bit64_sfx}/lib
elif test -d "$FTGL_PATH/${compiler_pfx}${DEBUG_SFX}${bit64_sfx}/lib"; then
   FTGL_LIBDIR=$FTGL_PATH/${compiler_pfx}${DEBUG_SFX}${bit64_sfx}/lib
fi
NCBI_FIX_DIR(FTGL_LIBDIR)
NCBI_RPATHIFY(FTGL_LIBPATH, $FTGL_LIBDIR)

NCBI_CHECK_THIRD_PARTY_LIB(ftgl,
  [[AC_LANG_PROGRAM([[#include <FTGL/ftgl.h>]], [[FTSimpleLayout layout;]])]],
  [$FREETYPE_LIBS], [], [$FREETYPE_INCLUDE])


NCBI_CHECK_THIRD_PARTY_LIB(magic,
   [AC_LANG_PROGRAM([#include <magic.h>],
       [magic_t m = magic_open(17);])])


# Mimetic
case "$with_mimetic" in
   yes | '' ) ;;
   *        ) MIMETIC_PATH=$with_mimetic ;;
esac
if test -d "$MIMETIC_PATH"; then
   NCBI_FIX_DIR(MIMETIC_PATH)
fi
for d in "$MIMETIC_LIBDIR" \
         "$MIMETIC_PATH/${compiler_vpfx}${DEBUG_SFX}${bit64_sfx}/lib" \
         "$MIMETIC_PATH/${compiler_pfx}${DEBUG_SFX}${bit64_sfx}/lib" \
         "$MIMETIC_PATH/lib${bit64_sfx}" \
         "$MIMETIC_PATH/${compiler_vpfx}${DEBUG_SFX}/lib" \
         "$MIMETIC_PATH/${compiler_pfx}${DEBUG_SFX}/lib" \
         "$MIMETIC_PATH/lib"; do
   if test -d "$d"; then
      MIMETIC_LIBDIR=$d
      break
   fi
done
NCBI_RPATHIFY(MIMETIC_LIBPATH, $MIMETIC_LIBDIR)

NCBI_CHECK_THIRD_PARTY_LIB(mimetic,
   [AC_LANG_PROGRAM([#include <mimetic/mimetic.h>],
       [mimetic::MimeEntity me;])])


# gSOAP++
case "$with_gsoap" in
   yes | '' ) ;;
   *        ) GSOAP_PATH=$with_gsoap ;;
esac
if test -d "$GSOAP_PATH"; then
   NCBI_FIX_DIR(GSOAP_PATH)
fi
for d in "$GSOAP_LIBDIR" \
         "$GSOAP_PATH/${compiler_vpfx}${DEBUG_SFX}${mt_sfx}${bit64_sfx}/lib" \
         "$GSOAP_PATH/${compiler_pfx}${DEBUG_SFX}${mt_sfx}${bit64_sfx}/lib" \
         "$GSOAP_PATH/lib${bit64_sfx}" \
         "$GSOAP_PATH/${compiler_vpfx}${DEBUG_SFX}${mt_sfx}/lib" \
         "$GSOAP_PATH/${compiler_pfx}${DEBUG_SFX}${mt_sfx}/lib" \
         "$GSOAP_PATH/lib"; do
   if test -d "$d"; then
      GSOAP_LIBDIR=$d
      NCBI_FIX_DIR(GSOAP_LIBDIR)
      break
   fi
done
NCBI_RPATHIFY(GSOAP_LIBPATH, $GSOAP_LIBDIR)

: ${GSOAP_BINDIR=`dirname "$GSOAP_LIBDIR"`/bin}
AC_PATH_PROG(GSOAP_SOAPCPP2, soapcpp2, [], $GSOAP_BINDIR:$PATH)
AC_PATH_PROG(GSOAP_WSDL2H,   wsdl2h,   [], $GSOAP_BINDIR:$PATH)

NCBI_CHECK_THIRD_PARTY_LIB_EX(gsoap, GSOAP, gsoapssl++,
   [AC_LANG_PROGRAM([#include <stdsoap2.h>
        SOAP_FMAC3 const char** SOAP_FMAC4 soap_faultcode(struct soap*)
        { return NULL; }
        SOAP_FMAC3 const char** SOAP_FMAC4 soap_faultsubcode(struct soap*)
        { return NULL; }
        SOAP_FMAC3 const char** SOAP_FMAC4 soap_faultstring(struct soap*)
        { return NULL; }
        SOAP_FMAC3 const char** SOAP_FMAC4 soap_faultdetail(struct soap*)
        { return NULL; }
        SOAP_FMAC3 const char* SOAP_FMAC4 soap_check_faultsubcode(struct soap*)
        { return NULL; }
        SOAP_FMAC3 const char* SOAP_FMAC4 soap_check_faultdetail(struct soap*)
        { return NULL; }
        SOAP_FMAC3 void SOAP_FMAC4 soap_serializeheader(struct soap*) { }
        SOAP_FMAC3 int SOAP_FMAC4 soap_putheader(struct soap*) { return 0; }
        SOAP_FMAC3 int SOAP_FMAC4 soap_getheader(struct soap*) { return 0; }
        SOAP_FMAC3 void SOAP_FMAC4 soap_serializefault(struct soap*) { }
        SOAP_FMAC3 int SOAP_FMAC4 soap_putfault(struct soap*) { return 0; }
        SOAP_FMAC3 int SOAP_FMAC4 soap_getfault(struct soap*) { return 0; }],
       [soap s;])],
   $OPENSSL_LIBS $Z_LIBS)

case "$with_sqlite3" in
   yes | '' ) ;;
   *        ) SQLITE3_PATH=$with_sqlite3 ;;
esac


# MongoDB
if test -d "$MONGODB_PATH"; then
   NCBI_FIX_DIR(MONGODB_PATH)
fi
if test -d $MONGODB_PATH/lib${bit64_sfx}; then
   MONGODB_LIBDIR=$MONGODB_PATH/lib${bit64_sfx}
else
   MONGODB_LIBDIR=$MONGODB_PATH/lib
fi
# need Boost rpath
NCBI_RPATHIFY(MONGODB_LIBPATH, $MONGODB_LIBDIR)
NCBI_CHECK_THIRD_PARTY_LIB_EX(mongodb, MONGODB, mongoclient,
   [AC_LANG_PROGRAM([#include <mongo/client/dbclient.h>],
      [std::vector<mongo::HostAndPort> v;
       mongo::DBClientReplicaSet client("foo", v);
       client.connect();])],
   [$BOOST_LIBPATH $boost_fs_lib $BOOST_THREAD_LIBS], [],
   [$BOOST_INCLUDE])
# MongoDB's own library is normally static, but its supporting Boost
# libraries might not be by default.
if test -n "$MONGODB_LIBS"; then
   MONGODB_STATIC_LIBS="$MONGODB_LIBPATH -lmongodb $BOOST_LIBPATH $boost_fs_static_lib $BOOST_THREAD_STATIC_LIBS"
else
   MONGODB_STATIC_LIBS=
fi

### Restore original compiler/linker flags
LIBS="$orig_LIBS"
CPPFLAGS="$orig_CPPFLAGS"


### Optional projects
### --------------------------------------------


## `local_lbsm' support

local_lbsm="ncbi_lbsm ncbi_lbsm_ipc ncbi_lbsmd"

## requires semaphores (lacking on older versions of Cygwin and Darwin)
if test $ncbi_cv_sys_semaphores = no; then
    NCBI_MISSING_PACKAGE(local-lbsm)
fi
## disable by default even on newer Cygwin, as its semaphores don't
## actually work very well
test $OSTYPE = cygwin  &&  : ${with_local_lbsm=no}

for fff in $local_lbsm ; do
   if test ! -f ${real_srcdir}/src/connect/${fff}.c ; then
      if test "$with_local_lbsm" = "yes" ; then
         AC_MSG_ERROR([--with-local-lbsm:  src/connect/${fff}.c is  missing])
      fi
      with_local_lbsm="no"
      break
   fi
done

if test "$with_local_lbsm" = "no" ; then
   local_lbsm="ncbi_lbsmd_stub"
else
   NCBI_PROJECT(local_lbsm)
   AC_DEFINE(HAVE_LOCAL_LBSM, 1,
             [Define to 1 if local LBSM support is available.])
fi


## `connext' support

connext="ncbi_dblb ncbi_ifconf ncbi_localnet"

for fff in $connext ; do
   if test ! -f ${real_srcdir}/src/connect/ext/${fff}.c ; then
      if test "$with_connext" = "yes" ; then
         AC_MSG_ERROR([--with-connext:  src/connect/ext/${fff}.c is missing])
      fi
      with_connext="no"
      break
   fi
done

if test "$with_connext" = "no"; then
   if test "$with_ncbi_crypt" = "yes" ; then
      AC_MSG_ERROR([--with-ncbi-crypt specified, but CONNECT extensions are unavailable])
   fi
   with_ncbi_crypt=no
fi

if test "$with_connext" = "no" ; then
   CONNEXT=
   XCONNEXT=
else
   NCBI_PROJECT(connext)
   CONNEXT=connext
   XCONNEXT=xconnext
   AC_DEFINE(HAVE_LIBCONNEXT, 1,
      [Define to 1 if non-public CONNECT extensions are available.])
fi


## `ncbi_crypt' support

ncbi_crypt="ncbi_crypt"

for fff in $ncbi_crypt ; do
   if test ! -f ${real_srcdir}/src/connect/ext/${fff}.c ; then
      if test "$with_ncbi_crypt" = "yes" ; then
         AC_MSG_ERROR([--with-ncbi-crypt:  src/connect/ext/${fff}.c is missing])
      fi
      with_ncbi_crypt="no"
      break
   fi
done

if test "$with_ncbi_crypt" = "no" ; then
   ncbi_crypt="ncbi_crypt_stub"
else
   NCBI_PROJECT(ncbi_crypt)
   AC_DEFINE(HAVE_NCBI_CRYPT, 1,
      [Define to 1 if the real version of ncbi_crypt support is available.])
fi


## `serial' projects

if test "$with_serial" = "no" ; then
   serial=
else
   NCBI_PROJECT(serial)
   serial="serial"
fi


## `objects' projects

if test "$with_objects" = "no"  -o  -z "$serial" ; then
   objects=
else
   NCBI_PROJECT(objects)
   objects="objects"
fi


## `bdb' projects

dnl NCBI_CHECK_SUBTREE(bdb)
if test "$with_bdb" = "no" ; then
   NoConfProjects="$NoConfProjects bdb"
fi

if test ! -f ${real_srcdir}/src/db/bdb/Makefile.in  -o  \
        ! -d ${real_srcdir}/include/db/bdb ; then
   if test "${with_bdb-no}" != "no" ; then
      AC_MSG_ERROR([--with-]bdb[:  ]BDB[ sources are missing])
   fi
   with_bdb="no"
fi


if test "$with_bdb" = "no" ; then
   bdb=
   BDB_LIB=
   BDB_CACHE_LIB=
else
   NCBI_PROJECT(bdb)
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

DBAPI_DRIVER=
DBAPI_CTLIB=
DBAPI_DBLIB=
DBAPI_MYSQL=
DBAPI_ODBC=
if test "$with_dbapi" = "no" ; then
   dbapi=
else
   NCBI_PROJECT(dbapi)
   dbapi="dbapi"
   DBAPI_DRIVER=dbapi_driver
   test -n "$SYBASE_LIBS"    &&  DBAPI_CTLIB=ncbi_xdbapi_ctlib
   test -n "$SYBASE_DBLIBS"  &&  DBAPI_DBLIB=ncbi_xdbapi_dblib
   test -n "$MYSQL_LIBS"     &&  DBAPI_MYSQL=ncbi_xdbapi_mysql
   test -n "$ODBC_LIBS"      &&  DBAPI_ODBC=ncbi_xdbapi_odbc
fi


## `app' projects

if test "$with_app" = "no" ; then
   app=
else
   NCBI_PROJECT(app)
   app="app"
fi


## `ctools' project cluster

if test "$with_ctools" != "no" ; then
   NCBI_PROJECT(ctools)
fi


## `gui' projects

if test "$with_gui" = "no" ; then
   gui=
else
   NCBI_PROJECT(gui)
   gui="gui"
fi


## `algo' projects

if test "$with_algo" = "no" -o -z "$objects" ; then
   algo=
else
   NCBI_PROJECT(algo)
   algo="algo"
fi


## `internal' project cluster

if test "$with_internal" = "no"  \
    -o  ! -f "${real_srcdir}/src/internal/Makefile.in"; then
   NoConfProjects="$NoConfProjects internal"
   internal=
else
   reason=""
   sep=
   if test ! -d "${real_srcdir}/include/internal"; then
     reason="$reason${sep}headers"
     sep=", "
   fi
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
   if test -n "$reason" -a "$with_internal" = "yes"; then
      if test -z "$with_projects"; then
         AC_MSG_ERROR([--with-internal:  Cannot build INTERNAL projects: missing $reason])
      else
         AC_MSG_WARN([--with-internal:  Cannot build all INTERNAL projects: missing $reason])
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

## Genome Workbench

if test "$with_gbench" != "no"  -a  -d "$real_srcdir/src/app/gbench"; then
   reason=""
   sep=
   if test "$with_wxwidgets" = "no"; then
     reason="$reason${sep}wxWidgets"
     sep=", "
   fi
   if test ! -d "$real_srcdir/src/gui/packages"; then
     reason="$reason${sep}internal packages"
     sep=", "
   fi
   if test "$with_opengl" = "no"; then
     reason="$reason${sep}OpenGL"
     sep=", "
   fi
   if test "$with_sqlite3" = "no"; then
     reason="$reason${sep}SQLite 3.x"
     sep=", "
   fi
   if test "$with_bdb" = "no"; then
     reason="$reason${sep}Berkeley DB"
     sep=", "
   fi
   if test "$with_mt" = "no"; then
     reason="$reason${sep}multithreading"
     sep=", "
   fi
   if test "$with_dll" != "yes"; then
     reason="$reason${sep}full-DLL setup"
     sep=", "
   fi
   if test "$with_algo" = "no"; then
     reason="$reason${sep}other required subtrees"
     sep=","
   fi
   # Anything else?
   if test -n "$reason" -a "$with_gbench" = "yes"; then
      AC_MSG_ERROR([--with-gbench:  Cannot build Genome Workbench: missing $reason])
   elif test -z "$reason"; then
      NCBI_PROJECT(gbench)
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

case "$with_deactivation" in
  no) IF_DEACTIVATING="#" ;;
  * ) IF_DEACTIVATING=""  ;;
esac

case "$with_makefile_auto_update" in
  no) configurables_mfname=configurables.null ;;
  * ) configurables_mfname=configurables      ;;
esac

# Run the test suite after the build?

if test -n "$with_check"  -a  "$with_check" != "no" ; then
   check="check"
   if test "$with_check" != "yes" ; then
      CHECK_ARG=`echo "$with_check" | sed 's/%/ /g'`
   fi
   NCBI_FEATURE(check)
fi
CHECK_TOOLS=$with_check_tools
case "$CHECK_TOOLS" in
   [*[Vv][Aa][Ll][Gg][Rr][Ii][Nn][Dd]*] ) NCBI_FEATURE(Valgrind) ;;
esac
CHECK_OS_NAME=
: ${CHECK_TIMEOUT_MULT=1}
case "$host_os" in
   linux* )
      if test -x /usr/bin/lsb_release; then
         CHECK_OS_NAME=`lsb_release -is | tr A-Z a-z`
      elif test -r /etc/SuSE-release; then
         CHECK_OS_NAME=suse
      elif test -r /etc/redhat-release; then
         CHECK_OS_NAME=`cut -d' ' -f1 /etc/redhat-release | tr A-Z a-z`
      elif test -d /usr/share/doc/ubuntu-keyring; then
         CHECK_OS_NAME=ubuntu
      fi
      ;;
esac

if test "${with_limited_linker}" = "yes"; then
   NCBI_FEATURE(LimitedLinker)
fi

FEATURES="$WithFeatures $WithPackages $WithProjects"

### Compute Without{Features,Packages,Projects}.  Takes quadratic time,
### but that's life.
m4_foreach(CATEGORY, [Features,Packages,Projects],
  [for x in m4_indir([NCBI_ALL_]CATEGORY); do
      case " [$With]CATEGORY " in
         *" $x "*) ;;
         *) [Without]CATEGORY="[$Without]CATEGORY[$Without]CATEGORY[Sep$x]"
            [Without]CATEGORY[Sep]=" "
            FEATURES="$FEATURES -$x"
          ;;
      esac
   done
  ])

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

### Get Insure++ to honor the Toolkit's psrc file
case "$CC" in
   */insure*)
      CC="$CC -Zop \$(top_srcdir)/compilers/unix/psrc -D__INSURE__"
      CXX="$CXX -Zop \$(top_srcdir)/compilers/unix/psrc -D__INSURE__"
      CXXCPP="$CXXCPP -Zop \$(top_srcdir)/compilers/unix/psrc -D__INSURE__"
      LINK_DLL=`echo "$LINK_DLL" | sed -e 's:insure:& -Zop \$(top_srcdir)/compilers/unix/psrc:'`
   ;;
esac


#############################################################################
### Compose a list of configurables("*.in" -- makefiles, shell scripts, etc.)
### in subdirs to configure
### Compose their target counterparts
### Configure

AC_MSG_NOTICE([looking for configurables below $real_srcdir...])
tmp_sed="`pwd`/config.tmp.$$"
for x in $NoConfProjects dll; do
   echo "/^\\.\\/$x/d"
done > $tmp_sed
cat >>$tmp_sed <<EOF
s%^.\\(/build-system.*/Makefile*\\)\\.in\$%$builddir\\1:./src\\1\\.in%
s%^.\\(/build-system\\)\\(/.*\\)\\.in\$%$builddir\\2:./src\\1\\2\\.in%
s%^.\\(/[[^:]]*\\)\\.in\$%$builddir\\1:./src\\1\\.in%
EOF
cat $tmp_sed >&AS_MESSAGE_LOG_FD
configurables=`cd "${real_srcdir}/src"  &&  find . -name '.svn' -prune -o -name '*.in' -print | sed -f $tmp_sed`
rm $tmp_sed

if test -z "$configurables" ; then
  AC_MSG_ERROR([cannot find any configurables below "${real_srcdir}"!])
else
  AC_MSG_NOTICE([...done])
fi

ncbi_runpath=`eval echo "$pure_runpath"`
dnl AC_DEFINE_UNQUOTED(NCBI_RUNPATH, "$ncbi_runpath", [NCBI runpath])
c_ncbi_runpath=`echo "$ncbi_runpath" | sed -e 's:\\$\\$:\\$:g'`

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
AC_SUBST(c_ncbi_runpath)

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
AC_SUBST(LDD_R)

AC_SUBST(CFLAGS)
AC_SUBST(FAST_CFLAGS)
AC_SUBST(CXXFLAGS)
AC_SUBST(FAST_CXXFLAGS)
AC_SUBST(OBJCXX_CXXFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(DEPFLAGS)
AC_SUBST(DEPFLAGS_POST)
AC_SUBST(LDFLAGS)
AC_SUBST(FAST_LDFLAGS)
AC_SUBST(APP_LDFLAGS)
AC_SUBST(DLL_LDFLAGS)
AC_SUBST(LIBS)
AC_SUBST(C_LIBS)
AC_SUBST(OBJCXX_LIBS)
AC_SUBST(GCCPCH)
AC_SUBST(RUNPATH_ORIGIN)
AC_SUBST(NO_STRICT_ALIASING)

AC_SUBST(D_SFX)
AC_SUBST(DEBUG_SFX)

AC_SUBST(LIB_OR_DLL)
AC_SUBST(FORCE_STATIC_LIB)
AC_SUBST(APP_LIB_SETTING)
AC_SUBST(APP_LIBS_SETTING)
AC_SUBST(LINK_DLL)
AC_SUBST(has_dll_loadable)
AC_SUBST(LINK_LOADABLE)
AC_SUBST(CFLAGS_DLL)
AC_SUBST(CXXFLAGS_DLL)
AC_SUBST(ALLOW_UNDEF)
AC_SUBST(FORBID_UNDEF)

AC_SUBST(OPT_GROUPS)
AC_SUBST(local_lbsm)
AC_SUBST(ncbi_crypt)
AC_SUBST(CONNEXT)
AC_SUBST(XCONNEXT)
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
AC_SUBST(CHECK_TOOLS)
AC_SUBST(CHECK_TIMEOUT_MULT)
AC_SUBST(CHECK_OS_NAME)
AC_SUBST(FEATURES)

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
AC_SUBST(DBAPI_DRIVER)
AC_SUBST(DBAPI_CTLIB)
AC_SUBST(DBAPI_DBLIB)
AC_SUBST(DBAPI_MYSQL)
AC_SUBST(DBAPI_ODBC)

AC_SUBST(THREAD_LIBS)
AC_SUBST(NCBIATOMIC_LIB)
AC_SUBST(OPENMP_FLAGS)
AC_SUBST(NETWORK_LIBS)
AC_SUBST(NETWORK_PURE_LIBS)
AC_SUBST(RESOLVER_LIBS)
AC_SUBST(MATH_LIBS)
AC_SUBST(KSTAT_LIBS)
AC_SUBST(RPCSVC_LIBS)
AC_SUBST(CRYPT_LIBS)
AC_SUBST(DL_LIBS)
AC_SUBST(RT_LIBS)
AC_SUBST(UUID_LIBS)
AC_SUBST(DEMANGLE_LIBS)
AC_SUBST(ICONV_LIBS)
AC_SUBST(Z_LIB)
AC_SUBST(BZ2_LIB)
AC_SUBST(PCREPOSIX_LIBS)
AC_SUBST(PCRE_LIB)
AC_SUBST(OPENSSL_STATIC_LIBS)
AC_SUBST(TLS_INCLUDE)
AC_SUBST(TLS_LIBS)
AC_SUBST(SYBASE_PATH)
AC_SUBST(SYBASE_LCL_PATH)
AC_SUBST(SYBASE_INCLUDE)
AC_SUBST(SYBASE_LIBS)
AC_SUBST(SYBASE_DLLS)
AC_SUBST(SYBASE_DBLIBS)
AC_SUBST(MYSQL_INCLUDE)
AC_SUBST(MYSQL_LIBS)
AC_SUBST(BERKELEYDB_INCLUDE)
AC_SUBST(BERKELEYDB_LIBS)
AC_SUBST(BERKELEYDB_STATIC_LIBS)
AC_SUBST(BERKELEYDB_CXX_LIBS)
AC_SUBST(BERKELEYDB_CXX_STATIC_LIBS)
AC_SUBST(ODBC_INCLUDE)
AC_SUBST(ODBC_LIBS)
AC_SUBST(BOOST_INCLUDE)
AC_SUBST(BOOST_LIBPATH)
AC_SUBST(BOOST_TAG)
AC_SUBST(BOOST_FILESYSTEM_LIBS)
AC_SUBST(BOOST_FILESYSTEM_STATIC_LIBS)
AC_SUBST(BOOST_REGEX_LIBS)
AC_SUBST(BOOST_REGEX_STATIC_LIBS)
AC_SUBST(BOOST_SYSTEM_LIBS)
AC_SUBST(BOOST_SYSTEM_STATIC_LIBS)
AC_SUBST(BOOST_TEST_PEM_LIBS)
AC_SUBST(BOOST_TEST_PEM_STATIC_LIBS)
AC_SUBST(BOOST_TEST_TEM_LIBS)
AC_SUBST(BOOST_TEST_TEM_STATIC_LIBS)
AC_SUBST(BOOST_TEST_UTF_LIBS)
AC_SUBST(BOOST_TEST_UTF_STATIC_LIBS)
AC_SUBST(BOOST_THREAD_LIBS)
AC_SUBST(BOOST_THREAD_STATIC_LIBS)
AC_SUBST(NCBI_C_INCLUDE)
AC_SUBST(NCBI_C_LIBPATH)
AC_SUBST(OPENGL_INCLUDE)
AC_SUBST(OPENGL_LIBS)
AC_SUBST(OPENGL_STATIC_LIBS)
AC_SUBST(OSMESA_INCLUDE)
AC_SUBST(OSMESA_LIBS)
AC_SUBST(OSMESA_STATIC_LIBS)
AC_SUBST(GLUT_INCLUDE)
AC_SUBST(GLUT_LIBS)
AC_SUBST(GLEW_INCLUDE)
AC_SUBST(GLEW_LIBS)
AC_SUBST(GLEW_STATIC_LIBS)
AC_SUBST(WXWIDGETS_INCLUDE)
AC_SUBST(WXWIDGETS_LIBS)
AC_SUBST(WXWIDGETS_STATIC_LIBS)
AC_SUBST(WXWIDGETS_GL_LIBS)
AC_SUBST(WXWIDGETS_GL_STATIC_LIBS)
AC_SUBST(WXWIDGETS_POST_LINK)
AC_SUBST(FASTCGI_INCLUDE)
AC_SUBST(FASTCGI_LIBS)
AC_SUBST(FASTCGI_OBJS)
AC_SUBST(NCBI_SSS_INCLUDE)
AC_SUBST(NCBI_SSS_LIBPATH)
AC_SUBST(LIBSSSUTILS)
AC_SUBST(LIBSSSDB)
AC_SUBST(sssutils)
AC_SUBST(SP_INCLUDE)
AC_SUBST(SP_LIBS)
AC_SUBST(ORBACUS_INCLUDE)
AC_SUBST(ORBACUS_LIBPATH)
AC_SUBST(LIBOB)
AC_SUBST(LIBIMR)
AC_SUBST(ICU_CONFIG)
AC_SUBST(ICU_INCLUDE)
AC_SUBST(ICU_LIBS)
AC_SUBST(ICU_STATIC_LIBS)
AC_SUBST(EXPAT_STATIC_LIBS)
AC_SUBST(SABLOT_STATIC_LIBS)
AC_SUBST(LIBXML_STATIC_LIBS)
AC_SUBST(LIBXSLT_STATIC_LIBS)
AC_SUBST(LIBEXSLT_STATIC_LIBS)
AC_SUBST(XERCES_INCLUDE)
AC_SUBST(XERCES_LIBS)
AC_SUBST(XERCES_STATIC_LIBS)
AC_SUBST(XALAN_INCLUDE)
AC_SUBST(XALAN_LIBS)
AC_SUBST(XALAN_STATIC_LIBS)
AC_SUBST(ZORBA_INCLUDE)
AC_SUBST(ZORBA_LIBS)
AC_SUBST(ZORBA_STATIC_LIBS)
AC_SUBST(SQLITE3_STATIC_LIBS)
AC_SUBST(FREETYPE_INCLUDE)
AC_SUBST(FREETYPE_LIBS)
AC_SUBST(GSOAP_PATH)
AC_SUBST(MONGODB_STATIC_LIBS)
AC_SUBST(ncbi_xreader_pubseqos)
AC_SUBST(ncbi_xreader_pubseqos2)
AC_SUBST(UNLESS_PUBSEQOS)
AC_SUBST(PERL)
AC_SUBST(PERL_INCLUDE)
AC_SUBST(PERL_LIBS)
AC_SUBST(JDK_PATH)
AC_SUBST(JDK_INCLUDE)
AC_SUBST(ncbi_java)

AC_SUBST(NCBI_C_ncbi)

AC_SUBST(BINCOPY)
AC_SUBST(APP_NOCOPY)
AC_SUBST(APP_OR_NULL)
AC_SUBST(IF_REBUILDING_LIBS)
AC_SUBST(IF_REBUILDING_CONDITIONALLY)
AC_SUBST(IF_DEACTIVATING)
AC_SUBST(configurables_mfname)
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

test -d "$status_dir"  ||  mkdir -p "$status_dir"
CONFIG_STATUS=$status_dir/config.status

## Configure the compiler/flag-dependent header
AC_CONFIG_HEADER(${ncbiconf}:src/build-system/config.h.in)

##
## Configure makefiles, shell scripts, etc.
##
AC_CONFIG_FILES($configurables $srcdir/./Makefile:src/build-system/Makefile.in.top)

AC_CONFIG_COMMANDS([default], [
dnl make all shell scripts *.sh be executable
find "$build_root/." -name '*.sh' -print | while read x_file ; do
   chmod a+x "$x_file"
done

dnl save cache and log files to the build tree dir "status/"
dnl (alongside config.status, which is now directly created there to
dnl avoid possible trouble; for that matter, it can be at least as
dnl important to avoid having config.log still open for writing during
dnl the move).
if test -f config.cache -a -f config.log ; then
   test -d  "$status_dir"  ||  mkdir -p "$status_dir"
   exec AS_MESSAGE_LOG_FD>/dev/null
   mv config.cache config.log "$status_dir"/
   exec AS_MESSAGE_LOG_FD>>"$status_dir/config.log"
fi

dnl Update list of enabled packages
rm -f "$status_dir"/*.enabled
for p in $WithPackages $WithProjects $WithFeatures ; do
   touch "$status_dir"/$p.enabled
done
for p in $WithoutPackages $WithoutProjects $WithoutFeatures ; do
   touch "$status_dir"/-$p.enabled
done

dnl Create dummy .dep files for system installations of (b)zlib.
dnl This is a stopgap measure, and fails for installations in non-system dirs.
# test -f "$status_dir"/LocalZ.enabled  ||  \
#    touch -t 197001010000 $status_dir/.z.dep
# test -f "$status_dir"/LocalBZ2.enabled  ||  \
#    touch -t 197001010000 $status_dir/.bz2.dep

dnl Incorporate any Makefile.*.mk and ncbiconf.*.h files found in the tree.
find "$srcdir"/src/* -name .svn -prune -o -name 'Makefile.*.mk' -print \
   | while read x; do
      echo
      echo "### Extra macro definitions from $x"
      echo
      echo "#line 1 \"$x\""
      cat "$x"
   done >> "$builddir/Makefile.mk"
$srcdir/scripts/common/impl/report_duplicates.awk \
   src="$srcdir/src/build-system/Makefile.mk.in" "$builddir/Makefile.mk"
find "$srcdir"/src/* -name .svn -prune -o -name 'ncbiconf.*.h' -print \
   | while read x; do
      echo
      echo "/*** Extra macro definitions from $x ***/"
      echo
      echo "#line 1 \"$x\""
      cat "$x"
   done > "$build_root/inc/ncbiconf_extra.h"
if test -s "$build_root/inc/ncbiconf_extra.h"; then
   ncbiconf=$build_root/inc/ncbiconf_unix.h
   newconf=$ncbiconf.extra.new
   savedconf=$ncbiconf.extra.prev
   cat "$ncbiconf" "$build_root/inc/ncbiconf_extra.h" > "$newconf"
   $srcdir/scripts/common/impl/report_duplicates.awk \
      src="$srcdir/src/build-system/config.h.in" "$newconf"
   if test -f "$savedconf"  &&  cmp "$newconf" "$savedconf" >/dev/null; then
      echo "$ncbiconf is unchanged"
   else
      cp -pf "$newconf" "$savedconf"
   fi
   cp -pf "$savedconf" "$ncbiconf"
else
   rm -f "$build_root/inc/ncbiconf_extra.h"
fi

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

dnl Create Makefile.flat if requested
if test "$with_configure_dialog" = yes; then
   if test "$with_flat_makefile" = no; then
      AC_MSG_ERROR([incompatible options: --without-flat-makefile but --with-configure-dialog.])
   fi
   with_flat_makefile=yes
elif test "${with_saved_settings:-no}" != no; then
   if test "$with_flat_makefile" = no; then
      AC_MSG_ERROR([incompatible options: --without-flat-makefile but --with-saved-settings.])
   fi
   with_flat_makefile=yes
fi
if test "$with_flat_makefile" != "no"; then
   if test "$with_flat_makefile" = yes; then
      cfm_flags=''
   else
      cfm_flags='-remoteptb'
   fi
   if test -n "$with_projects"; then
      cfm_flags="$cfm_flags -p $with_projects"
   fi
   if test "$with_configure_dialog" = yes; then
      cfm_flags="$cfm_flags -cfg"
   fi
   if test "${with_saved_settings-no}" != no; then
      cfm_flags="$cfm_flags -saved $with_saved_settings"
   fi
   if $real_srcdir/scripts/common/impl/create_flat_makefile.sh "$build_root" \
       -s "$real_srcdir" $cfm_flags  &&  test -f "$builddir/Makefile.flat"; then
      build_flat="Alternatively, cd $builddir && make -f Makefile.flat"
   elif test -f "$builddir/Flat.configuration_log"; then
      $EGREP 'Error:|already defined' "$builddir/Flat.configuration_log"
      AC_MSG_ERROR([flat makefile generation failed.])
   fi
fi

dnl execute extra-action
if test -n "$with_extra_action" ; then
   with_extra_action=`echo "$with_extra_action" | sed "s%{}%$build_root%g"`
   echo
   echo "executing:  $with_extra_action..."
   eval $with_extra_action
   exit_code="$?"
   if test $exit_code -ne 0 ; then
      AC_MSG_WARN([non-zero exit code ($exit_code):  $with_extra_action])
   fi
   echo
fi

dnl print out additional configuration options
case "$LIB_OR_DLL" in
both) DLL_USAGE="LIBRARIES:  build both static and dynamic by default" ;;
dll)  DLL_USAGE="LIBRARIES:  build as dynamic by default" ;;
lib)
   if test "$with_dll" = "no" ; then
      DLL_USAGE="LIBRARIES:  build as static only"
   else
      DLL_USAGE="LIBRARIES:  build as static by default"
   fi
   ;;
esac

if test -n "$with_projects"; then
    build_proj="To build selected projects (as listed in \"$with_projects\"):
  cd $builddir && make all_p"
fi

cat << EOCONF
===============================================================================
NCBI C++ Toolkit documentation:
  Online:   http://www.ncbi.nlm.nih.gov/toolkit/doc/book/
  Local:    ./doc/public/index.html
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
$build_flat
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
WithoutFeatures='$WithoutFeatures'
WithPackages='$WithPackages'
WithoutPackages='$WithoutPackages'
WithProjects='$WithProjects $OPT_GROUPS'
WithoutProjects='$WithoutProjects'
with_extra_action='$with_extra_action'
with_projects='$with_projects'
with_flat_makefile='$with_flat_makefile'
with_configure_dialog='$with_configure_dialog'
with_saved_settings='$with_saved_settings'
smart_pwd='$smart_pwd'
real_srcdir='$real_srcdir'
EGREP='$EGREP'
])

AC_WARNING([*** Please don't forget to update .../src/build-system/NEWS if appropriate! ***])dnl
AC_OUTPUT

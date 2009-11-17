#!/bin/sh

# $Id$
# Author:  Andrei Gourianov, NCBI (gouriano@ncbi.nlm.nih.gov)

#-----------------------------------------------------------------------------
#set -xv
#set -x

TAB="	"
# defaults
use_debug="yes"
use_dll="no"
use_64="no"
use_universal="no"
use_staticstd="no"
use_projectlst="src/"
use_action=""
use_arch=""

mk_name="Makefile"
sln_name="configured"
generated="_generated_files.txt"
tmpfile="tmptmp.tmp"
buildres="build_results.txt"
#-----------------------------------------------------------------------------
# silently ignored  options
noops="\
 --without-optimization \
 --with-profiling \
 --with-tcheck \
 --with-static \
 --with-plugin-auto-load \
 --with-bin-release \
 --with-mt \
 --without-exe \
 --with-runpath \
 --with-lfs \
 --with-autodep \
 --with-build-root \
 --with-fake-root \
 --without-suffix \
 --with-hostspec \
 --without-version \
 --with-build-root-sfx \
 --without-execopy \
 --with-bincopy \
 --with-lib-rebuilds \
 --with-lib-rebuilds \
 --without-deactivation \
 --without-makefile-auto-update \
 --without-flat-makefile \
 --with-check \
 --with-check-tools \
 --with-ncbi-public \
 --with-strip \
 --with-pch \
 --with-caution \
 --without-caution \
 --without-ccache \
 --with-distcc \
 --without-ncbi-c \
 --without-sss \
 --without-utils \
 --without-sssdb \
 --with-included-sss \
 --with-z \
 --without-z \
 --with-bz2 \
 --without-bz2 \
 --with-lzo \
 --without-lzo \
 --with-pcre \
 --without-pcre \
 --with-gnutls \
 --without-gnutls \
 --with-openssl \
 --without-openssl \
 --without-sybase \
 --with-sybase-local \
 --with-sybase-new \
 --without-ftds \
 --with-ftds \
 --without-ftds-renamed \
 --without-mysql \
 --with-mysql \
 --without-fltk \
 --with-fltk \
 --without-opengl \
 --with-opengl \
 --without-mesa \
 --with-mesa \
 --without-glut \
 --with-glut \
 --without-wxwin \
 --with-wxwin \
 --without-wxwidgets \
 --with-wxwidgets \
 --with-wxwidgets-ucs \
 --without-wxwidgets-ucs \
 --without-freetype \
 --with-freetype \
 --without-fastcgi \
 --with-fastcgi \
 --with-fastcgi \
 --without-bdb \
 --with-bdb \
 --without-sp \
 --without-orbacus \
 --with-orbacus \
 --with-odbc \
 --with-python \
 --without-python \
 --with-cppunit \
 --without-cppunit \
 --with-boost \
 --without-boost \
 --with-boost-tag \
 --without-boost-tag \
 --with-sqlite \
 --without-sqlite \
 --with-sqlite3 \
 --without-sqlite3 \
 --with-icu \
 --without-icu \
 --with-expat \
 --without-expat \
 --with-sablot \
 --without-sablot \
 --with-libxml \
 --without-libxml \
 --with-libxslt \
 --without-libxslt \
 --with-xerces \
 --without-xerces \
 --with-xalan \
 --without-xalan \
 --with-oechem \
 --without-oechem \
 --with-sge \
 --without-sge \
 --with-muparser \
 --without-muparser \
 --with-hdf5 \
 --without-hdf5 \
 --with-gif \
 --without-gif \
 --with-jpeg \
 --without-jpeg \
 --with-png \
 --without-png \
 --with-tiff \
 --without-tiff \
 --with-xpm \
 --without-xpm \
 --without-local-lbsm \
 --without-ncbi-crypt \
 --without-connext \
 --without-serial \
 --without-objects \
 --without-dbapi \
 --without-app \
 --without-ctools \
 --without-gui \
 --without-algo \
 --without-internal \
 --with-gbench \
 --without-gbench \
 --with-x \
"
#-----------------------------------------------------------------------------

initial_dir=`pwd`
script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`

Usage()
{
    cat <<EOF 1>&2
USAGE: $script_name [OPTION]...
SYNOPSIS:
 configure NCBI C++ toolkit for Xcode build system.
OPTIONS:
  --help                    -- print Usage
  --without-debug           -- build non-debug versions of libs and apps
  --with-debug              -- build debug versions of libs and apps
  --without-dll             -- build all toolkit libraries as static ones
  --with-dll                -- assemble toolkit libraries into DLLs where requested
  --with-64                 -- compile to 64-bit code
  --with-universal          -- build universal binaries
  --with-universal=CPU      -- build universal binaries targeting the given CPU
  --with-static-exe         -- use static C++ standard libraries
  --with-projects=FILE      -- build projects listed in FILE
  --with-extra-action=SCR   -- script to call after the configuration is complete
  --ignore-unsupported-options   --ignore unsupported options
EOF
}

# has one optional argument: error message
Error()
{
  Usage
  test -z "$1"  ||  echo ERROR: $1 1>&2
  cd "$initial_dir"
  exit 1
}

#--------------------------------------------------------------------------------

cd "$script_dir"
unknown=""
ignore_unknown="no"
for cmd_arg in "$@"; do
  case "$cmd_arg" in
    --help                )  Usage; exit 0 ;;
    --without-debug       )  use_debug="no" ;;
    --with-debug          )  use_debug="yes" ;;
    --without-dll         )  use_dll="no" ;;
    --with-dll            )  use_dll="yes" ;;
    --with-64             )  use_64="yes" ;;
    --with-universal      )  use_universal="yes" ;;
    --with-static-exe     )  use_staticstd="yes" ;;
    --with-universal=*    )  use_arch="$cmd_arg" ;;
    --with-projects=*     )  use_projectlst="$cmd_arg" ;;
    --with-extra-action=* )  use_action="$cmd_arg" ;;
    --ignore-unsupported-options ) ignore_unknown="yes" ;;
    *  ) unknown="$unknown $cmd_arg" ;;
  esac
done

# check and report unknown options
for u in $unknown; do
  found="no"
  for n in $noops; do
    echo $u | grep --regexp="^$n" >/dev/null 2>/dev/null
    if test $? -eq 0; then
      found="yes"
      break
    fi
  done
  if test $found = "yes"; then
    echo "Ignored:  $u"
  else
    if test $ignore_unknown = "no"; then
      Error  "Invalid option:  $u"
    else
      echo "Ignored unsupported:  $u"
    fi
  fi
done

use_arch=`echo $use_arch | sed -e s/--with-universal=//`
use_projectlst=`echo $use_projectlst | sed -e s/--with-projects=//`
use_action=`echo $use_action | sed -e s/--with-extra-action=//`
#--------------------------------------------------------------------------------

xcodebuild -version >/dev/null 2>/dev/null
test $? -ne 0 && Error "xcodebuild not found"
arch=`arch`
platform=`xcodebuild -version 2>/dev/null | grep 'Xcode [0-9][0-9]*' | sed -e 's/[ .]//g'`

# ------- target architecture
if test -z "$use_arch"; then
  if test "$use_universal" == "yes"; then
    if test "$use_64" == "yes"; then
      use_arch="ppc64 x86_64"
    else
      use_arch="ppc i386"
    fi
  else
    use_arch="$arch"
  fi
fi 
PTB_PLATFORM="$use_arch"

# ------- configuration and flags
if test "$use_dll" = "yes"; then
  build_results="dll"
  PTB_FLAGS="-dll"
  if test "$use_debug" = "yes"; then
    CONFIGURATION="DebugDLL"
    conf="DebugDLL"
  else
    CONFIGURATION="ReleaseDLL"
    conf="ReleaseDLL"
  fi
else
  build_results="static"
  PTB_FLAGS=""
  if test "$use_debug" = "yes"; then
    if test "$use_staticstd" = "yes"; then
      CONFIGURATION="DebugMT"
      conf="DebugMT"
    else
      CONFIGURATION="DebugDLL"
      conf="Debug"
    fi
  else
    if test "$use_staticstd" = "yes"; then
      CONFIGURATION="ReleaseMT"
      conf="ReleaseMT"
    else
      CONFIGURATION="ReleaseDLL"
      conf="Release"
    fi
  fi
fi
sln_path="${platform}-${conf}-${use_arch}"
sln_path=`echo $sln_path | sed -e 's/ /_/g'`

#test -d "$sln_path" && rm -rf "$sln_path"
#mkdir "$sln_path"
sln_name="$sln_path"
sln_path="$build_results"

test -e "$TREE_ROOT/$PTB_PROJECT_REQ" || Error "$PTB_PROJECT_REQ not found"

#--------------------------------------------------------------------------------
# prepare and run ptb.sh

export PTB_PLATFORM
export PTB_FLAGS
export PTB_PATH=./static/bin/ReleaseDLL
export SLN_PATH=$sln_path/$sln_name
export TREE_ROOT=../..
export BUILD_TREE_ROOT=.
export PTB_PROJECT_REQ="$use_projectlst"

./ptb.sh
test $? -ne 0 && Error "Configuration failed"

#--------------------------------------------------------------------------------
# generate makefile

TARGET="BUILD_ALL"
cfgs="$build_results,$sln_name,$CONFIGURATION"
{
  echo "# generated by $script_name  on `date`"
  echo
  echo TARGET=$TARGET
  echo CONFIGURATION=$CONFIGURATION
  echo
  echo "all :"
  echo "${TAB}xcodebuild -project ${sln_name}.xcodeproj -target \${TARGET} -configuration \${CONFIGURATION}"
  echo "${TAB}cd ..; echo $cfgs > cfgs.log"
  echo
  echo "clean :"
  echo "${TAB}xcodebuild -project ${sln_name}.xcodeproj -target \${TARGET} -configuration \${CONFIGURATION} clean"
  echo
  echo "check :"
  echo "${TAB}cd ..; ./check.sh run; ./check.sh concat_err; ./check.sh load_to_db"
  echo
  echo "all_r : all"
  echo "all_p : all"
  echo "clean_r : clean"
  echo "clean_p : clean"
  echo "purge : clean"
  echo "check_r : check"
  echo "check_p : check"
  echo
} > $sln_path/$mk_name

#--------------------------------------------------------------------------------
curr=`pwd`
resloc=$curr
genloc=$curr/$sln_path
cd $TREE_ROOT
root=`pwd`/
cd $curr
resloc=`echo $resloc | sed -e s%$root%%`
genloc=`echo $genloc | sed -e s%$root%%`
cd $sln_path

#--------------------------------------------------------------------------------
# generate build_results text

{
  echo $resloc/$build_results/bin/$CONFIGURATION
  echo $resloc/$build_results/lib/$CONFIGURATION
} > $buildres

#--------------------------------------------------------------------------------
# modify generated_files text

if test -f $sln_name$generated; then
  {
    cat $sln_name$generated
    echo $genloc/$mk_name
    echo $genloc/$buildres
  } > $tmpfile
  rm -f $sln_name$generated
  mv $tmpfile $sln_name$generated
fi

cd "$initial_dir"
#--------------------------------------------------------------------------------
# extra action
if test -n "$use_action"; then
  use_action=`echo $use_action | sed -e s%{}%$curr/$sln_path%g`
  echo
  echo "Executing $use_action"
  eval $use_action
  test "$?" -ne 0 && Error "Extra action $use_action failed"
fi

#--------------------------------------------------------------------------------
echo
echo "------------------------------------------------------------------------------"
echo "Configure succeeded"
echo "To build the project, execute the following commands:"
echo "cd $curr/$sln_path"
echo "make"

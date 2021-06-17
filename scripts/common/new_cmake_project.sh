#!/bin/sh
#############################################################################
# $Id$
#   Create new CMake project which uses prebuilt NCBI C++ toolkit from a sample
#   Author: Andrei Gourianov, gouriano@ncbi
#############################################################################
initial_dir=`pwd`
script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`
cd "$initial_dir"
tree_root=`pwd`

build_root=$NCBI
build_dir=c++.cmake.stable

repository="https://svn.ncbi.nlm.nih.gov/repos/toolkit/trunk/c++"
rep_inc="include"
rep_src="src"
rep_sample="sample"
prj_prj="CMakeLists.txt"
cfg_cfg="configure.sh"
############################################################################# 
Usage()
{
    cat <<EOF
USAGE:
  $script_name <name> <type> [builddir] 
SYNOPSIS:
  Create new CMake project which uses prebuilt NCBI C++ toolkit from a sample.
ARGUMENTS:
  --help       -- print Usage
  <name>       -- project name (destination directory)
  <type>       -- project type
  builddir     -- root directory of the pre-built NCBI C++ toolkit
                  default: $build_dir
EOF
if test -z "$1"; then
  echo
  echo The following project types are available:
  svn list -R $repository/$rep_src/$rep_sample | while IFS= read -r line; do
    if [[ $line =~ .*/$ ]]; then
      count=`echo $line | tr -cd '/' | wc -c`
      if [[ $count -eq 2 ]]; then
        prj=${line%?}
        case "$prj" in 
        app/alnmgr)         echo "  app/alnmgr          an application that uses Alignment Manager";;
        app/asn)            echo "  app/asn             an application that uses ASN.1 specification";;
        app/basic)          echo "  app/basic           a simple application";;
        app/blast)          echo "  app/blast           a BLAST application";;
        app/cgi)            echo "  app/cgi             a CGI application";;
        app/connect)        echo "  app/connect         a Usage report logging";;
        app/dbapi)          echo "  app/dbapi           a DBAPI application";;
        app/deployable_cgi) echo "  app/deployable_cgi  a CD-deployable CGI application";;
        app/eutils)         echo "  app/eutils          an eUtils client application";;
        app/http_session)   echo "  app/http_session    an application that uses CHttpSession";;
        app/lds)            echo "  app/lds             an application that uses local data store";;
        app/multicmd)       echo "  app/multicmd        a simple command-based application";;
        app/netcache)       echo "  app/netcache        an application that uses NetCache";;
        app/netschedule)    echo "  app/netschedule     an NCBI GRID (NetSchedule) application";;
        app/objects)        echo "  app/objects         an application that uses ASN.1 objects";;
        app/objmgr)         echo "  app/objmgr          an application that uses Object Manager";;
        app/sdbapi)         echo "  app/sdbapi          a Simple-DBAPI application";;
        app/serial)         echo "  app/serial          a data serialization application";;
        app/soap)           echo "  app/soap/client     a SOAP client"
                            echo "  app/soap/server     a SOAP server";;
        app/unit_test)      echo "  app/unit_test       a Boost-based unit test application";;
        lib/asn_lib)        echo "  lib/asn_lib         a static library from ASN.1 specification";;
        lib/basic)          echo "  lib/basic           a simple static library";;
        lib/dtd)            echo "  lib/dtd             a static library from DTD specification";;
        lib/jsd)            echo "  lib/jsd             a static library from JSON Schema specification";;
        lib/xsd)            echo "  lib/xsd             a static library from XML Schema specification";;
        *)                  echo "  $prj";;
        esac 
      fi
    fi
  done
fi
echo " "
}

Error()
{
  Usage notype
  if test -n "$1"; then
    echo ----------------------------------------------------------------------
    echo ERROR: $@ 1>&2
  fi
  cd "$initial_dir"
  exit 1
}

#----------------------------------------------------------------------------
if [ $# -eq 0 ]; then
  do_help="yes"
fi
pos=0
unknown=""
while [ $# -ne 0 ]; do
  case "$1" in 
    --help|-help|help|-h)
      do_help="yes"
    ;; 
    *) 
      if [ $pos -eq 0 ]; then
        prj_name=$1
      elif [ $pos -eq 1 ]; then
        prj_type=$1
      elif [ $pos -eq 2 ]; then
        toolkit=$1
      else
        unknown="$unknown $1"
      fi
      pos=`expr $pos + 1`
      ;; 
  esac 
  shift 
done 


if [ -n "$do_help" ]; then
    Usage
    exit 0
fi
if [ -n "$unknown" ]; then
  Error "Unknown options: $unknown" 
fi
if [ -z "$prj_name" ]; then
  Error "Mandatory argument 'name' is missing"
fi
if [ -z "$prj_type" ]; then
  Error "Mandatory argument 'branch' is missing"
fi
if [ -z "$toolkit" ]; then
  builddir=${build_root}/${build_dir}
else
  if [ -d "$toolkit" ]; then
    builddir="$toolkit"
  elif [ -d "${build_root}/$toolkit" ]; then
    builddir="${build_root}/$toolkit"
  elif [ -d "${build_root}/c++.cmake.$toolkit" ]; then
    builddir="${build_root}/c++.cmake.$toolkit"
  fi
fi
if [ ! -d "$builddir" ]; then
  echo "----------------------------------------------------------------------"
  if [ -z "$toolkit" ]; then
    echo "ERROR:  Directory not found: $builddir" 1>&2
  else
    echo "ERROR:  Directory not found: $toolkit" 1>&2
  fi
  echo "Try one of these:"
  ls $NCBI | grep c++.cmake
  exit 1
fi

if [ -e $prj_name ]; then
  Error File or directory $prj_name already exists
fi
mkdir -p $prj_name
if [ ! -d $prj_name ]; then
  Error Failed to create directory $prj_name
fi
cd $prj_name

#----------------------------------------------------------------------------
#path=`echo $prj_type | tr "/" " "`
#for d in $path; do
#  echo ---- $d
#done

mkdir $rep_inc 2>/dev/null
mkdir $rep_src 2>/dev/null
svn co $repository/$rep_inc/$rep_sample/$prj_type $rep_inc 1>/dev/null 2>/dev/null
svn co $repository/$rep_src/$rep_sample/$prj_type $rep_src 1>/dev/null
find . -name ".svn" -exec rm -rf {} \; 1>/dev/null 2>/dev/null
find $rep_src -name "Makefile.*" -exec rm -rf {} \; 1>/dev/null 2>/dev/null

# create configure script
host_os=`uname`
#if test $host_os = "Darwin"; then
cmake_cfg="cmake-cfg-unix.sh"

{
  echo "#!/bin/sh"
  echo "script_dir=\`dirname \$0\`"
  echo "script_name=\`basename \$0\`"
  echo "exec $builddir/src/build-system/cmake/$cmake_cfg --rootdir=\$script_dir --caller=\$script_name \"\$@\""

} > $cfg_cfg
chmod a+x $cfg_cfg

# modify CMakeLists.txt
{
  echo "cmake_minimum_required(VERSION 3.7)"
  echo "project($prj_name)"
  echo "include($builddir/src/build-system/cmake/CMake.NCBItoolkit.cmake)"
  echo "NCBI_add_subdirectory($rep_src)"
} > $prj_prj

echo "Created project $prj_name"
echo "To configure:  cd $prj_name; ./$cfg_cfg <arguments>"
echo "For help:      cd $prj_name; ./$cfg_cfg --help"
cd "$initial_dir"

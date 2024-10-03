#!/bin/sh
#############################################################################
# $Id$
#   Create new CMake project which uses Conan package of the NCBI C++ toolkit from a sample
#   Author: Andrei Gourianov, gouriano@ncbi
#############################################################################
initial_dir=`pwd`
script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`
cd "$initial_dir"
tree_root=`pwd`

repository="https://svn.ncbi.nlm.nih.gov/repos/toolkit/trunk/c++"
rep_inc="include"
rep_src="src"
rep_sample="sample"
prj_prj="CMakeLists.txt"
prj_con="conanfile.py"
ncbitk="ncbi-cxx-toolkit-core"
tkversion="[>=28]"
readme="README.txt"
############################################################################# 
Usage()
{
    cat <<EOF
USAGE:
  $script_name <name> <type>
SYNOPSIS:
  Create new CMake project which uses Conan package of the NCBI C++ toolkit from a sample.
ARGUMENTS:
  --help       -- print Usage
  <name>       -- project name (destination directory)
  <type>       -- project type
EOF
if test -z "$1"; then
  echo
  echo The following project types are available:
  svn list -R $repository/$rep_src/$rep_sample | while IFS= read -r line; do
    case "$line" in */ )
      count=`echo $line | tr -cd '/' | wc -c`
      if [ $count -eq 2 ]; then
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
      ;;
    esac
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
  Error "Mandatory argument 'type' is missing"
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


# create CMakeLists.txt
{
  echo "cmake_minimum_required(VERSION 3.15)"
  echo "project(${prj_name})"
  echo "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY \${CMAKE_BINARY_DIR}/bin)"
  echo "find_package(${ncbitk} REQUIRED)"
  echo "NCBIptb_setup()"
  echo "NCBI_add_subdirectory(${rep_src})"
} > $prj_prj

# create conanfile.py
{
  echo "from conan import ConanFile"
  echo "from conan.tools.cmake import cmake_layout"
  echo "class NCBIapp(ConanFile):"
  echo "    settings = \"os\", \"compiler\", \"build_type\", \"arch\""
  echo "    generators = \"CMakeDeps\", \"CMakeToolchain\", \"VirtualRunEnv\""
  echo "    def requirements(self):"
  echo "        self.requires(\"${ncbitk}/${tkversion}\")"
  echo "    def layout(self):"
  echo "        cmake_layout(self)"
} > $prj_con

# create readme
{
  echo "To request a specific version of the toolkit"
  echo "  edit ${prj_con}"
  echo " "
  echo "To install Conan packages:"
  echo "  conan install . --build missing -s build_type=Release"
  echo " "
  echo "To list available presets:"
  echo "  cmake --list-presets"
  echo " "
  echo "To run CMake to generate makefiles:"
  echo "  cmake --preset conan-release"
  echo " "
  echo "To build the project:"
  echo "  cmake --build build/Release"
  echo " "
  echo "For other command line options, see documentation:"
  echo " https://docs.conan.io/2/reference/commands/install.html"
  echo " https://cmake.org/cmake/help/latest/manual/cmake.1.html"
} > $readme

echo "Created project $prj_name"
echo " "
cat $readme
cd "$initial_dir"

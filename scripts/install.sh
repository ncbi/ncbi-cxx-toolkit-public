#! /bin/sh

# $Id$
# Author:  Denis Vakatov, NCBI 
#
###########################################################################
# 
#   Install non-configurable headers, sources, makefiles, scripts and docs
#   (which can be used/shared by several builds).
#
#   Note:  the builds (which contain libs, apps, configured headers and
#          makefiles must be installed separately, using script
#          "c++/*/build/install.sh".
#
###########################################################################

echo "[`date`]"

cvs_location=`echo '$Source$' | sed "s%\\$[S]ource: *\\([^$][^$]*\\) \\$.*%\\1%"`
cvs_revision=`echo '$Revision$' | sed "s%\\$[R]evision: *\\([^$][^$]*\\) \\$.*%\\1%"`

script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`

run_dir=`pwd`
run_cmd="$0 $*"

top_srcdir=`(cd "${script_dir}/.." ; pwd)`
install_dir="$1"


#####  USAGE

Usage()
{
  cat <<EOF 1>&2
Usage:
   $script_name <install_dir>
              [--without-src] [--without-doc] [--with-purge] [--with-cvs]

Synopsis:
   Install NCBI C++ source tree (shared sources, headers, scripts, docs).
   (Hint: to install a build tree, use "c++/*/build/install.sh".)

Arguments:
   <install_dir>  - where to install
   --without-src  - do not install source files (*.cpp, *.c from "src/" dir)
   --without-doc  - do not install HTML documentation (from "doc/" dir)
   --with-purge   - delete all(!) build dirs under the <install_dir>
   --with-cvs     - install CVS sub-dirs

Script:  CVS location/revision, run directory, and run command:
   $cvs_location | R$cvs_revision
   $run_dir
   $run_cmd

ERROR:  $1
EOF

  kill $$
  exit 1
}


#####  ARGS

test -f "$top_srcdir/include/corelib/ncbistd.hpp"  ||  \
   Usage "The script is located in the wrong directory"
test $# -ne 0  ||  \
   Usage "Too few arguments passed"
test -n "$install_dir"  ||  \
   Usage "<install_dir> argument missing"

shift
for x_arg in "$@" ; do
  case "$x_arg" in
    --without-src )  with_src="no"    ;;
    --without-doc )  with_doc="no"    ;;
    --with-purge  )  with_purge="yes" ;;
    --with-cvs    )  with_cvs="yes"   ;;
    * )  Usage "Unknown argument \"$x_arg\""
  esac
done


#####  INFO

cat <<EOF
Installing C++ Toolkit sources:
  from:  $top_srcdir
  to:    $install_dir
EOF


#####  INSTALL


# Directories to install
install_dirs="include compilers scripts"
if test "$with_src" != "no"; then 
    install_dirs="$install_dirs src LICENSE gpl.txt configure config.guess config.sub config.h.in"
fi
if test "$with_doc" != "no" -a -d "$top_srcdir/doc"; then
    install_dirs="$install_dirs doc"
fi


# Setup an empty install dir (create a new one, or purge an existing one)
if test "$with_purge" = "yes" ; then
   # Delete previous builds' installation, or whatever else happened
   # to be there...
   rm -rf "$install_dir"
   mkdir "$install_dir"
elif test -r "$install_dir" ; then
   # Must be a directory...
   test -d "$install_dir"  ||  Usage "$install_dir is not a directory"

   # Deal with non-empty install dir
   if test "`ls -a $install_dir/ | wc -w`" != "2" ; then
      # Test if it matches C++ Toolkit dir structure
      test ! -f "$install_dir/include/corelib/ncbistd.hpp"  &&  \
        Usage "Non-empty dir $install_dir does not have C++ Toolkit structure"

      # Delete previous sources' installation
      for d in compilers scripts src doc include ; do 
         test ! -r "$install_dir/$d"  ||  rm -r "$install_dir/$d"  ||  \
            Usage "Cannot delete $install_dir/$d"
      done
   fi
else
   # Create new install dir
   mkdir "$install_dir"  ||  Usage "Cannot create $install_dir"
fi


# Copy to the install dir;  use TAR to preserve symbolic links, if any
case "`uname`" in
  Darwin ) bs=63 ;;
  *      ) bs=200 ;;
esac
for d in $install_dirs ; do
   ( cd $top_srcdir  &&  tar cfb - $bs $d ) |  \
     ( cd $install_dir  &&  tar xfb - $bs )
   test $? -eq 0  ||  Usage "Failed to copy to $install_dir/$d"

   # Get rid of the CVS sub-dirs
   if test "$with_cvs" != "yes" ; then
      find $install_dir/$d -type d -name CVS -prune -exec rm -r {} \;
   fi
done


# Done
echo "DONE"

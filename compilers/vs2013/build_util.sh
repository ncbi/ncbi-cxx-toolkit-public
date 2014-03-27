#! /bin/sh
# $Id$
# Author:  Vladimir Ivanov (ivanov@ncbi.nlm.nih.gov)
#
# Utility functions for building C++ Toolkit.


#---------------- Arguments ----------------

arg="$1"

script=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "$script_dir"; pwd)`



#-------------- Functions --------------

error()
{
    echo "[$script] ERROR:  $1"
    exit 1
}


#---------------- Main ----------------

case "$arg" in

  --with-openmp )

      makefile="$script_dir/../../src/build-system/Makefile.mk.in.msvc"
      if [ ! -f $makefile ] ; then
          error "$makefile not found"
          exit 1
      fi
      tmp=/tmp/build_util_$$
      trap "rm -f $tmp $tmp.awk" 0 1 2 15

      cat <<-EOF >$tmp.awk
	    /^\[Compiler\]/ {
	       comp = 1
	    }
	    /^AdditionalOptions=/ {
	       if (comp == 1) {
	          gsub("/openmp","")
	          gsub("AdditionalOptions=","AdditionalOptions=/openmp ")
	          gsub("  ", " ")
	          comp = 0
	       }
	    }
	    /.*/ {
	       print
	    }
	EOF
      awk -f $tmp.awk $makefile >$tmp || exit 1
      touch -r $makefile $tmp
      cp -fp $tmp $makefile  || exit 2
      ;;

  * )
      error "Unknown command: $arg"
      ;;
esac

exit 0

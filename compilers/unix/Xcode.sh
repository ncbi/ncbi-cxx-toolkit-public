#!/bin/sh

# $Id$
# Author:  Andrei Gourianov, NCBI (gouriano@ncbi.nlm.nih.gov)
# Wrapper for Xcode configuration  script

#-----------------------------------------------------------------------------
initial_dir=`pwd`
script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`


Usage() {
    echo "USAGE: `basename $0` version [--configure-flags]"
    exit $1
}
Usage()
{
    cat <<EOF 1>&2
USAGE: $script_name version [--configure-flags] 
SYNOPSIS:
 Wrapper for Xcode configuration  script.
ARGUMENTS:
  version    -- mandatory. Xcode major version (eg, 30)
EOF
    echo
    test -z "$2"  ||  echo ERROR: $2 1>&2
    exit $1
}

(cd "$script_dir/../.." \
     &&  ./scripts/common/impl/generate_all_objects.sh --clients)

platform=`uname`
test ! "$platform" = "Darwin" && Usage 1 "Please run the script on Mac OS X (Darwin) computer only"
test $# -lt 1 && Usage 1 "Mandatory argument is missing"
xcode="$script_dir/../xcode${1}_prj"
test ! -d "$xcode" && Usage 1 "$xcode folder not found"
shift
$xcode/configure "$@"

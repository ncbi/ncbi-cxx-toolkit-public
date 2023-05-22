#! /bin/sh
#############################################################################
# $Id$
#  Generate CMake Toolchain file
#############################################################################
if test $# -lt 0; then
  echo ERROR: not enought arguments to cmkTool.sh
  exit 1
fi
script_dir=`dirname $0`
CCNAME=$1
CCVERSION=$2

processor=`uname -m | tr '[:upper:]' '[:lower:]'`
os=`uname | tr '[:upper:]' '[:lower:]'`
compiler=`echo ${CCNAME} | tr '[:upper:]' '[:lower:]'`
version=`echo ${CCVERSION} | tr -d '.'`

pfx="${script_dir}/"
sfx=".cmake.in"
template=${processor}-${os}-${compiler}-${version}
#echo "template ${template}" 1>&2
while [ -n "${template}" ]; do
    if [ -r ${pfx}${template}${sfx} ]; then
        break
    fi
    template=${template%?}
done
if [ -z "${template}" ]; then
    echo NOT FOUND: toolchain template ${processor}-${os}-${compiler}-${version}${sfx}
    exit 1
fi

#toolchain=`pwd`/toolchain.tmp
toolchain=`mktemp`
if ! which envsubst > /dev/null 2>&1; then
    {
        while read -r line; do
            echo $line;
        done < ${pfx}${template}${sfx}
    } > ${toolchain}
else
    envsubst < ${pfx}${template}${sfx} > ${toolchain}
fi
echo ${toolchain}

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
if which envsubst > /dev/null 2>&1; then
    envsubst < ${pfx}${template}${sfx} > ${toolchain}
#elif which python3 > /dev/null 2>&1; then
#    python3 -c 'import os,sys; sys.stdout.write(os.path.expandvars(sys.stdin.read()))' < ${pfx}${template}${sfx} > ${toolchain}
#elif which python > /dev/null 2>&1; then
#    python -c 'import os,sys; sys.stdout.write(os.path.expandvars(sys.stdin.read()))' < ${pfx}${template}${sfx} > ${toolchain}
#elif which perl > /dev/null 2>&1; then
#    perl -pe 's/\$(\{)?([a-zA-Z_]\w*)(?(1)\})/$ENV{$2}/g' < ${pfx}${template}${sfx} > ${toolchain}
else
    ( echo 'cat <<EOF'; cat "${pfx}${template}${sfx}"; echo 'EOF' ) | /bin/sh -s > "${toolchain}"
fi
chmod ug+rw ${toolchain}
chmod o+r ${toolchain}
echo ${toolchain}

#!/bin/sh
# $Id$

script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`

. ${script_dir}/common.sh

if test "$#" -lt 2; then
    COMMON_Error "USAGE: $script_name build-root [-]name[=value] ..."
fi

buildroot=$1
shift

ncbiconf="$buildroot/inc/ncbiconf_unix.h"
newconf="$ncbiconf.tweaked.new"
savedconf="$ncbiconf.tweaked.prev"

test -f "$ncbiconf"  ||  COMMON_Error "$ncbiconf not found"
cp "$ncbiconf" "$newconf"

cat >> "$newconf" <<EOF

/*
 * The following preprocessor directives result from passing configure the flag
 *    --with-extra-action="$0 {} $*"
 */

EOF

for arg in "$@"; do
    case "$arg" in
        [A-Za-z_]*=*)
            echo "$arg" | sed -e 's/^/\#define /; s/=/ /' >> "$newconf"
            ;;
        [A-Za-z_]*)
            echo "#define $arg 1" >> "$newconf"
            ;;
        -[A-Za-z_]*)
            name=`echo $arg | sed -e 's/^-//'`
            cat >> "$newconf" <<EOF
#ifdef $name
#  undef $name
#endif
EOF
            ;;
        *)
            COMMON_Error "Syntax error in argument $arg (must be name, -name, or name=value)"
            ;;
    esac
done

if test -f "$savedconf"  &&  cmp "$newconf" "$savedconf" >/dev/null; then
    echo "$script_name: $ncbiconf is unchanged."
else
    cp -p "$newconf" "$savedconf"
fi
cp -p "$savedconf" "$ncbiconf"

exit 0

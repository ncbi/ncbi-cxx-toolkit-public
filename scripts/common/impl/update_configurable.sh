#!/bin/sh
script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`
. ${script_dir}/../common.sh

top_srcdir=`(cd "$script_dir/../../.." && pwd)`
build_root=$1
builddir=$build_root/build
output=$2

inum() {
    ls -i $1 2>/dev/null | cut -d' ' -f1
}

different_file() {
    # echo "different_file $@" >&2
    if [ "`inum $1`" = "`inum $2`" ]; then
        # be paranoid, as device numbers may somehow differ...
        if cmp -s $1 $2; then
            return 1
        else
            return 0
        fi
    else
        return 0
    fi
}

case $output in
    /*) ;;
    *)  output=`pwd`/$output ;;
esac

ohead=`dirname $output`
otail=`basename $output`

[ -d $ohead ]   ||  mkdir -p $ohead
[ -f $output ]  ||  touch -t 197607040000 $output

while different_file $builddir/$otail $output; do
    case "$ohead" in [./]) break ;; esac
    otail=`basename $ohead`/$otail
    ohead=`dirname $ohead`
done

if different_file $builddir/$otail $output; then
    fmt >&2 <<EOF
$script_name: Ignoring $output, which seems not to belong to the build
tree rooted at $builddir.
EOF
    exit 0
fi

case $otail in
    */* | Makefile )
        CONFIG_FILES=$builddir/$otail:./src/$otail.in
        ;;
    * )
        CONFIG_FILES=$builddir/$otail:./src/build-system/$otail.in
        ;;
esac

cd $top_srcdir
CONFIG_HEADERS=
CONFIG_LINKS=
CONFIG_COMMANDS=
export CONFIG_FILES CONFIG_HEADERS CONFIG_LINKS CONFIG_COMMANDS
$build_root/status/config.status
status=$?

case $output in
    */Makefile.mk)
        find src/* -name .svn -prune -o -name 'Makefile.*.mk' -print \
            | while read x; do
            echo
            echo "### Extra macro definitions from $x"
            echo
            echo "#line 1 \"$x\""
            cat "$x"
        done >> "$builddir/Makefile.mk"
        scripts/common/impl/report_duplicates.awk \
            src=./src/build-system/Makefile.mk.in "$builddir/Makefile.mk"
        ;;

    *.sh)
        chmod +x $output
        ;;

    *.[ch] | *.[ch]pp)
        if cmp -s $output $output.last; then
            echo $output is unchanged.
            touch -r $output.last $output
        else
            cp -p $output $output.last
        fi
        ;;
esac

exit $status

#!/bin/sh
abs_saved_config_dir=$1
build_name=$2

abs_top_srcdir="`cd $abs_saved_config_dir/../../c++ && pwd`"
abs_config_dir=$abs_top_srcdir/$build_name

mkdir $abs_top_srcdir/$build_name
mkdir $abs_top_srcdir/$build_name/bin
mkdir $abs_top_srcdir/$build_name/build
mkdir $abs_top_srcdir/$build_name/inc
mkdir $abs_top_srcdir/$build_name/lib
mkdir $abs_top_srcdir/$build_name/status

sed "
s|@script_shell@|#!/bin/sh|
s|@builddir@|$abs_top_srcdir/$build_name/build|
s|@abs_top_srcdir@|$abs_top_srcdir|" <$abs_top_srcdir/src/reconfigure.sh.in >$abs_top_srcdir/$build_name/build/reconfigure.sh
chmod +x $abs_top_srcdir/$build_name/build/reconfigure.sh

touch $abs_top_srcdir/$build_name/inc/ncbiconf_unix.h

sed "
s|@build_name@|$build_name|g
s|@saved_config_abs_top_srcdir@|$abs_top_srcdir|g" <$abs_saved_config_dir/config.status.in >$abs_top_srcdir/$build_name/status/config.status
chmod +x $abs_top_srcdir/$build_name/status/config.status

echo "Configuration restored into $abs_config_dir."
echo "cd $abs_config_dir/build && ./reconfigure.sh update"

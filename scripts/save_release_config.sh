#!/bin/sh

config_src_dir=$1
config_name=$2

build_name="`basename $config_src_dir`"
abs_config_src_dir="`cd $config_src_dir && pwd`"
abs_top_srcdir="`cd $config_src_dir/.. && pwd`"
abs_release_dir="`cd $config_src_dir/../../builds && pwd`"
abs_config_dir=$abs_release_dir/$config_name

echo Config source $abs_config_src_dir
echo Config name $config_name
echo Config target $abs_config_dir

config_target_dir=builds/$config_name

if [ ! -d $abs_config_dir ] ; then
    mkdir $abs_config_dir
fi

sed "
s|$abs_config_src_dir|@saved_config_abs_top_srcdir@/@build_name@|g
s|$abs_top_srcdir|@saved_config_abs_top_srcdir@|g" <$abs_config_src_dir/status/config.status >$abs_config_dir/config.status.in

echo "#!/bin/sh
script_dir=\"\`dirname \$0\`\"
abs_script_dir=\"\`cd \$script_dir && pwd\`\"
../../c++/scripts/copy_saved_config.sh \$abs_script_dir $build_name" >$abs_config_dir/copy_saved_config.sh
chmod +x $abs_config_dir/copy_saved_config.sh

cp $config_src_dir/build/Makefile.mk $abs_config_dir/

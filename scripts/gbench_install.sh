#! /bin/sh
# $Id$
#
# GBENCH installation script.
# Author: Anatoliy Kuznetsov


script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`

PLUGINS='doc_basic doc_table algo_basic algo_stdio view_text view_graphic view_align view_sequence view_table'
BINS='gbench gbench_plugin_scan'



Usage()
{
    cat <<EOF 1>&2
USAGE: $script_name [--copy] sourcedir targetdir
SYNOPSIS:
   Create Genome Workbench installation from the standard Toolkit build.
ARGUMENTS:
   --copy     -- use Unix 'cp' command instead of 'ln -s'.
   sourcedir  -- path to the pre-built NCBI Toolkit.
   targetdir  -- target installation directory.
EOF
    test -z "$1"  ||  echo ERROR: $1 1>&2
    exit 1
}

Error()
{
    cat <<EOF 1>&2

ERROR: $1    
EOF
    exit 1
}


FindDirPath()
{
   path="$1"
   while [ "$path" != '.' ]; do
     path=`dirname $path`
     if [ -d "$path/$2" ]; then
        echo $path
        break;
     fi
   done
}


MakeDirs()
{
    mkdir -p $1
    mkdir -p $1/bin
    mkdir -p $1/lib
    mkdir -p $1/etc
    mkdir -p $1/plugins
}


CopyFiles()
{
    for x in $BINS; do
        echo copying: $x
        src_file=$src_dir/bin/$x 
        if [ -f $src_file ]; then
            mv -f $target_dir/bin/$x $target_dir/bin/$x.old  2>/dev/null
            rm -f $target_dir/bin/$x $target_dir/bin/$x.old
            $BINCOPY $src_file $target_dir/bin/ \
                || Error "Cannot copy file $x"
        else
            Error "File not found: $src_file"
        fi
    done

    for x in $PLUGINS; do
        echo copying plugin: $x
        rm -f $target_dir/plugins/libgui_$x.so
        $BINCOPY $src_dir/lib/libgui_$x.so $target_dir/plugins/ \
          || echo "Cannot copy plugin $x"
    done

    for x in $src_dir/lib/libdbapi*.so; do
        if [ -f $x ]; then
            f=`basename $x`
            echo copying DB interface: $f
            rm -f $target_dir/lib/$f
            $BINCOPY $x $target_dir/lib/ ||  Error "Cannot copy $x"
        fi
    done
}



if [ $# -eq 0 ]; then
    Usage "Wrong number of arguments passed"
fi 

copy_all="no"
if [ $1 = "--copy" ]; then
    copy_all="yes"
    BINCOPY="cp -pf"
    src_dir=$2
    target_dir=$3
else
    BINCOPY="ln -s"
    src_dir=$1
    target_dir=$2
fi


if [ -z "$src_dir" ]; then
    Usage "Source directory not provided."
fi

if [ -z "$target_dir" ]; then
    Usage "Target directory not provided."
fi

if [ ! -d $src_dir ]; then
    Error "Source directory not found: $src_dir"
else
    echo Source: $src_dir    
    echo Target: $target_dir    
fi

if [ ! -d $src_dir/bin ]; then
    Error "bin directory not found: $src_dir/bin"
fi

if [ ! -d $src_dir/lib ]; then
    Error "lib directory not found: $src_dir/lib"
fi



source_dir=`FindDirPath $src_dir /src/gui/gbench`
source_dir=$source_dir/src/gui/gbench/


MakeDirs $target_dir

CopyFiles


echo preparing scripts 

if [ ! -f $source_dir/gbench_install/run-gbench.sh ]; then
    Error "startup file not found: $source_dir/gbench_install/run-gbench.sh"
fi

cat ${source_dir}/gbench_install/run-gbench.sh \
 | sed "s,@install_dir@,${target_dir},g" \
 > ${target_dir}/bin/run-gbench.sh

chmod 755 ${target_dir}/bin/run-gbench.sh

if [ -f ${source_dir}/gbench_install/move-gbench.sh ]; then
   cp -p ${source_dir}/gbench_install/move-gbench.sh ${target_dir}/bin/
fi
cp -p ${source_dir}/gbench.ini ${target_dir}/etc/  

echo "Configuring plugin cache"
. ${script_dir}/common.sh
COMMON_AddRunpath ${target_dir}/lib
${target_dir}/bin/gbench_plugin_scan -dir ${target_dir}/plugins \
  || Error "Plugin scan failed"

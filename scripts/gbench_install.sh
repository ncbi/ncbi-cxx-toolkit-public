#! /bin/sh
# $Id$
#
# GBENCH installation script.
# Author: Anatoliy Kuznetsov, Denis Vakatov

script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`


DYLD_BIND_AT_LAUNCH=1
export DYLD_BIND_AT_LAUNCH

. ${script_dir}/common.sh


algos='align basic cn3d external gnomon linkout phylo validator'
docs='basic table'
views='align graphic phylo_tree table taxplot text validator'
PLUGINS="ncbi_init"
for t in algo doc view; do
   eval l=\$${t}s
   # echo $t: $l >&2
   for x in $l; do
      PLUGINS="$PLUGINS ${t}_$x"
   done
done

BINS='gbench-bin gbench_plugin_scan'

Usage()
{
    cat <<EOF 1>&2
USAGE: $script_name [--copy|--hardlink] [--setup-src] sourcedir targetdir
SYNOPSIS:
   Create Genome Workbench installation from the standard Toolkit build.
ARGUMENTS:
   --copy      -- use Unix 'cp' command instead of 'ln -s'.
   --hardlink  -- use Unix 'ln' command instead of 'ln -s'.
   --setup-src -- set up links in the source bin directory.
   sourcedir   -- path to pre-built NCBI Toolkit.
   targetdir   -- target installation directory.

EOF
    test -z "$1"  ||  echo ERROR: $1 1>&2
    exit 1
}


FindDirPath()
{
   path="$1"
   while [ "$path" != '.' -a "$path" != '/' ]; do
     path=`dirname $path`
     if [ -d "$path/$2" ]; then
        echo $path
        break
     fi
   done
}


MakeDirs()
{
    COMMON_ExecRB mkdir -p $1
    COMMON_ExecRB mkdir -p $1/bin
    COMMON_ExecRB mkdir -p $1/lib
    COMMON_ExecRB mkdir -p $1/etc
    COMMON_ExecRB mkdir -p $1/plugins
    COMMON_ExecRB mkdir -p $1/share
    COMMON_ExecRB mkdir -p $1/executables
    COMMON_ExecRB mkdir -p $1/etc/patterns
}


CommonRoot()
{
    dir=`echo $1 | sed -e 's|/\$||'`
    list1=`echo "$dir" | tr '/' ' '`
    root=''
    for i in $list1 ; do
        newroot=$root/$i
        tail=`echo $2 | sed -e "s|^$newroot||"`
        if [ ! $2 = ${newroot}$tail ]; then
            break;
        fi
        root=$newroot
    done
    if [ ! -z "$root" ]; then
        echo $root
    fi
}

RelativePath()
{
    path1="$1"
    path2="$2"
    if [ "$path1" = "$path2" ]; then
        echo .
        return
    fi
    diff=""

    croot=`CommonRoot $1 $2`
    if [ -z "$croot" ]; then
        return
    fi
    tail=`echo $path2 | sed -e "s|^$croot||"`
    for i in `echo "$tail" | tr '/' ' '` ; do
        back_path="${back_path}"../
    done
    back_path=`echo $back_path | sed -e 's|/\$||'`
    tail=`echo $path1 | sed -e "s|^$croot||"`
    diff=${back_path}${tail}
    echo $diff
}

RelativeCP()
{
    if [ "$copy_all" = "yes" ]; then
        COMMON_ExecRB $BINCOPY $1 $2
    else
        # trying to find the relative path
        path1=`dirname $1`
        path2=$2
        pdiff=`RelativePath $path1 $path2`

        if [ "$pdiff" = "" ]; then
            COMMON_ExecRB $BINCOPY $1 $2
        else
            path2=$2
            file2=`basename $1`
            old_path=`pwd`
            cd $path2
            COMMON_ExecRB $BINCOPY $pdiff/$file2 $file2
            cd $old_path
        fi
    fi
}

DoCopy()
{
    RelativeCP $1 $2

    case `uname`:$1 in
        Darwin*:*.so)
        dylib_file=`echo $1 | sed "s,\.so$,.dylib",`
        RelativeCP $dylib_file $2
        ;;
    esac
}

DoMove()
{
    $BINMOVE $1 $2
    case `uname`:$1 in
        Darwin*:*.so)
        dylib_file=`echo $1 | sed "s,\.so$,.dylib",`
        $BINMOVE $dylib_file $2
        ;;
    esac
}


CopyFiles()
{
    for x in $BINS; do
        echo copying: $x
        src_file=$src_dir/bin/$x
        if [ -f $src_file ]; then
            mv -f $target_dir/bin/$x $target_dir/bin/$x.old  2>/dev/null
            rm -f $target_dir/bin/$x $target_dir/bin/$x.old
            RelativeCP $src_file $target_dir/bin/
        else
            echo "++++++++ RP proc: $x_common_rb"
            $x_common_rb
            COMMON_Error "File not found: $src_file"
        fi
    done

    for src_file in $src_dir/lib/lib*.so; do
        echo copying: `basename $src_file`
        if [ -f $src_file ]; then
            rm -f $target_dir/lib/`basename $src_file`
            rm -f $target_dir/plugins/`basename $src_file .so`.dylib
            DoCopy $src_file $target_dir/lib/
        else
            $x_common_rb
            COMMON_Error "File not found: $src_file"
        fi
    done

    for x in $PLUGINS; do
        echo installing plugin: $x
        rm -f $target_dir/plugins/libgui_$x.so
        rm -f $target_dir/plugins/libgui_$x.dylib
        DoMove $target_dir/lib/libgui_$x.so $target_dir/plugins/
    done

    for x in $src_dir/lib/lib*dbapi*.so; do
        if [ -f $x ]; then
            f=`basename $x`
            echo copying DB interface: $f
            rm -f $target_dir/lib/$f
            DoCopy $x $target_dir/lib/
        fi
    done
}



#
#  MAIN
#

if [ $# -eq 0 ]; then
    Usage "Wrong number of arguments"
fi

copy_all="no"
setup_src="no"
BINCOPY="ln -sf"
BINMOVE="mv -f"
MAC_BINCOPY=
MAC_BINCOPY_OPTS=
MAC_BINMOVE=
while :; do
    case "$1" in
        --copy)
            copy_all="yes"
            BINCOPY="cp -pf"
            MAC_BINCOPY="/Developer/Tools/CpMac"
            MAC_BINCOPY_OPTS="-p"
            MAC_BINMOVE="/Developer/Tools/MvMac"
            ;;
        --hardlink)
            copy_all="yes"
            BINCOPY="ln -f"
            MAC_BINCOPY="$script_dir/ln_mac.sh"
            MAC_BINMOVE="/Developer/Tools/MvMac"
            ;;
        --setup-src)
            setup_src="yes"
            ;;
        -*)
            echo "Unrecognized option $1"
            exit 1
            ;;
        *)
            break
            ;;
    esac
    shift
done
src_dir=$1
target_dir=$2

if test "`uname`" = Darwin; then
    test -x "$MAC_BINCOPY"  &&  BINCOPY="$MAC_BINCOPY $MAC_BINCOPY_OPTS"
    test -x "$MAC_BINMOVE"  &&  BINMOVE=$MAC_BINMOVE
fi


src_dir=`cd "$src_dir" && /bin/pwd`
target_dir=`echo $target_dir | sed -e 's|/\$||'`

if [ -z "$src_dir" ]; then
    Usage "Source directory not provided"
fi

if [ -z "$target_dir" ]; then
    Usage "Target directory not provided"
fi

if [ ! -d $src_dir ]; then
    COMMON_Error "Source directory not found: $src_dir"
else
    echo Source: $src_dir
    echo Target: $target_dir
fi

if [ ! -d $src_dir/bin ]; then
    COMMON_Error "bin directory not found: $src_dir/bin"
fi

if [ ! -d $src_dir/lib ]; then
    COMMON_Error "lib directory not found: $src_dir/lib"
fi


source_dir=`FindDirPath $src_dir /src/gui/gbench`
source_dir="${source_dir}/src/gui/gbench"

# Set the rollback escape command (used by COMMON_ExecRB)
x_common_rb="rm -rf $target_dir"

MakeDirs $target_dir
target_dir=`cd "$target_dir" && /bin/pwd`

CopyFiles


echo "Preparing scripts"

COMMON_ExecRB cp ${source_dir}/gbench_install/run-gbench.sh ${target_dir}/bin/run-gbench.sh
chmod 755 ${target_dir}/bin/run-gbench.sh

COMMON_ExecRB cp ${source_dir}/gbench_install/gbench_remote_template.sh ${target_dir}/bin/gbench_remote_template.sh
COMMON_ExecRB cp ${source_dir}/gbench_install/gbench_noremote_template.sh ${target_dir}/bin/gbench_noremote_template.sh

COMMON_ExecRB cp -p ${source_dir}/gbench_install/move-gbench.sh ${target_dir}/bin/
chmod 755 ${target_dir}/bin/move-gbench.sh

COMMON_ExecRB ${target_dir}/bin/move-gbench.sh ${target_dir}

find ${source_dir}/../res -type f | grep -v '/CVS/' |
while read file
do
    new_file=`echo $file | sed "s,^${source_dir}/../res/,,"`
    echo "Installing resource: $new_file"
    new_file=${target_dir}/${new_file}
    mkdir -p `dirname $new_file`
    COMMON_ExecRB cp $file $new_file
done

echo "Configuring plugin cache"
# COMMON_AddRunpath may interfere with compiled-in runpaths, so we
# explicitly search third-party library directories.
fltk_config=`sed -ne 's/^FLTK_CONFIG *= *//p' ${src_dir}/build/Makefile.mk`
fltk_libdir=`dirname \`dirname $fltk_config\``/lib
bdb_libdir=`sed -ne 's/BERKELEYDB_LIBS *= *-L\([^ ]*\).*/\1/p' ${src_dir}/build/Makefile.mk`
sqlite_libdir=`sed -ne 's/SQLITE_LIBS *= *-L\([^ ]*\).*/\1/p' ${src_dir}/build/Makefile.mk`
COMMON_AddRunpath ${src_dir}/lib:${fltk_libdir}:${bdb_libdir}:${sqlite_libdir}
CHECK_TIMEOUT=600
export CHECK_TIMEOUT
COMMON_ExecRB ${script_dir}/check/check_exec_test.sh ${target_dir}/bin/gbench_plugin_scan -strict ${target_dir}/plugins

echo "Copying executable plugins:"
for f in ${source_dir}/../plugins/algo/executables/*; do
    # don't make links for *~ (emacs backup files) or #*# (autosave)
    case $f in *~ | \#*\# ) continue ;; esac
    if [ ! -d $f ]; then
        echo copying executable: `basename $f`
        rm -f ${target_dir}/executables/`basename $f`
        DoCopy $f ${target_dir}/executables
    fi
done

# copy pattern files for searching
for f in ${source_dir}/patterns/*; do
    if [ ! -d $f ]; then
        rm -f ${target_dir}/etc/patterns/`basename $f`
        DoCopy $f ${target_dir}/etc/patterns
    fi
done

[ "$setup_src" = yes ] || exit 0

# Do this last, to be sure the symlink doesn't end up dangling.
rm -f ${src_dir}/bin/gbench
ln -s ${target_dir}/bin/run-gbench.sh ${src_dir}/bin/gbench

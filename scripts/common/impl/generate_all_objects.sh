#!/bin/sh
# $Id$

# Generate classes for all public ASN.1/XML specs and selected
# internal ones (if present).

new_module=$NCBI/c++.metastable/Release/build/new_module.sh
force=false

for arg in "$@"; do
    case "$arg" in
        --force ) force=true ;;
        *       ) echo "Usage: $1 [--force]" ; exit 1 ;;
    esac
done

status=0

for spec in src/serial/test/we_cpp.asn src/objects/*/*.asn \
  src/objtools/eutils/*/*.dtd src/gui/objects/*.asn \
  src/algo/gnomon/gnomon.asn src/algo/ms/formats/*/*.dtd \
  src/build-system/project_tree_builder/msvc71_project.dtd \
  src/app/sample/asn/sample_asn.asn src/app/sample/soap/soap_dataobj.xsd \
  src/internal/objects/*/*.asn src/internal/mapview/objects/*/*.asn \
  src/internal/gbench/app/sviewer/objects/*.asn \
  src/internal/gbench/plugins/radar/lib/*.asn \
  src/internal/blast/SplitDB/asn*/*.asn \
  src/internal/blast/SplitDB/BlastdbInfo/asn/*.asn ; do
    if test -f "$spec"; then
        case $spec in
            */seq_annot_ref.asn ) continue ;; # sample data, not a spec
            *.asn               ) ext=.asn; flag= ;;
            *.dtd               ) ext=.dtd; flag=--dtd ;;
            *.xsd               ) ext=.xsd; flag=--xsd ;;
        esac
        dir=`dirname $spec`
        base=`basename $spec $ext`
        if $force || [ ! -f $dir/$base.files ]; then
            echo $spec
            if (cd $dir && $new_module $flag $base >/dev/null 2>&1); then
                : # all good
            else
                # exit $?
                echo "$new_module $flag $base FAILED with status $?:"
                (cd $dir && $new_module $flag $base)
                status=1
            fi
        else
            echo "$spec -- skipped, already built and --force not given."
        fi
    else
        # Not necessarily fatal -- the tree may be deliberately incomplete.
        echo "Warning: $spec not found"
    fi
done

splitdb_dir=src/internal/blast/SplitDB/asn
if [ -f $splitdb_dir/Makefile.asntool ]; then
    if $force || [ ! -f $splitdb_dir/objPSSM.c ]; then
        top_srcdir=`pwd`
        builddir=`ls -dt $top_srcdir/*/build $top_srcdir/.??*/build | head -1`
        [ -d "$builddir" ] || builddir=$NCBI/c++.metastable/Release/build
        (cd $splitdb_dir && ${MAKE-make} -f Makefile.asntool sources top_srcdir=$top_srcdir builddir=$builddir) || exit $?
    fi
fi

exit $status

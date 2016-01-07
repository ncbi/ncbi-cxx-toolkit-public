#!/bin/sh
# $Id$

# Generate classes for all public ASN.1/XML specs and selected
# internal ones (if present).

LC_ALL=C
export LC_ALL

new_module=$NCBI/c++.metastable/Release/build/new_module.sh
force=false

for arg in "$@"; do
    case "$arg" in
        --force ) force=true ;;
        *       ) echo "Usage: $1 [--force]" ; exit 1 ;;
    esac
done

failed=

for spec in src/serial/test/we_cpp.asn src/objects/*/*.asn \
  src/objtools/eutils/*/*.dtd src/gui/objects/*.asn src/gui/framework/*.dtd \
  src/algo/gnomon/gnomon.asn src/algo/ms/formats/*/*.??d \
  src/build-system/project_tree_builder/msvc71_project.dtd \
  src/build-system/project_tree_builder/msbuild/msbuild_dataobj.xsd \
  src/sample/app/asn/sample_asn.asn src/sample/app/soap/soap_dataobj.xsd \
  src/sample/lib/asn_lib/asn_sample_lib.asn \
  src/sample/lib/dtd/dtd_sample_lib.dtd \
  src/sample/lib/xsd/xsd_sample_lib.xsd \
  src/internal/objects/*/*.asn src/internal/objects/*/*.xsd \
  src/internal/stax/taxon/taxon.dtd \
  src/internal/ncbils2/asn/login.asn src/internal/ncbils2/auth/*xml/*.dtd \
  src/internal/geo/cgi/geo*/objects/geo*.asn \
  src/internal/asn_cache/lib/cache_blob.asn \
  src/internal/mapview/objects/*/*.asn \
  src/internal/gbench/packages/pkg_radar/*/*.asn \
  src/internal/gbench/app/sviewer/objects/*.asn \
  src/internal/blast/DistribDbSupport/*asn*/*.asn \
  src/internal/blast/JIRA/LibJiraAsn/jirasvc.asn \
  src/internal/blast/SplitDB/asn*/*.asn \
  src/internal/blast/SplitDB/BlastdbInfo/asn/BlastdbInfo.asn \
  src/internal/blast/Blastdb2Entrez/dbASN/bdb2ez.asn \
  src/internal/cppcore/test_stat_ext/loader/BoostTestXML.dtd \
  src/internal/gpipe/objects/*/*.asn src/internal/gpipe/objects/*/*.dtd \
  src/internal/gpipe/objects/*/*.xsd \
  src/internal/gpipe/gpinit/src/gpinit_compare/gpinit.asn \
  src/internal/gpipe/gpexec/queue/lib/gpxapi.asn \
  src/internal/peptides/objects/unimod/unimod.xsd \
  src/internal/variation/snp/objects/rsm/rsm.asn; do
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
                failed="$failed $base"
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
    top_srcdir=`pwd`
    builddir=`ls -dt $top_srcdir/*/build $top_srcdir/.[A-Z]??*/build | head -1`
    [ -d "$builddir" ] || builddir=$NCBI/c++.metastable/Release/build
    make_asntool="${MAKE-make} -f Makefile.asntool sources top_srcdir=$top_srcdir builddir=$builddir"
    if $force || [ ! -f ${splitdb_dir}gendefs/objGendefs.c ]; then
        (cd ${splitdb_dir}gendefs && $make_asntool) || failed="$failed asngendefs-C"
    fi
    if $force || [ ! -f $splitdb_dir/objPSSM.c ]; then
        (cd $splitdb_dir && $make_asntool) || failed="$failed SplitDB-misc-C"
    fi
    if $force || [ ! -f ${splitdb_dir}dbld/objDbld.c ]; then
        (cd ${splitdb_dir}dbld && $make_asntool) || failed="$failed asndbld-C"
    fi
    if $force || [ ! -f src/internal/msgmail2/asn/objmmail.c ]; then
        (cd src/internal/msgmail2/asn && $make_asntool) || failed="$failed objmmail"
    fi
fi

if test -n "$failed"; then
    echo "FAILED: $failed"
    exit 1
else
    echo DONE
    exit 0
fi

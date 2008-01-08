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

for spec in src/serial/test/we_cpp.asn src/objects/*/*.asn \
  src/objtools/eutils/*/*.dtd src/algo/ms/formats/*/*.dtd \
  src/app/project_tree_builder/msvc71_project.dtd \
  src/app/sample/asn/sample_asn.asn src/app/sample/soap/soap_dataobj.xsd \
  src/gui/dialogs/edit/feature/*.asn src/gui/objects/*.asn \
  src/internal/objects/*/*.asn src/internal/mapview/objects/*/*.asn \
  src/internal/gbench/app/sviewer/objects/*.asn \
  src/internal/gbench/plugins/radar/lib/*.asn ; do
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
            (cd $dir && $new_module $flag $base >/dev/null 2>&1)  ||  exit $?
        else
            echo "$spec -- skipped, already built and --force not given."
        fi
    else
        # Not necessarily fatal -- the tree may be deliberately incomplete.
        echo "Warning: $spec not found"
    fi
done

exit 0

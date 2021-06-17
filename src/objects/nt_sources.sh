#! /bin/sh
# $Id$
#
# Script to update ASN.1 objects' sources on Windows-NT
#       (using BASH and DATATOOL)
#

cd $(dirname $(echo $0 | sed 's%\\%/%g'))

if test -z "$1" ; then
    ROOT="$(echo $PWD | sed 's%/cygdrive/\([a-zA-Z]\)/%\1:\\%' | sed 's%//\([a-zA-Z]\)/%\1:\\%' | sed 's%/src/objects%%')"
    if echo "$ROOT" | grep '^/' >/dev/null ; then
        ROOT="u:\\`echo \"$ROOT\" | sed 's%/home/[a-zA-Z_]*/%%'`"
    fi
else
    ROOT="$1"
fi
if test ! -d $ROOT/src/objects ; then
    echo "Cannot auto-find C++ Toolkit in: \"$ROOT\""
    echo "please specify the path to it, like (note the double back-slash):"
    echo "      $0 c:\\\\ncbi_cxx"
    exit 1
fi


TOOL="$ROOT/compilers/msvc_prj/serial/datatool/DebugMT/datatool"

OBJECTS="$ROOT/src/objects"

MODULES='insdseq omssa tinyseq gbseq docsum taxon1 mim entrez2 general biblio medline medlars pub pubmed mla seqloc seqalign seqblock seqfeat seqres seqset seq submit proj mmdb1 mmdb2 mmdb3 cdd ncbimime access featdef objprt seqcode id1 id2 cn3d'

for m in $MODULES; do \
    echo Updating $m
    (
        cd $m
        M="$(grep ^MODULE_IMPORT $m.module | sed 's/^.*= *//' | sed 's/\(objects[/a-z0-9]*\)/\1.asn/g')"
        if ! "$TOOL" -oR "$ROOT" -m "$m.asn" -M "$M" -oA -of "$m.files" -or "objects/$m" -oc "$m" -odi -od "$m.def"; then
            echo ERROR!
            exit 2
        fi
    ) || exit 2
done

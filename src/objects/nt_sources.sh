#! /bin/sh
# $Id$
#
# Script to update ASN.1 objects' sources on Windows-NT
#       (using bash and datatool)
#

cd $(dirname $(echo $0 | sed 's%\\%/%g'))
ROOT="$(echo $PWD | sed 's%/cygdrive/\([a-zA-Z]\)/%\1:\\%' | sed 's%//\([a-zA-Z]\)/%\1:\\%' | sed 's%/src/objects%%')"
TOOL="$ROOT/compilers/msvc_prj/serial/datatool/DebugMT/datatool"

OBJECTS="$ROOT/src/objects"

MODULES='docsum taxon1 mim entrez2 general biblio medline medlars pub pubmed seqloc seqalign seqblock seqfeat seqres seqset seq submit proj mmdb1 mmdb2 mmdb3 cdd ncbimime access featdef objprt seqcode id1 cn3d'

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

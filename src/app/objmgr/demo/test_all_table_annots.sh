#! /bin/bash

d=~/cpp/src/objects/seqtable/samples

{
echo cdd 6
echo exon 42741670
echo mgc 17391200
echo snp 6912644
echo snp2 29794392
echo sts 30240931
echo trna 51459255
} | while read type gi; do
    for x in "" ".table"; do
        for cmd in echo "bash -c"; do
            $cmd "./objmgr_demo -loader - -id \"gnl|Annot:${type%[0-9]}|$gi\" -file $d/annot.$type$x.asn -externals -count_types -count_subtypes -print_features -only_features | sed 's/ in [0-9][^ ]* secs$//' > scan$x.txt" || exit 1
        done
    done
    echo diff scan.txt scan.table.txt
    diff scan.txt scan.table.txt || exit 1
    echo $type is correct
done

#! /bin/sh
#$Id$
err=0
for acc in AAAA AAAA01 AAAA02 ALWZ AINT AAAA01000001 AAAA01999999; do
    wgs_test -file "$acc" || err="$?"
done
echo "Test lookups"
wgs_test -file "AAAA01" -limit_count 1 -scaffold_name scaffold209197004 -contig_name contig209197004_1 -protein_name SEET0368_00020 -protein_acc EDT30481 -gi 25561846 || err="$?"
wgs_test -file "AABR01" -limit_count 1 -check_non_empty_lookup -gi-range -gi 25561846 || err="$?"
wgs_test -file "ALWZ01" -limit_count 1 -scaffold_name scaffold209197004x -contig_name contig209197004_1x || err="$?"
wgs_test -file "ALWZ01" -limit_count 1 -check_non_empty_lookup -scaffold_name scaffold209197004 -contig_name contig209197004_1 || err="$?"
PFILE="/home/dondosha/trunk/internal/c++/src/internal/ID/WGS/JTED01"
if test -r "$PFILE"; then
    echo "Testing PROTEIN table in $PFILE"
    wgs_test -file "$PFILE" -limit_count 1 -scaffold_name scaffold209197004x -contig_name contig209197004_1x -protein_name SEET0368_00020x -protein_acc EDT30481x || err="$?"
    wgs_test -file "$PFILE" -limit_count 1 -check_non_empty_lookup -protein_name SEET0368_00020 -protein_acc EDT30481 || err="$?"
fi
PFILE="/home/dondosha/TEST/AAAL02"
if test -r "$PFILE"; then
    echo "Testing PROTEIN table in $PFILE"
    wgs_test -file "$PFILE" -limit_count 1 -contig_name CTG92x -protein_name XFASADRAFT_1728x -protein_acc EAO13866x || err="$?"
    wgs_test -file "$PFILE" -limit_count 1 -check_non_empty_lookup -contig_name CTG92 -protein_name XFASADRAFT_1728 -protein_acc EAO13866 || err="$?"
    #wgs_test -file "$PFILE" -limit_count 1 -check_non_empty_lookup -contig_name ctg92 -protein_name XfasaDRAFT_1728 -protein_acc eao13866 || err="$?"
fi
exit "$err"

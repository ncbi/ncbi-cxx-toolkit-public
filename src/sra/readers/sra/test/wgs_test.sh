#! /bin/sh
#$Id$
err=0

do_test() {
    echo "Running:"
    echo wgs_test "$@"
    wgs_test "$@" || err="$?"
}

for acc in AAAA AAAA01 AAAA02 ALWZ AINT AAAA01000001 AAAA01999999; do
    do_test -file "$acc"
done

echo "Test lookups"
do_test -file "AAAA01" -limit_count 1  -check_empty_lookup -scaffold_name scaffold209197004 -contig_name contig209197004_1 -protein_name SEET0368_00020 -protein_acc EDT30481 -gi 25561846
do_test -file "AABR01" -limit_count 1 -suppressed -check_non_empty_lookup -gi-range -gi 25561846
do_test -file "ALWZ01" -limit_count 1 -suppressed -check_empty_lookup -scaffold_name scaffold209197004x -contig_name contig209197004_1x
do_test -file "ALWZ01" -limit_count 1 -suppressed -check_non_empty_lookup -scaffold_name scaffold209197004 -contig_name contig209197004_1

PFILE="JTED01"
echo "Testing PROTEIN table in $PFILE"
do_test -file "$PFILE" -limit_count 1 -check_empty_lookup -scaffold_name scaffold209197004x -contig_name contig209197004_1x -protein_name SEET0368_00020x -protein_acc EDT30481x
do_test -file "$PFILE" -limit_count 1 -suppressed -check_non_empty_lookup -contig_name CFSAN001197_contig0053 -protein_name SEET0368_00020 -protein_acc khd84007

PFILE="AAAL02"
echo "Testing PROTEIN table in $PFILE"
do_test -file "$PFILE" -limit_count 1 -check_empty_lookup -contig_name CTG92x -protein_name XFASADRAFT_1728x -protein_acc EAO13866x
do_test -file "$PFILE" -limit_count 1 -check_non_empty_lookup -contig_name ctg92 -protein_name xFASADRAFT_1728 -protein_acc eao13869

PFILE="BACI01"
echo "Testing PROTEIN table in $PFILE"
do_test -file "$PFILE" -limit_count 1 -check_empty_lookup -contig_name contig222 -protein_name H0262_000450 -protein_acc GAA10774.3
do_test -file "$PFILE" -limit_count 1 -check_empty_lookup -contig_name contig333 -protein_name gaa107777 -protein_acc gaa10774.1
do_test -file "$PFILE" -limit_count 1 -check_non_empty_lookup -contig_name conTig3 -protein_name gaA10776 -protein_acc gAA10774.2
do_test -file "$PFILE" -limit_count 1 -check_non_empty_lookup -contig_name Contig4 -protein_name Gaa10775 -protein_acc Gaa10774

PFILE="JACDXZ01"
echo "Testing PROTEIN table in $PFILE"
do_test -file "$PFILE" -limit_count 1 -check_empty_lookup -contig_name 111D_2_scaffold333 -protein_name H0262_000450 -protein_acc MBA2057862.3
do_test -file "$PFILE" -limit_count 1 -check_non_empty_lookup -contig_name 111d_2_scaffold3 -protein_name h0262_00045 -protein_acc mBA2057862.1
do_test -file "$PFILE" -limit_count 1 -check_non_empty_lookup -contig_name 111D_2_SCAFFOLD4 -protein_name h0262_00055 -protein_acc mBA2057862.2
do_test -file "$PFILE" -limit_count 1 -check_non_empty_lookup -contig_name 111D_2_scaffold5 -protein_name H0262_00060 -protein_acc mBA2057862


if test "$err" = 0; then
    echo "Passed all tests."
else
    echo "Failed some tests!"
fi
exit "$err"

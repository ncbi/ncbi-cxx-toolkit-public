#! /bin/sh
# $Id$
#
unset GENBANK_ID1_STATS
test_dir="./data"
tmp_out=$test_dir/out.asn
return_status=0

run_prg() {
  $CHECK_EXEC alnmrg -in $test_dir/$1.asn -asnoutb $tmp_out -dsout t $2
  if test "$?" != 0; then
      echo "FAILURE:"
      return_status=1
  fi
}

check_diff() {
  cmp -s $test_dir/$1.asn $tmp_out
  if test "$?" != 0; then
      echo "FAILURE: $2"
      return_status=1
  fi
}  

# in.asn == out.asn
test1() {
  run_prg "$1" "$2"
  check_diff "$1" "$3"
}

# in.asn != out.asn
test2() {
  run_prg "$1" "$2"
  check_diff "$1.out" "$3"
}



#### TEST CASES #####


# fill unaligned
test1 unaln "-fillunaln t -noobjmgr t" \
"test that fillunaln does not affect this alignment"


# gi 6467445
test2 independent_dss "-b Seq-entry" \
"test independent dss"


# gi 19880863
test2 gapjoin "-gapjoin t -b Seq-entry" \
"test gap join"


# gi 19172277
test2 19172277 "-b Seq-entry" "test gi 19172277"

test1 multiple_row_inserts "" "multiple row inserts"

test2 iterator_minus_minus "" "iterator minus minus"


# test2 blast "-queryseqmergeonly t" "blast output merge"
test2 seg_overlap "" "overlapping segments"
test2 trunc1 "-truncateoverlaps t -noobjmgr t" "truncation of overlaps 1"
test2 trunc2 "-truncateoverlaps t -noobjmgr t" "truncation of overlaps 2"
test2 trunc3 "-truncateoverlaps t -noobjmgr t" "truncation of overlaps 3"
test2 trunc4 "-truncateoverlaps t -noobjmgr t" "truncation of overlaps 4"
test2 trunc5 "-truncateoverlaps t -noobjmgr t" "truncation of overlaps 5"
test2 trunc6 "-truncateoverlaps t -noobjmgr t" "truncation of overlaps 6"
test2 trunc7 "-truncateoverlaps t -noobjmgr t" "truncation of overlaps 7"
test2 trunc8 "-truncateoverlaps t -noobjmgr t" "truncation of overlaps 8"
test2 framed_refseq "" "query sequence is on diff frames w/ overlaps on subject"
test2 trunc9 "-truncateoverlaps t -noobjmgr t" "truncation with mixed strands on subject"

# test preservation of rows
test1 preserve_rows "-preserverows t" "preservation of rows"
test2 preserve_rows_set "-noobjmgr t -preserverows t -truncateoverlaps t" \
"preserve rows and truncate overlaps"


test2 frames_with_overlaps "-noobjmgr t -truncateoverlaps t" "frames with truncation of overlaps"


test2 refseq_swap "" "the common sequence (refseq) comes second"


test2 best_reciprocal_hits "-queryseqmergeonly t -truncateoverlaps t -noobjmgr t" "best reciprocal hits"


# clean
rm $tmp_out

echo "Done!"
exit $return_status

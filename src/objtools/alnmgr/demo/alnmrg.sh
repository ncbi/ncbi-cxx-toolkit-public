#! /bin/sh
# $Id$
#
unset GENBANK_ID1_STATS
test_dir="./data"
tmp_out=$test_dir/out.asn
return_status=0

run_prg() {
  $CHECK_EXEC alnmrg -in $test_dir/$1.asn $2 > $tmp_out
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

rm $tmp_out

echo "Done!"
exit $return_status

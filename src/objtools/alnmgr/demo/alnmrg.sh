#! /bin/sh
# $Id$
#

prg="./alnmrg"
tmp_out="tst/out.asn"
return_status=0

run_prg() {
  $prg -in tst/$1.asn $2 > $tmp_out
}

check_diff() {
  cmp -s tst/$1.asn $tmp_out
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

test1 unaln "-fillunaln t -noobjmgr t" "test that fillunaln does not affect this alignment"

# gi 6467445
test2 independent_dss "" "test independent dss"

# gi 19880863
test2 gapjoin "-gapjoin t" "test gap join"

# gi 19172277
test2 19172277 "" "test gi 19172277"

test1 multiple_row_inserts "" "multiple row inserts"

test2 iterator_minus_minus "" "iterator minus minus"

rm $tmp_out
echo "Done!"
exit $return_status

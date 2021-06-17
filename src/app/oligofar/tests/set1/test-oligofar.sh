#!/bin/sh
errors=''
ok=""
function run () {
	test="$1"
	shift
	echo -e "\n\e[33m$test\t\e[34m" "$@" "\e[0m" >&2
	"$@"
	rc=$?
	if test "$rc" != 0 ; then
		errors="$errors$test failed;"
	fi
	return $?
}
run "Version" ./oligofar -V

run "Single Reads" ./oligofar -T+ -i NM_012345.reads -d NM_012345.fa -o NM_012345.reads.out -w9 -n1 -L2G "$@" 
run "Single Reads diff" diff NM_012345.reads.ref NM_012345.reads.out > NM_012345.reads.diff

run "Paired Reads" ./oligofar -T+ -i NM_012345.pairs -d NM_012345.fa -o NM_012345.pairs.out -w9 -n1 -L2G -m50 "$@"
run "Paired Reads diff" diff NM_012345.pairs.ref NM_012345.pairs.out > NM_012345.pairs.diff

x=$(run "2mism+1gap" ./oligofar -i test1.reads -d NM_012345.fa -Ou -n2 -e1 | tee test1.reads.out | awk '{print $14}' | sort | uniq -c)
run "2mism+1gap result" test "$x" = '     30 AGCCTTCCTCTGAGTCGCAGCCCCCCATGGAGGCCCAGTCT'

x=$(run "Strides" ./oligofar -i test2.reads -d NM_012345.fa -Ou -S6 -n2 -e1 | tee test2.reads.out | awk '{print $6}' | sort | uniq -c)
run "Strides result" test "$x" = '     22 100'

for R in solexa outside solid decr ; do 
	echo "R=$R"; run "Orientation $R" ./oligofar -i test3.pairs -d NM_012345.fa -D400-600 -R $R |awk '($6>100)' ; 
done > test3.pairs.out
run "Orientation diff" diff test3.pairs.ref test3.pairs.out > test3.pairs.diff

x=$(run "Pairs Length" ./oligofar -i test4.pairs -d NM_012345.fa -Ou -w10 -D80 -m 9|awk '($6==100)'|wc -l)
run "Pairs Length result" test "$x" = '4'

if test "$errors" == "$ok" ; then
	rm NM_012345.reads.{out,diff} NM_012345.pairs.{out,diff}
else
	head NM_012345.{reads,pairs}.diff
fi

echo -e "\x1b[31m$errors\x1b[0m" | tr ';' "\n"
if test "$errors" == "$ok" ; then echo -e "\x1b[32mOK\x1b[0m" ; fi
test "$errors" == "$ok"

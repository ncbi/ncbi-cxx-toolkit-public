#!/bin/bash

TEMP=`getopt -o hV1:2:o:T:b:d:q:x: --long help,debug,ref: -n 'bmtagger' -- "$@"`

if [ $? != 0 ] ; then echo "Terminating..." >&2 ; exit 1 ; fi

eval set -- "$TEMP"

quality=0
reference=''
blastdb=''
srindex=''
bmfile=''
input1=''
input2=''
output=/dev/stdout
done=0
debug=0

: ${BMFILTER:=bmfilter}
: ${BLASTN:=blastn}
: ${SRPRISM:=srprism}
: ${TMPDIR:=/tmp}

function show_help () {
	echo "usage: bmtagger [-hV] [-q 0|1] -1 input.fa [-2 matepairs.fa] -b genome.wbm -d genome-seqdb -x srindex [-o blacklist] [-T tmpdir]"
	echo "usage: bmtagger [-hV] [-q 0|1] -1 input.fa [-2 matepairs.fa] --ref=reference [-o blacklist] [-T tmpdir]"
	echo "use --ref=name to point to .wbm, seqdb and srprism index if they have the same path and basename"
	echo "use --debug to leave temporary data on exit"
	done=1
}

function show_version () {
	echo "version 0.0.0"
	done=1
}

function finalize () {
	if [ $debug == 0 ] ; then rm -fr "$TMPDIR"/bmtagger.$$.* ; fi
	if [ "$1" == "" ] ; then exit 100 ; else exit "$1" ; fi
}

while true ; do
	case "$1" in
	-h|--help) show_help ; shift ;;
	-V) show_version ; shift ;;
	--debug) debug=1 ; shift ;;
	--ref) reference="$2" ; shift 2 ;;
	-1) input1="$2" ; shift 2 ;;
	-2) input2="$2" ; shift 2 ;;
	-o) output="$2" ; shift 2 ;;
	-b) bmfile="$2" ; shift 2 ;;
	-d) blastdb="$2" ; shift 2 ;;
	-x) srindex="$2" ; shift 2 ;;
	-q) quality="$2" ;  shift 2 ;;
	-T) TMPDIR="$2" ; shift 2 ;;
	--) break ;;
	*) echo "Unknown option $1 $2" >&2 ; exit 1 ;;
	esac
done

if [ $done == 1 ] ; then exit 0 ; fi

if [ -n "$input2" ] ; then input2="-2$input2" ; fi ## NB: don't put space after -2

if [ "$output" == "-" ] ; then output="/dev/stdout" ; fi

case "$output" in
	/dev/*) tmpout="$output" ;;
	*) tmpout="$output~" ;;
esac

test -z "$srindex" && srindex="$reference"
test -z "$blastdb" && blastdb="$reference"
test -z "$bmfile"  && bmfile="$reference".wbm

echo "RUNNING $0 (PID=$$)" >&2
trap finalize INT TERM USR1 USR2 HUP

$BMFILTER -1"$input1" "$input2" -TP -lN5 -q "$quality" -b "$bmfile" -o "$TMPDIR"/bmtagger.$$
rc=$?

if [ $rc != 0 ] ; then
	echo "FAILED: bmfilter with rc=$rc" >&2 ; 
	finalize 2
fi

function align_long () {
	test -s "$1".fa || return 0
	echo "RUNNING align_long for '$1'" >&2
	$BLASTN \
		-task megablast \
		-db "$blastdb" \
		-query "$1".fa \
		-out   "$1".bn \
		-outfmt 6 \
		-word_size 16 \
		-best_hit_overhang 0.1 \
		-best_hit_score_edge 0.1 \
		-use_index true \
		-index_name "$blastdb"
	rc=$?
	if [ $rc != 0 ] ; then echo "FAILED: blastn for $1" >&2 ; finalize 3 ; fi
	## blastn filter criteria: $4 = hitLength, $3 = %id, $1 = readID
	awk '($4 >= 90 || ($4 >= 50 && $3 >= 90)) { print $1 }' "$1".bn > "$1".lst
	rc=$?
	if [ $rc != 0 ] ; then echo "FAILED: awk for blastn results for $1" >&2 ; finalize 4 ; fi
	return $rc
}

function align_short () {
	test -s "$1".fa || return 0
	echo "RUNNING align_short for '$1'" >&2
	$SRPRISM search \
		--trace-level info \
		-I "$srindex" \
		-i "$1".fa \
		-o "$1".srprism \
		-O tabular \
		-n 2 -R 0 -r 2 -M 7168 -T "$TMPDIR"
	#-M 15360 
	rc=$?
	if [ $rc != 0 ] ; then echo "FAILED: srprism for $1" >&2 ; finalize 5 ; fi
	## srprism filter criteria: everything, $2 = readID
	awk '{ print $2 }' "$1".srprism > "$1".lst
	rc=$?
	if [ $rc != 0 ] ; then echo "FAILED: awk for srprism results for $1" >&2 ; finalize 6 ; fi
	return $rc
}

function append () {
	test -s "$2" || return 0
	uniq "$2" >> "$1"
	rc=$?
	if [ $rc != 0 ] ; then echo "FAILED: cat $2 >> $1 with rc=$?" >&2 ; finalize 7 ; fi
	return $rc
}

align_short "$TMPDIR"/bmtagger.$$.short
align_short "$TMPDIR"/bmtagger.$$.short2

align_long "$TMPDIR"/bmtagger.$$.long
align_long "$TMPDIR"/bmtagger.$$.long2

awk '($2 == "H") { print $1 }' "$TMPDIR"/bmtagger.$$.tag >> "$tmpout"
rc=$?
if [ $rc != 0 ] ; then echo "FAILED: awk for tagfile" >&2 ; finalize 8; fi

append "$tmpout" "$TMPDIR"/bmtagger.$$.short.lst
append "$tmpout" "$TMPDIR"/bmtagger.$$.short2.lst
append "$tmpout" "$TMPDIR"/bmtagger.$$.long.lst
append "$tmpout" "$TMPDIR"/bmtagger.$$.long2.lst

test "$tmpout" != "$output" && mv "$tmpout" "$output"

echo "DONE $0 (PID=$$)" >&2
finalize 0


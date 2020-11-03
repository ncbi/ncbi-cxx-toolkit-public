#!/bin/env bash

usage() {
    echo "USAGE:"
    echo "$0  [-h]  -i input_file  [-o output_file]  [-logfile log_file]  -url URL"
    exit 0
}

(( $# == 0 )) && usage

cdir=$(dirname $0)

while (( $# )); do
    case $1 in
        -i)
            # No input file parameter so just suppress it
            # (( $# > 1 )) || usage
            # shift
            # ifile=$1
            ;;
        -o)
            (( $# > 1 )) || usage
            shift
            ofile=$1
            ;;
        -logfile)
            (( $# > 1 )) || usage
            shift
            logfile=$1
            ;;
        -url)
            (( $# > 1 )) || usage
            shift
            url=$1
            ;;
        -server)
            (( $# > 1 )) || usage
            shift
            server=$1
            ;;
        -h|*) # Help
            usage
            ;;
    esac
    shift
done


#[[ -n "$ifile" ]] || usage
#[[ -r "$ifile" ]] || usage
[[ -n "$url"   ]] || usage


# The name is like /....../<test_id>/candidate_file
odirname=$(dirname $ofile)
obasename=$(basename $odirname)

full_url="http://${server}/${url}"

if [[ $url == ADMIN* ]] && [[ $obasename != admin_ack_alert* ]]; then
    curl -I --HEAD -s -i "${full_url}" | grep -v '^Date: ' | grep -v '^Server: ' | grep -v '^Content-Length: ' | ${cdir}/printable_string encode --exempt 92,10,13 -z > $ofile
    exit 0
fi

TESTS="many_chunk_blob get_auto_blob_skipping_no2 get_auto_blob_skipping_no1 get_115952497_3 get_115952497__12_3 get_with_exclude_not_excluded get_115952497_2 get_115952497__12_2 get_fasta_bbm_164040 get_fasta_bbm_164040__3 get_AC052670 get_AC052670__5 get_115952497 get_115952497__12 get_164040__3 get_invalid_exclude_blob comment_basic_suppressed comment_basic_diff_suppressed comment_basic_withdrawn comment_withdrawn_default"
if echo $TESTS | grep -w $obasename > /dev/null; then
    # The chunks may come in an arbitrary order so diff may fail
    curl -s -i "${full_url}" | grep --text '^PSG-Reply-Chunk: ' | sed  -r 's/item_id=[0-9]+&//g' | sort > $ofile
    exit 0
fi

TESTS="get_splitted_tse_whole get_splitted_tse_orig get_non_splitted_tse_smart get_non_splitted_tse_whole get_non_splitted_tse_orig get_blob_splitted_tse_whole get_blob_splitted_tse_orig get_blob_non_splitted_tse_smart get_blob_non_splitted_tse_whole get_blob_non_splitted_tse_orig"
if echo $TESTS | grep -w $obasename > /dev/null; then
    # The chunks may come in arbitrary order so diff may fail
    curl -s -i "${full_url}" | grep --text '^PSG-Reply-Chunk: ' | sed  -r 's/item_id=[0-9]+&//g' | sort > $ofile
    exit 0
fi

# The most common case
curl -s -i "${full_url}" | grep --text -v '^Date: ' | grep --text -v '^Server: ' | ${cdir}/printable_string encode --exempt 92,10,13 -z > $ofile
exit 0

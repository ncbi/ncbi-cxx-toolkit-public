#!/bin/env bash

usage() {
    echo "USAGE:"
    echo "$0  [-h]  -i input_file  [-o output_file]  [-logfile log_file]  -url URL"
    exit 0
}

(( $# == 0 )) && usage

cdir=$(dirname $0)
https="False"

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
        -https)
            (( $# > 1 )) || usage
            shift
            https=$1
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
curl_https=""
if [[ "${https}" == "True" ]]; then
    full_url="https://${server}/${url}"
    curl_https="-k"
fi

if [[ $url == ADMIN* ]] && [[ $obasename != admin_ack_alert* ]]; then
    curl "${curl_https}" -I --HEAD -s -i "${full_url}" | grep -v '^Date: ' | grep -v '^Server: ' | grep -v '^Content-Length: ' | ${cdir}/printable_string encode --exempt 92,10,13 -z > $ofile
    exit 0
fi

TESTS="many_chunk_blob get_auto_blob_skipping_no2 get_auto_blob_skipping_no1
       get_115952497_3 get_115952497__12_3 get_with_exclude_not_excluded
       get_115952497_2 get_115952497__12_2 get_fasta_bbm_164040
       get_fasta_bbm_164040__3 get_AC052670 get_AC052670__5 get_115952497
       get_115952497__12 get_164040__3 get_invalid_exclude_blob
       comment_basic_suppressed comment_basic_diff_suppressed
       comment_basic_withdrawn comment_withdrawn_default
       get_blob_split_tse_smart get_blob_split_tse_slim get_split_tse_smart
       get_split_tse_slim
       get_split_tse_whole get_split_tse_orig get_non_split_tse_smart
       get_non_split_tse_whole get_non_split_tse_orig get_blob_split_tse_whole
       get_blob_split_tse_orig get_blob_non_split_tse_smart
       get_blob_non_split_tse_whole get_blob_non_split_tse_orig
       get_slim_send_if_small_50 get_slim_send_if_small_50000
       get_smart_send_if_small_50 get_smart_send_if_small_50000
       getblob_slim_send_if_small_50 getblob_slim_send_if_small_50000
       getblob_smart_send_if_small_50 getblob_smart_send_if_small_50000
       getna_slim_send_if_small_50 getna_slim_send_if_small_50000
       getna_smart_send_if_small_50 getna_smart_send_if_small_50000
       getna_slim_send_if_small_50_2 getna_slim_send_if_small_50000_2
       getna_smart_send_if_small_50_2 getna_smart_send_if_small_50000_2
       getna_auto_exclude_1 getna_auto_exclude_2"
if echo $TESTS | grep -w $obasename > /dev/null; then
    # The chunks may come in an arbitrary order so diff may fail
    curl "${curl_https}" -s -i "${full_url}" | grep --text '^PSG-Reply-Chunk: ' | sed  -r 's/item_id=[0-9]+&//g' | sort > $ofile
    exit 0
fi

# If there are two processors of the same type they will produce double output
# in case of expected errors and may produce double output in case of named
# annotations. So filter those test cases in a special way
TESTS="pdb_1_5 pdb_1_6 pdb_2_5 pdb_2_6 pdb_3_5 pdb_3_6 pdb_4_5 pdb_4_6
       non_existing_seq_id_no_cass get_incorrect_prim_seq_id_type_no_cass
       get_incorrect_sec_seq_id_type_no_cass tse_chunk_chunk_too_big
       non_existing_seq_id_1 invalid_seq_id_type_1
       invalid_primary_seq_id non_exsting_sat get_non_existing_seq_id
       get_incorrect_sec_seq_id_type get_incorrect_prim_seq_id_type
       get_na_two_valid_annot get_na_one_of_two_valid_annot get_na_one_valid_annot"
if echo $TESTS | grep -w $obasename > /dev/null; then
    curl "${curl_https}" -s -i "${full_url}" | grep -v '^Date: ' | grep -v '^Server: ' | grep -v '^Content-Length: ' | sed -e 's/\r$//' | grep . | sed  -r 's/item_id=[0-9]+&//g' | sed -r 's/&n_chunks=[0-9]+//g' | sort | uniq > $ofile
    exit 0
fi

# The most common case
curl "${curl_https}" -s -i "${full_url}" | grep --text -v '^Date: ' | grep --text -v '^Server: ' | ${cdir}/printable_string encode --exempt 92,10,13 -z > $ofile
exit 0

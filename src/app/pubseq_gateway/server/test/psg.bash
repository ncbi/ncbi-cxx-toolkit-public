#!/bin/env bash

usage() {
    echo "USAGE:"
    echo "$0  [-h]  -i input_file  [-o output_file]  [-logfile log_file]  -url URL"
    exit 0
}

(( $# == 0 )) && usage

cdir=$(dirname $0)
https="False"
cookie=""

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
        -cookie)
            (( $# > 1 )) || usage
            shift
            cookie=$1
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

TESTS="wrong_no_series_statistics no_end_statistics unsorted_statistics no_division_statistics"
if echo $TESTS | grep -w $obasename > /dev/null; then
    curl "${curl_https}" -s -i "${full_url}" | grep --text -v '^HTTP/' | grep --text -v '^Connection: keep-alive' | grep --text -v '^transfer-encoding: chunked' | grep --text -v -i '^Date: ' | grep --text -v -i '^Server: ' | sed  -r 's/exec_time=[0-9]+/exec_time=/g' | sed  -r 's/sent_seconds_ago=[0-9]+.[0-9]+/sent_seconds_ago=/g' | sed  -r 's/time_until_resend=[0-9]+.[0-9]+/time_until_resend=/g' | ${cdir}/printable_string encode --exempt 92,10,13 -z > $ofile
    exit 0
fi


if [[ $url == ADMIN* ]] && [[ $obasename != admin_ack_alert* ]]; then
    curl "${curl_https}" -I --HEAD -s -i "${full_url}" | grep --text -v '^HTTP/' | grep --text -v '^content-length:' | grep --text -v '^Connection: keep-alive' | grep --text -v '^transfer-encoding: chunked' | grep --text -v -i '^Date: ' | grep --text -v -i '^Server: ' | grep -v '^Content-Length: ' | sed  -r 's/exec_time=[0-9]+/exec_time=/g' | ${cdir}/printable_string encode --exempt 92,10,13 -z > $ofile
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
       getna_auto_exclude_1 getna_auto_exclude_2
       acc_ver_hist_1 acc_ver_hist_2 proper_skip_1 proper_skip_2
       extra_chunk_44_info_size_1 no_extra_chunk_info_size_1
       no_extra_chunk_info_size_5 extra_chunk_266_info_size_5
       extra_chunk_266_info_size_5_gi get_resend_timeout_setup get_resend_timeout_short
       osg_getchunk_ok"
if echo $TESTS | grep -w $obasename > /dev/null; then
    # The chunks may come in an arbitrary order so diff may fail
    curl "${curl_https}" -s -i "${full_url}" | grep --text '^PSG-Reply-Chunk: ' | sed  -r 's/exec_time=[0-9]+/exec_time=/g' | sed  -r 's/item_id=[0-9]+&//g' | sed  -r 's/sent_seconds_ago=[0-9]+.[0-9]+/sent_seconds_ago=/g' | sed  -r 's/time_until_resend=[0-9]+.[0-9]+/time_until_resend=/g' | sort > $ofile
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
       get_na_two_valid_annot get_na_one_of_two_valid_annot get_na_one_valid_annot
       get_na_two_valid_annot_sep1 get_na_two_valid_annot_sep2 get_na_two_valid_annot_sep3
       get_na_two_valid_annot_dup get_na_two_valid_annot_dup2"
if echo $TESTS | grep -w $obasename > /dev/null; then
    curl "${curl_https}" -s -i "${full_url}" | grep --text -v '^HTTP/' | grep --text -v '^Connection: keep-alive' | grep --text -v '^transfer-encoding: chunked' | grep --text -v -i '^Date: ' | grep --text -v -i '^Server: ' | grep -v '^Content-Length: ' | sed -e 's/\r$//' | grep . | sed  -r 's/exec_time=[0-9]+/exec_time=/g' | sed  -r 's/item_id=[0-9]+&//g' | sed -r 's/&n_chunks=[0-9]+//g' | sed  -r 's/sent_seconds_ago=[0-9]+.[0-9]+/sent_seconds_ago=/g' | sed  -r 's/time_until_resend=[0-9]+.[0-9]+/time_until_resend=/g' | sort | uniq > $ofile
    exit 0
fi

# The fallback happens due to various chunks so the only final status is to be
# checked
TESTS="no_id2chunk_fallback_whole_7 no_id2chunk_prop_fallback_whole_8"
if echo $TESTS | grep -w $obasename > /dev/null; then
    curl "${curl_https}" -s -i "${full_url}" | grep --text 'status=200' | sed -r 's/&n_chunks=[0-9]+//g' | sed  -r 's/chunk=[0-9]+/chunk=/g' | sed  -r 's/exec_time=[0-9]+/exec_time=/g' | sed  -r 's/item_id=[0-9]+&//g' | sed  -r 's/sent_seconds_ago=[0-9]+.[0-9]+/sent_seconds_ago=/g' | sed  -r 's/time_until_resend=[0-9]+.[0-9]+/time_until_resend=/g' | sort > $ofile
    exit 0
fi



# Need the fact that it is a fallback or an exception
TESTS="bad_id2chunk_fallback_orig bad_id2chunk_fallback_whole
       bad_id2chunk_fallback_orig_2 bad_id2chunk_fallback_whole_2
       getna_id2chunk_fallback_slim getna_id2chunk_fallback_smart
       getna_id2chunk_fallback_smart_2 getna_id2chunk_fallback_slim_2
       bad_id2chunk_fallback_whole_4 bad_id2chunk_fallback_whole_3
       getna_id2chunk_prop_fallback_smart_2 getna_id2chunk_prop_fallback_slim_2
       bad_id2chunk_prop_fallback_whole_4 bad_id2chunk_prop_fallback_whole_3"
if echo $TESTS | grep -w $obasename > /dev/null; then
    # The chunks may come in an arbitrary order so diff may fail
    curl "${curl_https}" -s -i "${full_url}" | grep --text -e 'Falling' -e 'Fallback' -e 'Callback error' -e 'Failed to fetch' -e 'Exception' | sed  -r 's/chunk=[0-9]+/chunk=/g' | sed  -r 's/exec_time=[0-9]+/exec_time=/g' | sed  -r 's/item_id=[0-9]+&//g' | sed  -r 's/sent_seconds_ago=[0-9]+.[0-9]+/sent_seconds_ago=/g' | sed  -r 's/time_until_resend=[0-9]+.[0-9]+/time_until_resend=/g' | sed  -r 's/900.50032421/900.500324xx/g' | sed  -r 's/900.50032468/900.500324xx/g' | sed  -r 's/900.50022421/900.500224xx/g' | sed  -r 's/900.50022468/900.500224xx/g' | sed  -r 's/eInvalidId2Info.*id2info.cpp//g' | sort > $ofile
    exit 0
fi


# Need psg chunks and the fact that it is a fallback
TESTS="bad_id2chunk_prop_fallback_slim bad_id2chunk_prop_fallback_smart
       bad_id2chunk_prop_fallback_whole bad_id2chunk_prop_fallback_none
       bad_id2chunk_prop_fallback_orig
       bad_id2chunk_prop_fallback_slim_2 bad_id2chunk_prop_fallback_smart_2
       bad_id2chunk_prop_fallback_whole_2 bad_id2chunk_prop_fallback_none_2
       bad_id2chunk_prop_fallback_orig_2
       bad_id2chunk_prop_fallback_slim_3 bad_id2chunk_prop_fallback_smart_3
       bad_id2chunk_prop_fallback_none_3
       bad_id2chunk_prop_fallback_orig_3
       bad_id2chunk_prop_fallback_slim_4 bad_id2chunk_prop_fallback_smart_4
       bad_id2chunk_prop_fallback_none_4
       bad_id2chunk_prop_fallback_orig_4
       bad_id2chunk_fallback_slim bad_id2chunk_fallback_smart
       bad_id2chunk_fallback_none
       bad_id2chunk_fallback_slim_2 bad_id2chunk_fallback_smart_2
       bad_id2chunk_fallback_none_2
       bad_id2chunk_fallback_slim_3 bad_id2chunk_fallback_smart_3
       bad_id2chunk_fallback_none_3
       bad_id2chunk_fallback_orig_3
       bad_id2chunk_fallback_slim_4 bad_id2chunk_fallback_smart_4
       bad_id2chunk_fallback_none_4
       bad_id2chunk_fallback_orig_4
       getna_bad_id2info_fallback_slim getna_bad_id2info_fallback_smart
       getna_bad_id2info_fallback_whole getna_bad_id2info_fallback_orig
       getna_bad_id2info_fallback_none
       getna_id2chunk_prop_fallback_slim getna_id2chunk_prop_fallback_smart
       getna_id2chunk_prop_fallback_whole getna_id2chunk_prop_fallback_orig
       getna_id2chunk_prop_fallback_none
       getna_id2chunk_prop_fallback_whole_2 getna_id2chunk_prop_fallback_orig_2
       getna_id2chunk_prop_fallback_none_2
       getna_id2chunk_fallback_whole getna_id2chunk_fallback_orig
       getna_id2chunk_fallback_none
       getna_id2chunk_fallback_whole_2 getna_id2chunk_fallback_orig_2
       getna_id2chunk_fallback_none_2
       no_id2chunk_fallback_slim_7 no_id2chunk_fallback_smart_7
       no_id2chunk_fallback_none_7
       no_id2chunk_fallback_orig_7
       no_id2chunk_prop_fallback_slim_8 no_id2chunk_prop_fallback_smart_8
       no_id2chunk_prop_fallback_none_8 no_id2chunk_prop_fallback_orig_8"
if echo $TESTS | grep -w $obasename > /dev/null; then
    # The chunks may come in an arbitrary order so diff may fail
    curl "${curl_https}" -s -i "${full_url}" | grep --text -e '^PSG-Reply-Chunk: ' -e 'Falling' -e 'Fallback' -e 'Callback error' -e 'Failed to fetch' | sed  -r 's/size=[0-9]+/size=/g' | sed  -r 's/exec_time=[0-9]+/exec_time=/g' | sed  -r 's/item_id=[0-9]+&//g' | sed  -r 's/sent_seconds_ago=[0-9]+.[0-9]+/sent_seconds_ago=/g' | sed  -r 's/time_until_resend=[0-9]+.[0-9]+/time_until_resend=/g' | sort > $ofile
    exit 0
fi

# Need to handle an exception and a size of that message
TESTS="bad_id2info_fallback_slim bad_id2info_fallback_smart bad_id2info_fallback_whole
       bad_id2info_fallback_orig bad_id2info_fallback_none
       bad_id2info_fallback_slim_2 bad_id2info_fallback_smart_2 bad_id2info_fallback_whole_2
       bad_id2info_fallback_orig_2 bad_id2info_fallback_none_2"
if echo $TESTS | grep -w $obasename > /dev/null; then
    # The chunks may come in an arbitrary order so diff may fail
    curl "${curl_https}" -s -i "${full_url}" | grep --text -v '^HTTP/' | grep --text -v '^Connection: keep-alive' | grep --text -v '^transfer-encoding: chunked' | grep --text -v -i '^Date: ' | grep --text -v -i '^Server: ' | sed -r 's/line [0-9]+/line xx/g' | sed  -r 's/exec_time=[0-9]+/exec_time=/g' | sed  -r 's/sent_seconds_ago=[0-9]+.[0-9]+/sent_seconds_ago=/g' | sed  -r 's/time_until_resend=[0-9]+.[0-9]+/time_until_resend=/g' | sed  -r 's/eInvalidId2Info.*id2info.cpp//g' | sed  -r 's/size=[0-9]+/size=/g' | ${cdir}/printable_string encode --exempt 92,10,13 -z > $ofile
    exit 0
fi


TESTS="readyz_exclude healthz readyz_snp_no_verbose readyz_cdd_no_verbose
       readyz_wgs_no_verbose readyz_lmdb_no_verbose readyz_cassandra_no_verbose
       readyz_no_verbose readyz_snp readyz_cdd readyz_wgs readyz_lmdb
       readyz_cassandra readyz livez_no_verbose livez hello_bad_user_agent
       hello_bad_id hello_no_id hello_no_user_agent hello_ok"
if echo $TESTS | grep -w $obasename > /dev/null; then
    # in addition to the most common case the 'content length' needs to be
    # stripped because the http outputs it starting with a capital letter while
    # https outputs it in all small letters
    curl "${curl_https}" -s -i "${full_url}" | grep --text -v '^HTTP/' | grep --text -v '^Connection: keep-alive' | grep --text -v '^transfer-encoding: chunked' | grep --text -v -i '^Date: ' | grep --text -v -i '^Server: ' | grep --text -v -i '^Content-Length: ' | sed  -r 's/exec_time=[0-9]+/exec_time=/g' | sed  -r 's/sent_seconds_ago=[0-9]+.[0-9]+/sent_seconds_ago=/g' | sed  -r 's/time_until_resend=[0-9]+.[0-9]+/time_until_resend=/g' | ${cdir}/printable_string encode --exempt 92,10,13 -z > $ofile
    exit 0
fi

# The most common case
if [[ "${cookie}" != "" ]]; then
    curl "${curl_https}" -s --cookie "WebCubbyUser=\"${cookie}\"" -i "${full_url}" | grep --text -v '^HTTP/' | grep --text -v '^Connection: keep-alive' | grep --text -v '^transfer-encoding: chunked' | grep --text -v -i '^Date: ' | grep --text -v -i '^Server: ' | sed  -r 's/exec_time=[0-9]+/exec_time=/g' | sed  -r 's/sent_seconds_ago=[0-9]+.[0-9]+/sent_seconds_ago=/g' | sed  -r 's/time_until_resend=[0-9]+.[0-9]+/time_until_resend=/g' | ${cdir}/printable_string encode --exempt 92,10,13 -z > $ofile
else
    curl "${curl_https}" -s -i "${full_url}" | grep --text -v '^HTTP/' | grep --text -v '^Connection: keep-alive' | grep --text -v '^transfer-encoding: chunked' | grep --text -v -i '^Date: ' | grep --text -v -i '^Server: ' | sed  -r 's/exec_time=[0-9]+/exec_time=/g' | sed  -r 's/sent_seconds_ago=[0-9]+.[0-9]+/sent_seconds_ago=/g' | sed  -r 's/time_until_resend=[0-9]+.[0-9]+/time_until_resend=/g' | ${cdir}/printable_string encode --exempt 92,10,13 -z > $ofile
fi
exit 0


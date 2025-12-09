#!/bin/env bash


usage() {
    echo "USAGE:"
    echo "$0  [-h|--help]  [--https] [--memcheck] [--max-sessions N] --server host:port"
    exit 0
}

(( $# == 0 )) && usage



outdir="`pwd`/stressoutput"
aggregate="`pwd`/aggregate.py"
server="tonka1:2180"
https="0"
memcheck="0"
max_sessions="200"

H2LOAD_CNT="100"
H2LOAD_CNT_ADMIN="50"
H2LOAD_CNT_NON_CASS="50"
REQ_CNT="50000"
REQ_CNT_ADMIN="1000"
REQ_CNT_NON_CASS="100"


while (( $# )); do
    case $1 in
        --server)
            (( $# > 1 )) || usage
            shift
            server=$1
            ;;
        --max-sessions)
            (( $# > 1 )) || usage
            shift
            max_sessions=$1
            ;;
        --https)
            (( $# > 1 )) || usage
            https="1"
            ;;
        --memcheck)
            (( $# > 1 )) || usage
            memcheck="1"
            ;;
        --help)
            usage
            ;;
        -h|*) # Help
            usage
            ;;
    esac
    shift
done

url="http://${server}"
if [[ "${https}" == "1" ]]; then
    url="https://${server}"
fi

if [[ "${memcheck}" == "1" ]]; then
    H2LOAD_CNT="10"
    H2LOAD_CNT_ADMIN="5"
    H2LOAD_CNT_NON_CASS="5"
    REQ_CNT="500"
    REQ_CNT_ADMIN="50"
    REQ_CNT_NON_CASS="10"
fi

# Waits for h2load to finish; exit if failed;
# aggregate all output; remove output files
finilize() {
    while [ 0 != `ps -ef | grep ${USER} | grep h2load | grep -v grep | wc -l` ]; do sleep 1; done
    ${aggregate} ${outdir} aggr.${1}.json || exit 1
    rm -rf ${outdir}/*.out
}


# Runs a particular case
# case_run "name" "url"
case_run() {
    echo "${1}..."
    for i in `seq 1 ${H2LOAD_CNT}`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -T 6000000 -N 6000000 -n ${REQ_CNT} -c 4 -t 4 -m ${max_sessions} -v  "${url}/${2}" > ${outdir}/h2load.${i}.out &); done
    finilize "${1}"
}

# Runs a particular admin case
# case_run "name" "url"
admin_case_run() {
    echo "${1}..."
    for i in `seq 1 ${H2LOAD_CNT_ADMIN}`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -T 6000000 -N 6000000 -n ${REQ_CNT_ADMIN} -c 4 -t 4 -m ${max_sessions} -v  "${url}/${2}" > ${outdir}/h2load.${i}.out &); done
    finilize "${1}"
}


non_cass_case_run() {
    echo "${1}..."
    for i in `seq 1 ${H2LOAD_CNT_NON_CASS}`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -T 6000000 -N 6000000 -n ${REQ_CNT_NON_CASS} -c 4 -t 4 -m ${max_sessions} -v  "${url}/${2}" > ${outdir}/h2load.${i}.out &); done
    finilize "${1}"
}

mkdir -p ${outdir}
rm -rf ${outdir}/*


case_run "resolve-CBRG44L02" "ID/resolve?psg_protocol=yes&seq_id=CBRG44L02&fmt=json&all_info=yes"
case_run "resolve-XP_015453951" "ID/resolve?psg_protocol=yes&seq_id=XP_015453951&fmt=json&all_info=yes"
case_run "resolve-SPUUN_WGA87311_2" "ID/resolve?psg_protocol=yes&seq_id=SPUUN_WGA87311_2%7C&fmt=json&all_info=yes"
case_run "resolve-817747779-protobuf" "ID/resolve?psg_protocol=yes&seq_id=817747779&seq_id_type=12&fmt=protobuf&all_info=yes"
case_run "resolve-nonexisting" "ID/resolve?psg_protocol=yes&seq_id=nonexisting"
case_run "resolve-RCU83448" "ID/resolve?psg_protocol=yes&seq_id=RCU83448&seq_id_type=12"
case_run "resolve-817747779-json" "ID/resolve?psg_protocol=no&seq_id=817747779&seq_id_type=12&fmt=json&all_info=yes"
case_run "get_na-NW_017890465-1" "ID/get_na?fmt=json&all_info=yes&seq_id=NW_017890465&psg_protocol=yes&names=NA000122202.1,invalid"
case_run "get_na-NW_017890465-2" "ID/get_na?fmt=json&all_info=yes&seq_id=NW_017890465&psg_protocol=yes&names=NA000122.1"
case_run "get-3150015" "ID/get?seq_id=3150015&seq_id_type=12&tse=orig"
case_run "get-bbm-164040" "ID/get?seq_id=bbm%7C164040"
case_run "get-123791" "ID/get?seq_id=123791&seq_id_type=2"
case_run "get-164040" "ID/get?seq_id=164040&seq_id_type=3&exclude_blobs=1.1,4.8091,3.3"
case_run "getblob-4-15971" "ID/getblob?blob_id=4.15971"
case_run "getblob-4-509567" "ID/getblob?blob_id=4.509567&tse=whole"
case_run "getblob-4-1" "ID/getblob?blob_id=4.1"
case_run "get_tse_chunk-25-116773935-5" "ID/get_tse_chunk?id2_chunk=3&id2_info=25.116773935.5"
case_run "get_tse_chunk-25-116773935-5-too-big" "ID/get_tse_chunk?id2_chunk=96&id2_info=25.116773935.5"
case_run "get_tse_chunk-999999999" "ID/get_tse_chunk?id2_chunk=999999999&id2_info=25.116773935.5"
case_run "comment-basic-suppressed" "ID/getblob?blob_id=4.94088756"
case_run "get_na_smart_send_if_small_50000" "ID/get_na?fmt=json&all_info=yes&seq_id=NW_019824422&names=NA000150051.1&tse=smart&send_blob_if_small=50000"
case_run "get_na_slim_send_if_small_50000" "ID/get_na?fmt=json&all_info=yes&seq_id=NW_024096525&names=NA000288180.1&tse=slim&send_blob_if_small=50000"
case_run "ipg_resolve_both" "IPG/resolve?ipg=642300&protein=EGB0689184.1"

non_cass_case_run "wgs_resolve_EAB2056000" "ID/resolve?fmt=json&all_info=yes&seq_id=EAB2056000&disable_processor=osg"
non_cass_case_run "cdd_getna_2222_3_na" "ID/get_na?fmt=json&all_info=yes&seq_id=2222&names=FOOBAR,SNP,CDD&disable_processor=osg"
non_cass_case_run "snp_getna_NG_011231" "ID/get_na?fmt=json&all_info=yes&seq_id=NG_011231.1&names=SNP&disable_processor=osg"
non_cass_case_run "wgs_get_EAB1000000" "ID/get?seq_id=EAB1000000&disable_processor=osg"

admin_case_run "status" "ADMIN/status"
admin_case_run "config" "ADMIN/config"
admin_case_run "info" "ADMIN/info"
admin_case_run "get_alerts" "ADMIN/get_alerts"
admin_case_run "not-served-url" "notservedurl"


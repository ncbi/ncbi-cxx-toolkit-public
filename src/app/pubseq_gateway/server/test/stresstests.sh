#!/bin/env bash


usage() {
    echo "USAGE:"
    echo "$0  [-h|--help]  [--https] --server host:port"
    exit 0
}

(( $# == 0 )) && usage



outdir="`pwd`/stressoutput"
aggregate="`pwd`/aggregate.py"
server="tonka1:2180"
https="0"

while (( $# )); do
    case $1 in
        --server)
            (( $# > 1 )) || usage
            shift
            server=$1
            ;;
        --https)
            (( $# > 1 )) || usage
            https="1"
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


# Waits for h2load to finish; exit if failed;
# aggregate all output; remove output files
finilize() {
    while [ 0 != `ps -ef | grep ${USER} | grep h2load | grep -v grep | wc -l` ]; do sleep 1; done
    ${aggregate} ${outdir} aggr.${1}.json || exit 1
    rm -rf ${outdir}/*.out
}

mkdir -p ${outdir}
rm -rf ${outdir}/*

echo "resolve CBRG44L02..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/resolve?psg_protocol=yes&seq_id=CBRG44L02&fmt=json&all_info=yes" > ${outdir}/h2load.${i}.out &); done
finilize "resolve-CBRG44L02"

echo "resolve XP_015453951..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/resolve?psg_protocol=yes&seq_id=XP_015453951&fmt=json&all_info=yes" > ${outdir}/h2load.${i}.out &); done
finilize "resolve-XP_015453951"

echo "resolve SPUUN_WGA87311_2|..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 100000 -c 4 -t 4 -m 4  "${url}/ID/resolve?psg_protocol=yes&seq_id=SPUUN_WGA87311_2%7C&fmt=json&all_info=yes" > ${outdir}/h2load.${i}.out &); done
finilize "resolve-SPUUN_WGA87311_2"

echo "resolve 817747779-protobuf..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/resolve?psg_protocol=yes&seq_id=817747779&seq_id_type=12&fmt=protobuf&all_info=yes" > ${outdir}/h2load.${i}.out &); done
finilize "resolve-817747779-protobuf"

echo "resolve nonexisting..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/resolve?psg_protocol=yes&seq_id=nonexisting" > ${outdir}/h2load.${i}.out &); done
finilize "resolve-nonexisting"

echo "resolve RCU83448..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/resolve?psg_protocol=yes&seq_id=RCU83448&seq_id_type=12" > ${outdir}/h2load.${i}.out &); done
finilize "resolve-RCU83448"

echo "resolve 817747779-json..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/resolve?psg_protocol=no&seq_id=817747779&seq_id_type=12&fmt=json&all_info=yes" > ${outdir}/h2load.${i}.out &); done
finilize "resolve-817747779-json"

echo "get_na NW_017890465 NA000122202.1,invalid..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 100000 -c 4 -t 4 -m 4  "${url}/ID/get_na?fmt=json&all_info=yes&seq_id=NW_017890465&psg_protocol=yes&names=NA000122202.1,invalid" > ${outdir}/h2load.${i}.out &); done
finilize "get_na-NW_017890465-1"

echo "get_na NW_017890465 NA0001222.1..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get_na?fmt=json&all_info=yes&seq_id=NW_017890465&psg_protocol=yes&names=NA000122.1" > ${outdir}/h2load.${i}.out &); done
finilize "get_na-NW_017890465-2"

echo "get 3150015..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get?seq_id=3150015&seq_id_type=12&tse=orig" > ${outdir}/h2load.${i}.out &); done
finilize "get-3150015"

echo "get bbm|164040..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 100000 -c 4 -t 4 -m 4  "${url}/ID/get?seq_id=bbm%7C164040" > ${outdir}/h2load.${i}.out &); done
finilize "get-bbm-164040"

echo "get 123791..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 1000 -c 4 -t 4 -m 4  "${url}/ID/get?seq_id=123791&seq_id_type=2" > ${outdir}/h2load.${i}.out &); done
finilize "get-123791"

echo "get 164040..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get?seq_id=164040&seq_id_type=3&exclude_blobs=1.1,4.8091,3.3" > ${outdir}/h2load.${i}.out &); done
finilize "get-164040"

echo "getblob 4.15971..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 1000 -c 4 -t 4 -m 4  "${url}/ID/getblob?blob_id=4.15971" > ${outdir}/h2load.${i}.out &); done
finilize "getblob-4-15971"

echo "getblob 4.509567..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/getblob?blob_id=4.509567&tse=whole" > ${outdir}/h2load.${i}.out &); done
finilize "getblob-4-509567"

echo "getblob 4.1..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/getblob?blob_id=4.1" > ${outdir}/h2load.${i}.out &); done
finilize "getblob-4-1"

echo "get_tse_chunk 25-116773935-5..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get_tse_chunk?id2_chunk=3&id2_info=25.116773935.5" > ${outdir}/h2load.${i}.out &); done
finilize "get_tse_chunk-25-116773935-5"

echo "get_tse_chunk 25-116773935-5-too-big..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get_tse_chunk?id2_chunk=96&id2_info=25.116773935.5" > ${outdir}/h2load.${i}.out &); done
finilize "get_tse_chunk-25-116773935-5-too-big"

echo "get_tse_chunk 999999999..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get_tse_chunk?id2_chunk=999999999&id2_info=25.116773935.5" > ${outdir}/h2load.${i}.out &); done
finilize "get_tse_chunk-999999999"

echo "comment-basic-suppressed..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/getblob?blob_id=4.94088756" > ${outdir}/h2load.${i}.out &); done
finilize "comment-basic-suppressed"

echo "get_na_smart_send_if_small_50000..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get_na?fmt=json&all_info=yes&seq_id=NW_019824422&names=NA000150051.1&tse=smart&send_blob_if_small=50000" > ${outdir}/h2load.${i}.out &); done
finilize "get_na_smart_send_if_small_50000"

echo "get_na_slim_send_if_small_50000..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get_na?fmt=json&all_info=yes&seq_id=NW_024096525&names=NA000288180.1&tse=slim&send_blob_if_small=50000" > ${outdir}/h2load.${i}.out &); done
finilize "get_na_slim_send_if_small_50000"

echo "status..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ADMIN/status" > ${outdir}/h2load.${i}.out &); done
finilize "status"

echo "config..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ADMIN/config" > ${outdir}/h2load.${i}.out &); done
finilize "config"

echo "info..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ADMIN/info" > ${outdir}/h2load.${i}.out &); done
finilize "info"

echo "get_alerts..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ADMIN/get_alerts" > ${outdir}/h2load.${i}.out &); done
finilize "get_alerts"

echo "notservingurl..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/notservingurl" > ${outdir}/h2load.${i}.out &); done
finilize "notservingurl"


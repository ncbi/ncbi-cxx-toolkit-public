#!/bin/env bash

usage() {
    echo "USAGE:"
    echo "$0  [-h|--help]  [--https] --server1 host:port --server2 host:port [--h2load-count count]"
    echo "Example: $0 --https --server1 localhost:2180 --server2 localhost:2181"
    exit 0
}

(( $# == 0 )) && usage

server1=""
server2=""
https="0"
h2loadcount="10"

while (( $# )); do
    case $1 in
        --server1)
            (( $# > 1 )) || usage
            shift
            server1=$1
            ;;
        --server2)
            (( $# > 1 )) || usage
            shift
            server2=$1
            ;;
        --h2load-count)
            (( $# > 1 )) || usage
            shift
            h2loadcount=$1
            ;;
        --https)
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

if [[ "${server1}0" == "0" ]]; then
    usage
    exit 1
fi
if [[ "${server2}0" == "0" ]]; then
    usage
    exit 1
fi

url1="http://${server1}"
if [[ "${https}" == "1" ]]; then
    url1="https://${server1}"
fi
url2="http://${server2}"
if [[ "${https}" == "1" ]]; then
    url2="https://${server2}"
fi
outdir="`pwd`/perf"
aggregate="`pwd`/aggregate.py"

# Waits for h2load to finish; exit if failed;
# aggregate all output; remove output files
finilize() {
    while [ 0 != `ps -ef | grep ${USER} | grep h2load | grep -v h2load-count | grep -v h2loadcount | grep -v grep | wc -l` ]; do sleep 1; done
    ${aggregate} ${outdir} aggr.${1}.json || exit 1
    rm -rf ${outdir}/*.out
}


run() {
    local casename="$1"
    local path="$2"

    echo "${casename} for ${server1} ..."
    for i in `seq 1 ${h2loadcount}`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 1000 -c 4 -t 4 -m 4  "${url1}${path}" > ${outdir}/h2load.${i}.out &); done
    finilize "${casename}.old"

    echo "${casename} for ${server2} ..."
    for i in `seq 1 ${h2loadcount}`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 1000 -c 4 -t 4 -m 4  "${url2}${path}" > ${outdir}/h2load.${i}.out &); done
    finilize "${casename}.new"
}



mkdir -p ${outdir}
rm -rf ${outdir}/*.out

run "resolve-primary-cache" "/ID/resolve?psg_protocol=yes&seq_id=XP_015453951&fmt=json&all_info=yes&use_cache=yes"
run "resolve-primary-db" "/ID/resolve?psg_protocol=yes&seq_id=XP_015453951&fmt=json&all_info=yes&use_cache=no"
run "resolve-secondary-cache" "/ID/resolve?psg_protocol=yes&seq_id=123791&fmt=json&all_info=yes&use_cache=yes"
run "resolve-secondary-db" "/ID/resolve?psg_protocol=yes&seq_id=123791&fmt=json&all_info=yes&use_cache=no"
run "resolve-not-found-cache" "/ID/resolve?psg_protocol=yes&seq_id=CBRG44L01&fmt=json&all_info=yes&use_cache=yes"
run "resolve-not-found-db" "/ID/resolve?psg_protocol=yes&seq_id=CBRG44L01&fmt=json&all_info=yes&use_cache=no"
run "get_na-found-cache" "/ID/get_na?fmt=json&all_info=yes&seq_id=NW_017890465&psg_protocol=yes&names=NA000122202.1&use_cache=yes"
run "get_na-found-db" "/ID/get_na?fmt=json&all_info=yes&seq_id=NW_017890465&psg_protocol=yes&names=NA000122202.1&use_cache=no"
run "get_na-not-found-cache" "/ID/get_na?fmt=json&all_info=yes&seq_id=NW_017890465&psg_protocol=yes&names=NA000122202.55&use_cache=yes"
run "get_na-not-found-db" "/ID/get_na?fmt=json&all_info=yes&seq_id=NW_017890465&psg_protocol=yes&names=NA000122202.55&use_cache=no"
run "getblob-not-found-cache" "/ID/getblob?blob_id=4.1&use_cache=yes"
run "getblob-not-found-db" "/ID/getblob?blob_id=4.1&use_cache=no"
run "getblob-small-cache" "/ID/getblob?blob_id=4.268180274&use_cache=yes"
run "getblob-small-db" "/ID/getblob?blob_id=4.268180274&use_cache=no"
run "getblob-large-cache" "/ID/getblob?blob_id=4.509567&use_cache=yes"
run "getblob-large-db" "/ID/getblob?blob_id=4.509567&use_cache=no"
run "getblob-medium-cache" "/ID/getblob?blob_id=4.212993637&use_cache=yes"
run "getblob-medium-db" "/ID/getblob?blob_id=4.212993637&use_cache=no"
run "get-small-cache" "/ID/get?seq_id=EC097430&seq_id_type=5&use_cache=yes"
run "get-small-db" "/ID/get?seq_id=EC097430&seq_id_type=5&use_cache=no"
run "get-large-cache" "/ID/get?seq_id=3150015&use_cache=yes"
run "get-large-db" "/ID/get?seq_id=3150015&use_cache=no"
run "get-medium-cache" "/ID/get?seq_id=AGO14358.1&use_cache=yes"
run "get-medium-db" "/ID/get?seq_id=AGO14358.1&use_cache=no"


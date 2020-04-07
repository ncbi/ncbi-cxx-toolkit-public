if [ $# -ne 1 ]; then
    echo "Usage: performance.sh <run suffix>"
    exit 1
fi

suffix=$1
outdir="`pwd`/perf"
aggregate="`pwd`/aggregate.py"
url="http://localhost:2180"

# Waits for h2load to finish; exit if failed;
# aggregate all output; remove output files
finilize() {
    while [ 0 != `ps -ef | grep ${USER} | grep h2load | grep -v grep | wc -l` ]; do sleep 1; done
    ${aggregate} ${outdir} aggr.${1}.json || exit 1
    rm -rf ${outdir}/*.out
}

mkdir -p ${outdir}
rm -rf ${outdir}/*.out

echo "resolve primary in cache..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/resolve?psg_protocol=yes&seq_id=XP_015453951&fmt=json&all_info=yes&use_cache=yes" > ${outdir}/h2load.${i}.out &); done
finilize "resolve-primary-cache.${suffix}"

echo "resolve primary in db..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/resolve?psg_protocol=yes&seq_id=XP_015453951&fmt=json&all_info=yes&use_cache=no" > ${outdir}/h2load.${i}.out &); done
finilize "resolve-primary-db.${suffix}"

echo "resolve secondary in cache..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/resolve?psg_protocol=yes&seq_id=123791&fmt=json&all_info=yes&use_cache=yes" > ${outdir}/h2load.${i}.out &); done
finilize "resolve-secondary-cache.${suffix}"

echo "resolve secondary in db..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/resolve?psg_protocol=yes&seq_id=123791&fmt=json&all_info=yes&use_cache=no" > ${outdir}/h2load.${i}.out &); done
finilize "resolve-secondary-db.${suffix}"

echo "resolve not found in cache..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/resolve?psg_protocol=yes&seq_id=CBRG44L01&fmt=json&all_info=yes&use_cache=yes" > ${outdir}/h2load.${i}.out &); done
finilize "resolve-not-found-cache.${suffix}"

echo "resolve not found in db..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/resolve?psg_protocol=yes&seq_id=CBRG44L01&fmt=json&all_info=yes&use_cache=no" > ${outdir}/h2load.${i}.out &); done
finilize "resolve-not-found-db.${suffix}"

echo "get_na found in cache..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get_na?fmt=json&all_info=yes&seq_id=NW_017890465&psg_protocol=yes&names=NA000122202.1&use_cache=yes" > ${outdir}/h2load.${i}.out &); done
finilize "get_na-found-cache.${suffix}"

echo "get_na found in db..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get_na?fmt=json&all_info=yes&seq_id=NW_017890465&psg_protocol=yes&names=NA000122202.1&use_cache=no" > ${outdir}/h2load.${i}.out &); done
finilize "get_na-found-db.${suffix}"

echo "get_na not found in cache..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get_na?fmt=json&all_info=yes&seq_id=NW_017890465&psg_protocol=yes&names=NA000122202.55&use_cache=yes" > ${outdir}/h2load.${i}.out &); done
finilize "get_na-not-found-cache.${suffix}"

echo "get_na not found in db..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get_na?fmt=json&all_info=yes&seq_id=NW_017890465&psg_protocol=yes&names=NA000122202.55&use_cache=no" > ${outdir}/h2load.${i}.out &); done
finilize "get_na-not-found-db.${suffix}"

echo "getblob not found in cache..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/getblob?blob_id=4.1&use_cache=yes" > ${outdir}/h2load.${i}.out &); done
finilize "getblob-not-found-cache.${suffix}"

echo "getblob not found in db..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/getblob?blob_id=4.1&use_cache=no" > ${outdir}/h2load.${i}.out &); done
finilize "getblob-not-found-db.${suffix}"

echo "getblob small in cache..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/getblob?blob_id=4.268180274&use_cache=yes" > ${outdir}/h2load.${i}.out &); done
finilize "getblob-small-cache.${suffix}"

echo "getblob small in db..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/getblob?blob_id=4.268180274&use_cache=no" > ${outdir}/h2load.${i}.out &); done
finilize "getblob-small-db.${suffix}"

echo "getblob large in cache..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 2000 -c 4 -t 4 -m 4  "${url}/ID/getblob?blob_id=4.509567&use_cache=yes" > ${outdir}/h2load.${i}.out &); done
finilize "getblob-large-cache.${suffix}"

echo "getblob large in db..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 2000 -c 4 -t 4 -m 4  "${url}/ID/getblob?blob_id=4.509567&use_cache=no" > ${outdir}/h2load.${i}.out &); done
finilize "getblob-large-db.${suffix}"

echo "getblob medium in cache..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 3300 -c 4 -t 4 -m 4  "${url}/ID/getblob?blob_id=4.212993637&use_cache=yes" > ${outdir}/h2load.${i}.out &); done
finilize "getblob-medium-cache.${suffix}"

echo "getblob medium in db..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 3300 -c 4 -t 4 -m 4  "${url}/ID/getblob?blob_id=4.212993637&use_cache=no" > ${outdir}/h2load.${i}.out &); done
finilize "getblob-medium-db.${suffix}"

echo "get small in cache..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get?seq_id=EC097430&seq_id_type=5&use_cache=yes" > ${outdir}/h2load.${i}.out &); done
finilize "get-small-cache.${suffix}"

echo "get small in db..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 10000 -c 4 -t 4 -m 4  "${url}/ID/get?seq_id=EC097430&seq_id_type=5&use_cache=no" > ${outdir}/h2load.${i}.out &); done
finilize "get-small-db.${suffix}"

echo "get large in cache..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 2000 -c 4 -t 4 -m 4  "${url}/ID/get?seq_id=3150015&use_cache=yes" > ${outdir}/h2load.${i}.out &); done
finilize "get-large-cache.${suffix}"

echo "get large in db..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 2000 -c 4 -t 4 -m 4  "${url}/ID/get?seq_id=3150015&use_cache=no" > ${outdir}/h2load.${i}.out &); done
finilize "get-large-db.${suffix}"

echo "get medium in cache..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 3300 -c 4 -t 4 -m 4  "${url}/ID/get?seq_id=AGO14358.1&use_cache=yes" > ${outdir}/h2load.${i}.out &); done
finilize "get-medium-cache.${suffix}"

echo "get medium in db..."
for i in `seq 1 100`; do (LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./ ./h2load -n 3300 -c 4 -t 4 -m 4  "${url}/ID/get?seq_id=AGO14358.1&use_cache=no" > ${outdir}/h2load.${i}.out &); done
finilize "get-medium-db.${suffix}"


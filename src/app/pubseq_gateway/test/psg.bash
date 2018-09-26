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

# full_url="http://nctest21:2180/${url}"
full_url="http://iebdev12:2280/${url}"
if [[ $url == ADMIN* ]]; then
    curl -I --HEAD -s -i "${full_url}" | grep -v '^Date: ' | grep -v '^Content-Length: ' | ${cdir}/printable_string -z > $ofile
    exit 0
fi

if [[ $obasename == "many_chunk_blob" ]]; then
    # The chunks may come in an arbitrary order so diff may fail
    curl -s -i "${full_url}" | grep --text '^PSG-Reply-Chunk: ' | sort > $ofile
    exit 0
fi

if [[ $obasename == "get_splitted_3150015__12" ]]; then
    # The chunks may come in arbitrary order so diff may fail
    curl -s -i "${full_url}" | grep --text '^PSG-Reply-Chunk: ' | sed  -r 's/item_id=[0-9]+&//g' | sort > $ofile
    exit 0
fi

# The most common case
curl -s -i "${full_url}" | grep --text -v '^Date: ' | ${cdir}/printable_string -z > $ofile
exit 0

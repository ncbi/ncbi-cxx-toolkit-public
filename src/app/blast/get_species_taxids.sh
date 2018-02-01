#!/bin/bash 

usage() { 
    echo "Usage:"; 
    echo -e "-t Input Tax Id\n\tGet tax ids at or below input tax id level"; 
    echo -e "-n Scientic Name, Common Name or Keyword\n\tGet tax info for organism";
    echo -e "-o Output Filename"; 
    exit 1; 
}

while getopts  "ht::n::o::" OPT; do
    case $OPT in
	h)
            usage
            ;;
	t)
            TAXID=${OPTARG}
            ;;
        n)
            NAME=${OPTARG}
            ;;
        o)
            OUTFILE=${OPTARG}
            ;;
    esac
done

if [ -z "${TAXID}" ] && [ -z "${NAME}" ]; then
    usage
fi


tmp=${TMPDIR:-/tmp}/taxid.$$
 
if  [ ! -z "${TAXID}" ]; then
    esearch -db taxonomy -query "txid$TAXID[orgn] AND \"at or below species level\"[prop]" > $tmp.1  
    if [ $? -ne 0 ]; then
        echo "esearch error";
        exit 1;
    fi
    cat $tmp.1 | efetch -format uid > $tmp.2
    if [ $? -ne 0 ]; then
        echo "efetch error";
        exit 1;
    fi
    sort -n $tmp.2 > $tmp.1
fi

if [ ! -z "${NAME}" ]; then
    esearch -db taxonomy -query "$NAME[All Names]" > $tmp.1  
    if [ $? -ne 0 ]; then
        echo "esearch error";
        exit 1;
    fi
    cat $tmp.1 | esummary -db taconomy -mode json > $tmp.2

    if [ $? -ne 0 ]; then
        echo "esummary error";
        exit 1;
    fi

    cat $tmp.2 | grep 'uid\|rank\|division\|scientificname\|commonname\|genbankdivision' | grep -v uids | tr -d '"\|,' | tr -s ' ' | sed 's/uid/\nTaxid/g' > $tmp.1 

fi

if [ ! -z "${OUTFILE}" ]; then
    cat $tmp.1 > $OUTFILE
else
    cat $tmp.1 
fi


rm -f $tmp.[12]





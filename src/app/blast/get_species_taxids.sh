#!/bin/bash
# $Id$
# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================
#
# Author:  Amelia Fong
#
# File Description:
#   Script to convert NCBI taxonomy IDs or text into taxonomy IDs suitable for
#   filtering BLAST searches.
#
# N.B.: Depends on EDirect (https://www.ncbi.nlm.nih.gov/books/NBK179288/)
#
# ===========================================================================

export PATH=/bin:/usr/bin:/am/ncbiapdata/bin:$HOME/edirect

TMP1=`mktemp -t $(basename -s .sh $0)-XXXXXXX`
TMP2=`mktemp -t $(basename -s .sh $0)-XXXXXXX`
trap " /bin/rm -fr $TMP1 $TMP2" INT QUIT EXIT HUP KILL ALRM

usage() { 
    echo "Usage:"; 
    echo -e "-t Input Tax ID\n\tGet taxonomy IDs at or below input taxonomy ID level"; 
    echo -e "-n Scientific Name, Common Name or Keyword\n\tGet taxonomy information for organism";
    echo -e "-o Output file name (default stdout)"; 
    exit 1; 
}

error_exit() {
    echo $1;
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
if [ ! -z "${TAXID}" ] && [ ! -z "${NAME}" ]; then
    echo -e "Input Error: -t is incompatible with -n\n"
    usage
fi

if  [ ! -z "${TAXID}" ]; then
    esearch -db taxonomy -query "txid$TAXID[orgn] AND \"at or below species level\"[prop]" > $TMP1 
    if [ $? -ne 0 ]; then
        error_exit  "esearch error"
    fi

    efetch -format uid < $TMP1 > $TMP2 
    if [ $? -ne 0 ]; then
        error_exit "efetch error"
    fi

    if [ ! -s $TMP2 ]; then
        error_exit "Tax Id not found"
    fi

    sort -n $TMP2 > $TMP1
fi

if [ ! -z "${NAME}" ]; then

    esearch -db taxonomy -query "$NAME[All Names]" > $TMP1 
    if [ $? -ne 0 ]; then
        error_exit "esearch error"
    fi

    NUM_RESULTS=$(grep "<Count>" $TMP1 | sed -e 's,.*<Count>\([^<]*\)</Count>.*,\1,g')

    if [ $NUM_RESULTS -eq 0 ]; then
        CORRECT_NAME=$(espell -db taxonomy -query "$NAME" | grep "<CorrectedQuery>" | sed -e 's,.*<CorrectedQuery>\([^<]*\)</CorrectedQuery>.*,\1,g')

        if [ ! -z "${CORRECT_NAME}" ]; then
            echo "Do you mean? \"$CORRECT_NAME\"";
        fi

        esearch -db taxonomy -query "$NAME[Name Tokens]" > $TMP1 
        if [ $? -ne 0 ]; then
            error_exit "esearch error"
        fi
        NUM_RESULTS=$(grep "<Count>" $TMP1 | sed -e 's,.*<Count>\([^<]*\)</Count>.*,\1,g')
        if [ $NUM_RESULTS -gt 500 ]; then
            error_exit "More than 500 matches, please refine your search"
        fi
        if [ $NUM_RESULTS -eq 0 ]; then
            error_exit "No matches for \"$NAME\""
        fi
    fi

    esummary -db taxonomy -mode json < $TMP1 > $TMP2

    if [ $? -ne 0 ]; then
        error_exit "esummary error"
    fi
        
    grep 'uid\|rank\|division\|scientificname\|commonname' $TMP2 | \
    grep -v "uids\|genbankdivision" | tr -d '"\|,' | tr -s ' ' | \
    sed 's/uid/\nTaxid/g;s/name/ name/g' > $TMP1

    echo -e "\n$NUM_RESULTS matches found\n" >> $TMP1
fi

if [ ! -z "${OUTFILE}" ]; then
    mv $TMP1 $OUTFILE
else
    cat $TMP1
fi


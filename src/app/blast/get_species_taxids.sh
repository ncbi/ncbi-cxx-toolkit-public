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

export PATH=/bin:/usr/bin:/am/ncbiapdata/bin:$HOME/edirect:$PATH
set -uo pipefail

TOO_MANY_MATCHES=500
OUTPUT=`mktemp`
TMP=`mktemp`
trap " /bin/rm -fr $OUTPUT $TMP" INT QUIT EXIT HUP KILL ALRM

usage() { 
    echo "$0 usage:"; 
    echo -e "\t-t <taxonomy ID>\n\t\tGet taxonomy IDs at or below input taxonomy ID level"; 
    echo -e "\t-n <Scientific Name, Common Name or Keyword>\n\t\tGet taxonomy information for organism";
    exit 0; 
}

error_exit() {
    msg=$1
    exit_code=${2:-1}
    >&2 echo $msg;
    exit $exit_code;
}

check_deps() {
    for app in esearch efetch esummary; do 
        command -v $app >/dev/null 2>&1 || error_exit "Cannot find Entrez EDirect $app tool, please see installation in https://www.ncbi.nlm.nih.gov/books/NBK179288/"
    done
}

check_deps

TAXID=""
NAME=""
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
    esearch -db taxonomy -query "txid$TAXID[orgn]" > $OUTPUT 
    if [ $? -ne 0 ]; then
        error_exit  "esearch error" $?
    fi

    efetch -format uid < $OUTPUT > $TMP 
    if [ $? -ne 0 ]; then
        error_exit "efetch error" $?
    fi

    if [ ! -s $TMP ]; then
        error_exit "Taxonomy ID not found"
    fi

    sort -n $TMP > $OUTPUT
fi

if [ ! -z "${NAME}" ]; then

    esearch -db taxonomy -query "$NAME[All Names]" > $OUTPUT 
    if [ $? -ne 0 ]; then
        error_exit "esearch error" $?
    fi

    NUM_RESULTS=$(grep "<Count>" $OUTPUT | sed -e 's,.*<Count>\([^<]*\)</Count>.*,\1,g')

    if [ $NUM_RESULTS -eq 0 ]; then
        CORRECT_NAME=$(espell -db taxonomy -query "$NAME" | grep "<CorrectedQuery>" | sed -e 's,.*<CorrectedQuery>\([^<]*\)</CorrectedQuery>.*,\1,g')

        if [ ! -z "${CORRECT_NAME}" ]; then
            error_exit "No matches found for \"$NAME\". Did you mean \"$CORRECT_NAME\"?"
        fi

        esearch -db taxonomy -query "$NAME[Name Tokens]" > $OUTPUT 
        if [ $? -ne 0 ]; then
            error_exit "esearch error"
        fi
        NUM_RESULTS=$(grep "<Count>" $OUTPUT | sed -e 's,.*<Count>\([^<]*\)</Count>.*,\1,g')
        if [ $NUM_RESULTS -gt $TOO_MANY_MATCHES ]; then
            error_exit "More than $TOO_MANY_MATCHES matches found, please refine your search."
        fi
        if [ $NUM_RESULTS -eq 0 ]; then
            error_exit "No matches for \"$NAME\"."
        fi
    fi

    esummary -mode json < $OUTPUT > $TMP

    if [ $? -ne 0 ]; then
        error_exit "esummary error" $?
    fi
        
    cat $TMP | tr ',|{' '\n' | \
    grep 'uid\|rank\|division\|scientificname\|commonname' | \
    grep -v "uids\|genbankdivision" | tr  '"\|,'  " " | tr -s ' ' | \
    sed 's/ uid/Taxid/g;s/name/ name/g' | awk '/Taxid/{print ""}1' > $OUTPUT

    echo -e "\n$NUM_RESULTS matche(s) found.\n" >> $OUTPUT
fi

cat $OUTPUT

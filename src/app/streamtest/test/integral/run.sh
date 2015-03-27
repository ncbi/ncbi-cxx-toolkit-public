#!/bin/sh

#streamtest -in z:\proj\AGP\wgs.AZMS.scflds.1.pbs -infmt asn -type Bioseq-set -outfmt agp -out wgs.AZMS.scflds.1.agp

function check()
{
    FILE_NAME=$1
    rm -f "test_$FILE_NAME.agp"
    streamtest -test agpwrite -i "$FILE_NAME" -o "test_$FILE_NAME.agp" -wgsload
    diff "test_$FILE_NAME.agp" "$FILE_NAME.agp"
    if [ $? -eq 0 ]; then
        echo "$FILE_NAME passed"
    else
        echo "ERROR: $FILE_NAME FAILED"
    fi
}

check ALWZ03S4175630.pse

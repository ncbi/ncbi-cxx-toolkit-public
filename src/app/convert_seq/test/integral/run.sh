#!/bin/sh

TEST_DATA_DIR="../streamtest/test/integral"

function check()
{
    FILE_NAME=$1
    rm -f "test_$FILE_NAME.agp"
    if convert_seq.exe -in "$TEST_DATA_DIR/$FILE_NAME" -infmt asn -type Seq-entry -out "test_$FILE_NAME.agp" -outfmt agp -wgsload &&
       diff "test_$FILE_NAME.agp" "$TEST_DATA_DIR/$FILE_NAME.agp"
    then
        echo "$FILE_NAME passed"
    else
        echo "ERROR: $FILE_NAME FAILED"
    fi
}

check "ALWZ03S4175630.pse"

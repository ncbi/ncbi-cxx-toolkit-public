#!/bin/bash -x

RELEASE=$1
ARM_PATH="arm"
x86_PATH="x86"
UNIVERSAL_PATH="universal"
RELEASE_TAG="ncbi-blast-${RELEASE}+"

if [ -d "$ARM_PATH" ]; then
    rm -rf $ARM_PATH
fi
if [ -d "$x86_PATH" ]; then
rm -rf $x86_PATH
fi

if [ -d "$UNIVERSAL_PATH" ]; then
rm -rf $UNIVERSAL_PATH
fi

mkdir $ARM_PATH
mkdir $x86_PATH
mkdir $UNIVERSAL_PATH

x86_package="${RELEASE_TAG}-x64-macosx.tar.gz"
cp $RELEASE/$x86_package $x86_PATH/ 
tar -vxf $x86_PATH/$x86_package -C $x86_PATH/

arm_package="${RELEASE_TAG}-aarch64-macosx.tar.gz"
cp $RELEASE/$arm_package $ARM_PATH/ 
tar -vxf $ARM_PATH/$arm_package -C $ARM_PATH/

cp $RELEASE/$arm_package $UNIVERSAL_PATH/ 
tar -vxf $UNIVERSAL_PATH/$arm_package -C $UNIVERSAL_PATH/

blast_programs=("blast_formatter" 
"blast_formatter_vdb"
"blast_vdb_cmd"
"blastdb_aliastool"
"blastdbcheck"
"blastdbcmd"
"blastn"
"blastn_vdb"
"blastp"
"blastx"
"convert2blastmask"
"deltablast"
"dustmasker"
"makeblastdb"
"makembindex"
"makeprofiledb"
"psiblast"
"rpsblast"
"rpstblastn"
"segmasker"
"tblastn"
"tblastn_vdb"
"tblastx"
"windowmasker")

echo "Initialized"
for program in "${blast_programs[@]}"
do
rm $UNIVERSAL_PATH/${RELEASE_TAG}/bin/$program
done

for program in "${blast_programs[@]}"
do
lipo -create $ARM_PATH/${RELEASE_TAG}/bin/$program $x86_PATH/${RELEASE_TAG}/bin/$program -output $UNIVERSAL_PATH/${RELEASE_TAG}/bin/$program
done

cd $UNIVERSAL_PATH
tar czf ${RELEASE_TAG}-universal-macosx.tar.gz $RELEASE_TAG 

rm -rf $RELEASE_TAG
rm $arm_package 


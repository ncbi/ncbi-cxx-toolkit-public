#! /bin/sh
# Script to deploy mkkblastindex (please compile this in 64-bit linux, release mode)

if [ $# -eq 0 ]; then
    echo "Usage: ./deploy.sh \"Description of update\""
    exit 0;
fi

# Set the password here
PW=

if [ "x${PW}" = "x" ]; then
    echo "Please set the PW variable to the password to run this script and"
    echo "then REMOVE IT"
    echo "*****************************************************************"
    echo "******   DON'T CHECK IN THE CODE WITH THE PASSWORD   ************"
    echo "*****************************************************************"
    exit 1;
fi

TARGETS="ckblastindex mkkblastindex"

#  production hosts
HOSTS="blastintprod11"

##  test hosts
HOSTS="blastdev3"  

for target in $TARGETS; do
    sdrelease -P $PW -src $target -dst $HOSTS -U support -c "$*"
done

# COLO production hosts
# NOTE: Create an ssh tunnel first!  See 
# http://intranet/cvsutils/index.cgi/internal/blast/interfaces/blast4/test_and_deployment_procedures.txt

# HOSTS="blast1000.st-va"
# SERVER=127.0.0.1
# for target in $TARGETS; do
#   sdrelease -P $PW -S $SERVER -src $target -dst $HOSTS -U support -c "$*"
# done

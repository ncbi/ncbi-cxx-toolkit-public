#!/bin/bash

APP_NM=`basename $0`

 #
## Usage
 #
SPEED_TAG="s:"
DELAY_TAG="d:"
RATIO_TAG="r:"
BATCHES_TAG="b:"

usage()
{
	cat >&2 <<EOF

That script will do stress test for cache.

Syntax :

   $APP_NM [ ${SPEED_TAG}## ] [ ${DELAY_TAG}## ] [ ${RATIO_TAG}## ] [ ${BATCHES_TAG}## ] conn_string

Where :

   conn_string - netcache connection string in the form 'server:port'.
                 Required parameter
   ${SPEED_TAG}##        - speed of netcache stress. Integer value between 10 and 400.
                 Optional, default value 100 blobs per second
   ${DELAY_TAG}##        - delay time in minutes for repeating reading of
                 stored blobs. Integer value between 1 and 400.
                 Optional, default value 30
   ${RATIO_TAG}##        - ratio for stored blobs for repeating reading.
                 Integer value greater than 0.
                 Optional, default value 4
   ${BATCHES_TAG}##        - quantity of parralell processes to run.
                 Integer value greater than 0.
                 Optional, default value is (speed / 40)

Example :

   $APP_NM ${SPEED_TAG}80 ${DELAY_TAG}20 ${RATIO_TAG}2 ${BATCHES_TAG}8 pdev1:9002

EOF
}

 #
## Arguments
 #
ARGS_V=($@)
ARGS_V_QTY=${#ARGS_V[*]}

if [ $ARGS_V_QTY -lt 1 ]
then
	echo Missed arguments >&2
	usage
	exit 1
fi

CONNECTION_SPEC=${ARGS_V[$(($ARGS_V_QTY - 1))]}
SPEED_SPEC=100
DELAY_SPEC=30
RATIO_SPEC=4
BATCHES_SPEC=0

CNT=0
while [ $CNT -lt $(($ARGS_V_QTY - 1)) ]
do
	ARG=${ARGS_V[$CNT]}

	case $ARG in
		${SPEED_TAG}*)
			SPEED_SPEC=`echo $ARG | sed "s#$SPEED_TAG##1"`
			;;
		${DELAY_TAG}*)
			DELAY_SPEC=`echo $ARG | sed "s#$DELAY_TAG##1"`
			;;
		${RATIO_TAG}*)
			RATIO_SPEC=`echo $ARG | sed "s#$RATIO_TAG##1"`
			;;
		${BATCHES_TAG}*)
			BATCHES_SPEC=`echo $ARG | sed "s#$BATCHES_TAG##1"`
			;;
		*)
			echo "ERROR : unknown parameter '$ARG'"
			usage
			exit 1
			;;
	esac

	CNT=$(($CNT + 1))
done

if [ $BATCHES_SPEC -ge $SPEED_SPEC ]
then
	echo Batches qty should be less than stress speed
	exit 1
fi

PER_SEC=$SPEED_SPEC

if [ $BATCHES_SPEC -gt 0 ]
then
	STREAM_QTY=$BATCHES_SPEC
	PER_STREAM=$(($PER_SEC / $STREAM_QTY))
	PER_STREAM=$((PER_STREAM + 1))
else
	PER_BA=40
	if [ $PER_SEC -gt $PER_BA ]
	then
		STREAM_QTY=$(($PER_SEC / $PER_BA))
		STREAM_QTY=$(($STREAM_QTY + 1))
		PER_STREAM=$(($PER_SEC / $STREAM_QTY))
	else
		STREAM_QTY=1
		PER_STREAM=$PER_SEC
	fi
fi

LOG_FILE=log.`date "+%Y-%m-%d:%H-%M-%S"`
echo Log file : $LOG_FILE
exec >$LOG_FILE 2>&1

echo Executing test in $PER_SEC blobs per second
echo Delayed reading in $DELAY_SPEC minutes with $RATIO_SPEC ratio
echo Executing in $STREAM_QTY batches by $PER_STREAM blobs per second

run_stres()
{
	CMD="./test_nc_stress_pubmed -c $CONNECTION_SPEC -d $DELAY_SPEC -p $PER_STREAM -r $RATIO_SPEC"
	echo "## $CMD"
	eval $CMD
}

CNT=0
while [ $CNT -lt $STREAM_QTY ]
do
	echo Starting stress $CNT

	run_stres &

	CNT=$(($CNT + 1))
done

wait

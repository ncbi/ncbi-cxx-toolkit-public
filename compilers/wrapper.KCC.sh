#!/bin/sh
# Needed because KCC sometimes hangs :-/

# 10 minutes, in 10-second increments
interval=10
count=60


guard() {
    n=0
    while [ $n -lt $count ]; do
	kill -0 $1 || return
	sleep $interval
	n=`expr $n + 1`
    done
    kill $1
    sleep $interval
    kill -9 $1
}


for tries in 1 2; do
    "$@" &
    kcc_pid=$!
    guard $kcc_pid &
    wait $kcc_pid
    status=$?
    test $status -lt 128 && exit $status
done

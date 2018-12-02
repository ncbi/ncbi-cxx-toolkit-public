#! /bin/sh
# $Id$

# Default to a basic test service.
: ${service:=bouncehttp}

# Test the service using a pseudo-random number of threads (between 2 and 11).
nthreads="`expr $$ % 10 + 2`"

if test -z "$CHECK_EXEC"; then
    run_local="./"
else
    run_local=" "
fi
$CHECK_EXEC${run_local}test_ncbi_service_cxx_mt -threads $nthreads -service $service

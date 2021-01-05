#! /bin/sh
# $Id$

# Disable all Windows configurations.
#   The problem is that devops doesn't yet support linkerd on Windows.
if echo "$FEATURES" | grep -E '(^| )MSWin( |$)' > /dev/null; then
    echo "NCBI_UNITTEST_DISABLED"
    exit 0
fi

# LINKERD_TODO - possible enhancements:
# 1.    Test services other than or in addition to bouncehttp and enable the
#           pseudo-random service selection.  This might depend on #2.
# 2.    Add support for query parameters.

# Create an array of service names to test; pseudo-randomly pick one.
#set \
#    'bouncehttp' \
#    'account' \
#    'demo-project' \
#    'solr-fls_meta' \
#    'FWD' \
#    'Accn2Gi' \
#    'alndbasn_lb' \
#    'aligndb_dbldd' \
#    'TaxService3Test' \
#    'MMDevLinux' \
#    'NC_SV_ObjCache' \
#    'beVarSearch' \
#    'GC_GetAssembly_v3'
#shift `expr $$ '%' $#`
#svc="$1"
svc=cxx-fast-cgi-sample
path="/"

# Test the service using a pseudo-random number of threads (between 2 and 11).
nthreads="`expr $$ % 10 + 2`"

# Pick some arbitrary text for testing.
test_text="message=hi%20there%0A"
expected_regex='^.*?C\+\+ GIT FastCGI Sample.*?<p>Your previous message: +'\''hi there\n'\''.*$'

: ${CHECK_TIMEOUT:=600}
if test -z "$CHECK_EXEC"; then
    run_local="./"
else
    run_local=" "
fi
$CHECK_EXEC${run_local}test_ncbi_linkerd_mt -timeout $CHECK_TIMEOUT -threads $nthreads -service $svc -path $path -post "$test_text" -expected "$expected_regex"

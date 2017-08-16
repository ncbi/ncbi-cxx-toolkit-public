#! /bin/sh
# $Id$

# Disable all Windows configurations.
#   The problem is that devops doesn't yet support linkerd on Windows.
if echo "$FEATURES" | grep -E '(^| )MSWin( |$)' > /dev/null; then
    echo "NCBI_UNITTEST_DISABLED"
    exit 0
fi

# LINKERD_TODO - possible enhancements:
# 1.    Test services other than or in addition to bounce and enable the
#           pseudo-random service selection.  This might depend on #2.
# 2.    Add support for query parameters.

# Create an array of service names to test; pseudo-randomly pick one.
#set \
#    'bounce' \
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
#    'GC_GetAssembly_v3s'
#shift `expr $$ '%' $#`
#svc="$1"
svc=bouncehttp

# Test the service using a pseudo-random number of threads (between 2 and 11).
nthreads="`expr $$ % 10 + 2`"

# Pick some arbitrary text for testing.
test_text="param1%3Dval1%26param2%3D%22line+1%0Aline+2%0A%22"

: ${CHECK_TIMEOUT:=600}
if test -n "$CHECK_EXEC"; then
    $CHECK_EXEC test_ncbi_linkerd_mt -timeout $CHECK_TIMEOUT -threads $nthreads -service $svc -path /Service/bounce.cgi -post "$test_text" -expected "$test_text"
else
    ./test_ncbi_linkerd_mt -timeout $CHECK_TIMEOUT -threads $nthreads -service $svc -path /Service/bounce.cgi -post "$test_text" -expected "$test_text"
fi

#! /bin/sh
#$Id$

# set development PTIS service URL where linkerd is not available

if echo test | nc -w 1 linkerd 4142
then
    NCBI_CONFIG__ID2SNP__PTIS_NAME="pool.linkerd-proxy.service.bethesda-dev.consul.ncbi.nlm.nih.gov:4142"
    export NCBI_CONFIG__ID2SNP__PTIS_NAME
fi

$CHECK_EXEC "$@"

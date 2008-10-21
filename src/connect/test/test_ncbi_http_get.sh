#! /bin/sh
#$Id$

if [ "$1" != "" ]; then
    CONN_USESSL=1; export CONN_USESSL
    url='http://test.gnutls.org:5556'
else
    url='http://www.ncbi.nlm.nih.gov/entrez/viewer.cgi?view=0&maxplex=1&save=idf&val=4959943'
fi
exec test_ncbi_http_get "$url"

#! /bin/sh
# $Id$

trap 'exit $?' ERR

test_ncbi_server_info 'HTTP :1111 /somepath'
test_ncbi_server_info 'HTTP :1111 /somepath A=B'
test_ncbi_server_info 'HTTP :1111 /somepath H=vhost A=B'
test_ncbi_server_info 'HTTP :1111 /somepath H=vhost A=B $=yes'
test_ncbi_server_info 'HTTP :1111 /somepath H=vhost A=B R=1500 $=yes'
test_ncbi_server_info 'STANDALONE :1111 S=no'
test_ncbi_server_info 'STANDALONE :1111 S=yes'
test_ncbi_server_info 'STANDALONE :1111 S=yes $=yes'
test_ncbi_server_info 'STANDALONE :1111 S=yes $=yes H=vhost.blah.do'
test_ncbi_server_info 'STANDALONE :1111 S=yes $=yes H=vhost.blah.do A=B'
test_ncbi_server_info 'STANDALONE :1111 S=yes $=yes H=vhost.blah.do A=R'
test_ncbi_server_info 'DNS :0'
test_ncbi_server_info 'DNS :0 A=B'
test_ncbi_server_info 'DNS :111 A=B R=1500 T=600'

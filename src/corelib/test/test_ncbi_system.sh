#! /bin/sh

./test_ncbi_system cpu; if test $? -ne 255; then exit 1; fi
./test_ncbi_system mem; if test $? -ne 255; then exit 2; fi
exit 0

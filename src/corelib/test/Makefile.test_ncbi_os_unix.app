# $Id$

APP = test_ncbi_os_unix
SRC = test_ncbi_os_unix
LIB = xncbi

REQUIRES = unix -Cygwin

CHECK_CMD  = test_ncbi_os_unix.sh
CHECK_COPY = test_ncbi_os_unix.sh


WATCHERS = lavr

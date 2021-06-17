#$Id$

APP = test_ncbi_system
SRC = test_ncbi_system
LIB = xncbi

CHECK_CMD  = test_ncbi_system.sh
CHECK_COPY = test_ncbi_system.sh

CHECK_REQUIRES = -Valgrind

WATCHERS = ivanov

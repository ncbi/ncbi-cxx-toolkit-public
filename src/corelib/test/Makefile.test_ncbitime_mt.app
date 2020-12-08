# $Id$

APP = test_ncbitime_mt
SRC = test_ncbitime_mt
LIB = test_mt xncbi

CHECK_CMD = test_ncbitime.sh test_ncbitime_mt /CHECK_NAME=test_ncbitime_mt
CHECK_COPY = test_ncbitime.sh

WATCHERS = ivanov

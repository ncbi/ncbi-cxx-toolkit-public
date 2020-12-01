# $Id$

APP = test_ncbitime_mt
SRC = test_ncbitime_mt
LIB = test_mt xncbi


CHECK_CMD = env TZ=America/New_York test_ncbitime_mt

WATCHERS = ivanov

# $Id$

APP = test_ncbidiag_perf
SRC = test_ncbidiag_perf
LIB = test_mt xncbi

REQUIRES = MT

CPPFLAGS = $(ORIG_CPPFLAGS) -DNCBI_SHOW_FUNCTION_NAME

CHECK_CMD = test_ncbidiag_perf

WATCHERS = vasilche

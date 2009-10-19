# $Id$

APP = test_ncbidiag_mt
SRC = test_ncbidiag_mt
LIB = xncbi test_mt

CPPFLAGS = $(ORIG_CPPFLAGS) -DNCBI_SHOW_FUNCTION_NAME

CHECK_CMD = test_ncbidiag_mt -format old
CHECK_CMD = test_ncbidiag_mt -format new

WATCHERS = grichenk

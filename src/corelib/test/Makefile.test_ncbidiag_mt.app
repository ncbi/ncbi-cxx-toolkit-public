# $Id$

APP = test_ncbidiag_mt
SRC = test_ncbidiag_mt
LIB = test_mt xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) -DNCBI_SHOW_FUNCTION_NAME

CHECK_CMD = env DIAG_OLD_POST_FORMAT= test_ncbidiag_mt -format old /CHECK_NAME=test_ncbidiag_mt_old_fmt
CHECK_CMD = env DIAG_OLD_POST_FORMAT= test_ncbidiag_mt -format new /CHECK_NAME=test_ncbidiag_mt_new_fmt

WATCHERS = grichenk

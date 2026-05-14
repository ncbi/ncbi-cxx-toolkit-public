# $Id$

APP = test_ncbidiag_mt
SRC = test_ncbidiag_mt
LIB = test_mt xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) -DNCBI_SHOW_FUNCTION_NAME

CHECK_COPY = test_ncbidiag_mt.sh
CHECK_CMD = test_ncbidiag_mt.sh -format old /CHECK_NAME=test_ncbidiag_mt_old_fmt
CHECK_CMD = test_ncbidiag_mt.sh -format new /CHECK_NAME=test_ncbidiag_mt_new_fmt
CHECK_CMD = test_ncbidiag_mt.sh -format old -async /CHECK_NAME=test_ncbidiag_mt_old_fmt_async
CHECK_CMD = test_ncbidiag_mt.sh -format new -async /CHECK_NAME=test_ncbidiag_mt_new_fmt_async
CHECK_CMD = test_ncbidiag_mt.sh -format old -async -async_discard /CHECK_NAME=test_ncbidiag_mt_old_fmt_async_discard
CHECK_CMD = test_ncbidiag_mt.sh -format new -async -async_discard /CHECK_NAME=test_ncbidiag_mt_new_fmt_async_discard

WATCHERS = grichenk

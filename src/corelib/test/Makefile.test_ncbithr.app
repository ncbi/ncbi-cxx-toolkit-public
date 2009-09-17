# $Id$

APP = test_ncbithr
SRC = test_ncbithr
LIB = xncbi

REQUIRES = MT

CHECK_CMD = test_ncbithr
CHECK_CMD = test_ncbithr -favorwriters

WATCHERS = grichenk

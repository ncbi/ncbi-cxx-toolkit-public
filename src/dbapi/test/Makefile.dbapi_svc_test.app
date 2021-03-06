# $Id$

# Temporarily disable on Windows due to missing devops support.
REQUIRES = -MSWin

APP = dbapi_svc_test
SRC = dbapi_svc_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = sybdb_ftds100 $(DBAPI_CTLIB) $(SDBAPI_LIB) xconnect xutil xncbi
LIBS = $(SDBAPI_LIBS) $(SYBASE_LIBS) $(SYBASE_dllS) $(ORIG_LIBS)

# LINK = purify $(C_LINK)

CHECK_REQUIRES = in-house-resources connext
CHECK_COPY = dbapi_svc_test.bash dbapi_svc_test.ini interfaces
CHECK_CMD = dbapi_svc_test.bash LBSMD  NOSYBASE /CHECK_NAME=dbapi_svc_test_lbsmd_nosyb
CHECK_CMD = dbapi_svc_test.bash LBSMD  SYBASE   /CHECK_NAME=dbapi_svc_test_lbsmd_syb
CHECK_CMD = dbapi_svc_test.bash NAMERD NOSYBASE /CHECK_NAME=dbapi_svc_test_namerd_nosyb
CHECK_CMD = dbapi_svc_test.bash NAMERD SYBASE   /CHECK_NAME=dbapi_svc_test_namerd_syb
CHECK_TIMEOUT = 600

WATCHERS = ucko lavr

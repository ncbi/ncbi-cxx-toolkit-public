# $Id$

REQUIRES = -MSWin

APP = test_ncbi_namerd_mt
SRC = test_ncbi_namerd_mt

LIB = xconnect test_mt xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_ncbi_namerd_mt.sh
CHECK_COPY = test_ncbi_namerd_mt.sh test_ncbi_namerd_mt.ini
CHECK_TIMEOUT = 60

WATCHERS = lavr mcelhany

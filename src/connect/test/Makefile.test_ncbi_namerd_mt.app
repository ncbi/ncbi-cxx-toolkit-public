# $Id$

APP = test_ncbi_namerd_mt
SRC = test_ncbi_namerd_mt

LIB = xconnect test_mt xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = in-house-resources
CHECK_CMD = test_ncbi_namerd_mt.sh /CHECK_NAME=test_ncbi_namerd_mt
CHECK_COPY = test_ncbi_namerd_mt.sh test_ncbi_namerd_mt.ini
CHECK_TIMEOUT = 600

WATCHERS = lavr

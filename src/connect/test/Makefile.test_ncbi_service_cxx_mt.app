# $Id$

APP = test_ncbi_service_cxx_mt
SRC = test_ncbi_service_cxx_mt

LIB = xconnect test_mt xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = in-house-resources
CHECK_CMD = test_ncbi_service_cxx_mt.sh /CHECK_NAME=test_ncbi_service_cxx_mt
CHECK_COPY = test_ncbi_service_cxx_mt.sh test_ncbi_service_cxx_mt.ini
CHECK_TIMEOUT = 60

WATCHERS = lavr

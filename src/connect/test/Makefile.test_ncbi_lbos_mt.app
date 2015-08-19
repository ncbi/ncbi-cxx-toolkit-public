# $Id$

###  BASIC PROJECT SETTINGS
APP = test_ncbi_lbos_mt
SRC = test_ncbi_lbos_mt
# OBJ =

CPPFLAGS =  -DLBOS_TEST_MT $(ORIG_CPPFLAGS)

LIB = test_mt xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

# Comment out if you do not want it to run automatically as part of
# "make check".
CHECK_CMD =
CHECK_COPY = test_ncbi_lbos_mt.ini
CHECK_TIMEOUT = 600

WATCHERS = elisovdn


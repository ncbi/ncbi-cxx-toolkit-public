# $Id$

###  BASIC PROJECT SETTINGS
APP = test_ncbi_lbos
SRC = test_ncbi_lbos
# OBJ = 

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xconnect test_boost xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

# Comment out if you do not want it to run automatically as part of
# "make check".
CHECK_CMD =
CHECK_COPY = test_ncbi_lbos.ini
CHECK_TIMEOUT = 600

WATCHERS = elisovdn

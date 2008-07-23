# $Id$

APP = dbapi_unit_test
SRC = dbapi_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = dbapi dbapi_driver dbapi_util_blobstore$(STATIC) \
       $(COMPRESS_LIBS) $(XCONNEXT) test_boost$(STATIC) xconnect xutil xncbi
PRE_LIBS = $(BOOST_LIBS)
LIBS =  $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test

# CHECK_COPY = dbapi_unit_test.sh dbapi_unit_test.ini
CHECK_COPY = dbapi_unit_test.sh
CHECK_CMD = dbapi_unit_test.sh
CHECK_TIMEOUT = 600

CHECK_AUTHORS = ivanovp

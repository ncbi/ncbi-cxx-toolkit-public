# $Id$

APP = dbapi_unit_test
SRC = dbapi_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(CPPUNIT_INCLUDE)

LIB  = dbapi dbapi_driver xncbi
LIBS = $(CPPUNIT_LIBS) $(ORIG_LIBS)

CHECK_CMD = dbapi_unit_test
CHECK_REQUIRES = DLL

# $Id$

APP = dbapi-unit-test
SRC = dbapi-unit-test

CPPFLAGS = $(ORIG_CPPFLAGS) $(CPPUNIT_INCLUDE)

LIB  = dbapi dbapi_driver xncbi
LIBS = $(CPPUNIT_LIBS) $(ORIG_LIBS)

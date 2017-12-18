# $Id$

APP = gapinfo_unit_test
SRC = gapinfo_unit_test 

CPPFLAGS = -DNCBI_MODULE=BLAST $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
LIB = test_boost $(BLAST_LIBS) xconnect xncbi

LIBS = $(BLAST_THIRD_PARTY_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = MT in-house-resources
CHECK_CMD = gapinfo_unit_test
CHECK_COPY = gapinfo_unit_test.ini

WATCHERS = boratyng madden camacho fongah2

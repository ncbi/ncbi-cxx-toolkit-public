# $Id$

APP = unit_test_polya

SRC = unit_test_polya

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xobjutil xregexp tables test_boost $(OBJMGR_LIBS)
LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

# Uncomment if you do not want it to run automatically as part of
# "make check".
CHECK_CMD = unit_test_polya -data-in polya.seq.asn
CHECK_COPY = polya.seq.asn

WATCHERS = mozese2

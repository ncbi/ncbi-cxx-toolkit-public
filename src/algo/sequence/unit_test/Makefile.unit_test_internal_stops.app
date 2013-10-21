# $Id$

APP = unit_test_internal_stops

SRC = unit_test_internal_stops

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xalgoseq xobjutil xalnmgr tables test_boost $(OBJMGR_LIBS)
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

# Uncomment if you do not want it to run automatically as part of
# "make check".
CHECK_CMD = unit_test_internal_stops
CHECK_COPY =

WATCHERS = mozese2

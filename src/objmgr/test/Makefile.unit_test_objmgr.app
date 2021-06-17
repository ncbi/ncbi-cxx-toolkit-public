#################################
# $Id$
#################################

REQUIRES = Boost.Test.Included

APP = unit_test_objmgr
SRC = unit_test_objmgr
LIB = test_boost $(SOBJMGR_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

CHECK_CMD = unit_test_objmgr

WATCHERS = vasilche

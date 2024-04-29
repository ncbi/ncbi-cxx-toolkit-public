#################################
# $Id$
#################################

APP = test_huge_file_process
SRC = test_huge_file_process

LIB = xhugeasn xobjread xlogging xutil test_boost $(SOBJMGR_LIBS)
LIBS = $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

WATCHERS = gotvyans foleyjp


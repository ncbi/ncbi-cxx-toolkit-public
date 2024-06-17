#################################
# $Id$
#################################

APP = test_feature_table_reader
SRC = test_feature_table_reader

LIB = $(OBJREAD_LIBS) xobjutil test_boost $(SOBJMGR_LIBS)
LIBS = $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_COPY = 5col_feature_table_files
WATCHERS = gotvyans foleyjp

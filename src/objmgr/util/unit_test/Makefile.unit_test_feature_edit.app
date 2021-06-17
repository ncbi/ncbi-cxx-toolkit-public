APP = unit_test_feature_edit
SRC = unit_test_feature_edit

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xobjutil test_boost $(SOBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD = 
CHECK_COPY = feature_edit_test_cases
CHECK_TIMEOUT = 3000

WATCHERS = foleyjp

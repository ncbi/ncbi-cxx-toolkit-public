APP = test_jsonwrapp11
SRC = test_jsonwrapp11

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)
LIB = test_boost xncbi

REQUIRES = Boost.Test.Included

PROJ_TAG = test
CHECK_CMD =

WATCHERS = gouriano

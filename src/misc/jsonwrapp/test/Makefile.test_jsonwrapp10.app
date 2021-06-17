APP = test_jsonwrapp10
SRC = test_jsonwrapp10

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)
LIB = test_boost xncbi

REQUIRES = Boost.Test.Included

PROJ_TAG = test
CHECK_CMD =

WATCHERS = gouriano

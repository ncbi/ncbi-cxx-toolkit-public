APP = test_jsonwrapp
SRC = test_jsonwrapp

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)
LIB = test_boost xncbi

REQUIRES = Boost.Test.Included

PROJ_TAG = test
CHECK_CMD =

WATCHERS = gouriano

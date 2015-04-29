APP = test_base64
SRC = test_base64

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)
LIB = test_boost xncbi

REQUIRES = Boost.Test.Included

CHECK_CMD =

WATCHERS = sadyrovr

APP = test_uncaught_exception
SRC = test_uncaught_exception

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)
LIB = test_boost xncbi

REQUIRES = Boost.Test.Included

CHECK_CMD =

WATCHERS = satskyse

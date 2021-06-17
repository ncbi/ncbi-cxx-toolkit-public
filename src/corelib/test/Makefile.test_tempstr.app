APP = test_tempstr
SRC = test_tempstr

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)
LIB = test_boost xncbi

REQUIRES = Boost.Test.Included

CHECK_CMD =

WATCHERS = vasilche ivanov

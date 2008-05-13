APP = test_tempstr
SRC = test_tempstr

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)
LIB = xncbi
LIBS = $(BOOST_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test

CHECK_CMD =

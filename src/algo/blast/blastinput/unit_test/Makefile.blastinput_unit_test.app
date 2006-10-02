# $Id$

APP = blastinput_unit_test
SRC = blastinput_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = blastinput xobjsimple xobjutil xobjread $(OBJMGR_LIBS)
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(NETWORK_LIBS) \
		$(CMPRS_LIBS) $(ORIG_LIBS)

REQUIRES = objects
# CHECK_CMD = blastinput_unit_test
# CHECK_TIMEOUT = 90


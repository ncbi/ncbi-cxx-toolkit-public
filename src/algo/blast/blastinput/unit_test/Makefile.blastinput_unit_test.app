# $Id$

APP = blastinput_unit_test
SRC = blastinput_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = blastinput xblast composition_adjustment xnetblastcli xnetblast seqdb \
      scoremat blastdb xobjsimple xobjutil xobjread tables $(OBJMGR_LIBS)
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(NETWORK_LIBS) \
		$(CMPRS_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects

CHECK_COPY = data

# $Id$

APP = unit_test_extended_cleanup
SRC = unit_test_extended_cleanup

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)


LIB = xcleanup xunittestutil $(XFORMAT_LIBS) ncbi_xloader_wgs $(SRAREAD_LIBS) xalnmgr xobjutil valid taxon3 gbseq submit xconnect \
      tables xregexp $(PCRE_LIB) test_boost $(OBJMGR_LIBS) $(OBJEDIT_LIBS)

LIBS = $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = Boost.Test.Included $(VDB_REQ)

CHECK_CMD =

WATCHERS = bollin kornbluh kans

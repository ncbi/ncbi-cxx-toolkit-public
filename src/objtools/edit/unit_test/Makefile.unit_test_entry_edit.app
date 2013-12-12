# $Id$

APP = unit_test_entry_edit
SRC = unit_test_entry_edit

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = xobjedit $(XFORMAT_LIBS) xalnmgr xobjutil valid valerr \
       taxon3 gbseq submit tables xregexp $(PCRE_LIB) test_boost $(OBJMGR_LIBS) 
LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =
CHECK_COPY = entry_edit_test_cases
CHECK_TIMEOUT = 3000

WATCHERS = bollin kornbluh

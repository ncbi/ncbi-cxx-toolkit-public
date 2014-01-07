###############################
# $Id$
###############################

APP = test_basic_cleanup
SRC = test_basic_cleanup
LIB = xcleanup xregexp xobjutil valid valerr submit $(OBJMGR_LIBS) $(PCRE_LIB)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)

WATCHERS = bollin kornbluh

###############################
# $Id$
###############################

APP = test_basic_cleanup
SRC = test_basic_cleanup
LIB = xcleanup xobjutil xobjedit valid taxon3 submit xconnect xregexp $(PCRE_LIB) \
      $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = bollin kornbluh kans

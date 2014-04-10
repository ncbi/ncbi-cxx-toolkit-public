###############################
# $Id$
###############################

APP = test_basic_cleanup
SRC = test_basic_cleanup
LIB = xcleanup xobjutil valid submit xregexp $(PCRE_LIB) $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = bollin kornbluh

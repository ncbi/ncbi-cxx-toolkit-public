###############################
# $Id$
###############################

APP = test_basic_cleanup
SRC = test_basic_cleanup
LIB = xcleanup xobjutil submit $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = bollin

#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "streamtest"
#################################

APP = streamtest
SRC = streamtest
LIB = prosplign xalgoalignutil xalnmgr xcleanup xobjutil submit \
      $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


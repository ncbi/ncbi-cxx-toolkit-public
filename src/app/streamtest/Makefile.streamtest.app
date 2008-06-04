#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "streamtest"
#################################

APP = streamtest
SRC = streamtest
LIB = xobjutil submit $(SOBJMGR_LIBS)

LIBS = $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

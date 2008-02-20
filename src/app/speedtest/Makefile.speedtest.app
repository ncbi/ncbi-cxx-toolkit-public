#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "speedtest"
#################################

APP = speedtest
SRC = speedtest
LIB = xcleanup xobjutil submit $(SOBJMGR_LIBS)

LIBS = $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


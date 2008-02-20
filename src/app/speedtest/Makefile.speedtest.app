#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "speedtest"
#################################

APP = speedtest
SRC = speedtest
LIB = xcleanup xobjutil submit $(SOBJMGR_LIBS)

REQUIRES = objects -Cygwin


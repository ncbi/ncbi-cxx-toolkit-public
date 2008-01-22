#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "speedtest"
#################################

APP = speedtest
SRC = speedtest
LIB = xformat xobjutil submit gbseq xalnmgr xcleanup entrez2cli entrez2 tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = dbapi objects -Cygwin


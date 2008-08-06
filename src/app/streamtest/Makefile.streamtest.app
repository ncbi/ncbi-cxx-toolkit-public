#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "streamtest"
#################################

APP = streamtest
SRC = streamtest
LIB = xobjutil xcleanup prosplign submit $(COMPRESS_LIBS) $(SOBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "multireader"
#################################

APP = multireader
SRC = multireader
LIB = xobjutil submit xobjread $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


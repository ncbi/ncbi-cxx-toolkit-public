#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "idmapper"
#################################

APP = idmapper
SRC = idmapper
LIB = xobjreadex xobjread xformat xobjutil submit gbseq xalnmgr xcleanup \
	  entrez2cli entrez2 tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


WATCHERS = ludwigf

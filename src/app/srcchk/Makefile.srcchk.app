#################################
# $Id$
# Author: Frank Ludwig
#################################

# Build application "srcchk"
#################################

APP = srcchk
SRC = srcchk
LIB = xobjwrite xobjread ${OBJREAD_LIBS} xobjutil gbseq xalnmgr entrez2cli entrez2 \
	tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin

WATCHERS = ludwigf

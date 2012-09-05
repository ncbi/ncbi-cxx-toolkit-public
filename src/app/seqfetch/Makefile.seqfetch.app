#################################
# $Id$
# Author:  Mati Shomrat
#################################

# Build application "seqfetch"
#################################

APP = seqfetch
SRC = seqfetch
LIB = xobjwrite xobjread xobjutil gbseq xalnmgr entrez2cli entrez2 tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


WATCHERS = ludwigf

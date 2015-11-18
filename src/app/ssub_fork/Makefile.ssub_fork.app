#################################
# $Id$
# Author:  Justin Foley
#################################

# Build application "seqsub_split"
#################################

APP = seqsub_split
SRC = ssub_fork
LIB = xobjwrite variation_utils $(OBJREAD_LIBS) xalnmgr xobjutil \
      gbseq entrez2cli entrez2 tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


WATCHERS = foleyjp

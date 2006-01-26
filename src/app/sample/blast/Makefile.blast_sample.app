#################################
# $Id$
#################################

APP = blast_sample
SRC = blast_sample

LIB = xblast composition_adjustment xalgodustmask \
		xnetblastcli xnetblast xobjutil xobjsimple \
		scoremat seqdb blastdb tables $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects


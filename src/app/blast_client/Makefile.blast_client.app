# $Id$

APP = blast_client
SRC = blast_client queue_poll search_opts align_parms
LIB = xalnmgr xblastformat xobjutil xobjread xnetblastcli xnetblast \
      scoremat blastdb tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects dbapi

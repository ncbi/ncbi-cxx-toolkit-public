# $Id$

APP = test_remote_updater
SRC = test_remote_updater
LIB = $(OBJEDIT_LIBS) xobjutil xobjmgr genome_collection seqset $(SEQ_LIBS) mlacli mla pub medline biblio general xser xconnect xutil xncbi

LIBS = $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

WATCHERS = stakhovv gotvyans

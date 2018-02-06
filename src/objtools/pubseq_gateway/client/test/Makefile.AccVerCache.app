# $Id$

APP = AccVerCache
SRC = AccVerCacheApp
LIB = psg_client psg_rpc psg_diag xncbi

LIBS = $(PSG_RPC_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(PSG_RPC_INCLUDE) $(ORIG_CPPFLAGS)

WATCHERS = sadyrovr dmitrie1

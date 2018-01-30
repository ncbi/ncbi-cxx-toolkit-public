# $Id$

APP = AccVerCache
SRC = AccVerCacheApp
LIB = psg_diag psg_rpc psg_api xncbi

LIBS = $(PSG_RPC_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(PSG_RPC_INCLUDE) $(ORIG_CPPFLAGS)

WATCHERS = sadyrovr dmitrie1

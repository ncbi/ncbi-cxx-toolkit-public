# $Id$

APP = logs_replay
SRC = logs_replay
LIB = ncbi_xcache_netcache xconnserv xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

#REQUIRES = MT
REQUIRES = MT Linux

WATCHERS = gouriano

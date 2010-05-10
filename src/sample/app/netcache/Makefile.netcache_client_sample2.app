# $Id$

APP = netcache_client_sample2
SRC = netcache_client_sample2
LIB = ncbi_xblobstorage_netcache xconnserv xthrserv xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


WATCHERS = kazimird

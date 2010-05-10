# $Id$

APP = grid_worker_sample
SRC = grid_worker_sample

### BEGIN COPIED SETTINGS

LIB = ncbi_xblobstorage_netcache xconnserv xthrserv xconnect xutil xncbi 

REQUIRES = MT

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

### END COPIED SETTINGS

WATCHERS = kazimird

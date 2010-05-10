# $Id$

APP = remote_app_client_sample
SRC = remote_app_client_sample

### BEGIN COPIED SETTINGS

LIB = ncbi_xblobstorage_netcache xconnserv xthrserv xconnect xutil xncbi 

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

### END COPIED SETTINGS

WATCHERS = kazimird

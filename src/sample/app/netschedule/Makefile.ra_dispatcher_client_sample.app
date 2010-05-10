# $Id$

APP = ra_dispatcher_client_sample
SRC = ra_dispatcher_client_sample

### BEGIN COPIED SETTINGS

LIB = ncbi_xblobstorage_file xconnserv xconnect xutil xncbi 

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

### END COPIED SETTINGS

WATCHERS = kazimird

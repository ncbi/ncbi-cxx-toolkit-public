# $Id$

APP = remote_app_dispatcher.cgi
SRC = remote_app_dispatcher
LIB = ncbi_xblobstorage_netcache xconnserv xconnect xcgi xutil xncbi 

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


# $Id$

SRC = file_upload
APP = file_upload.cgi

LIB = netstorage connssl ncbi_xcache_netcache \
      xconnserv xcgi xthrserv xconnect xutil test_boost xncbi

LIBS = $(GNUTLS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = sadyrovr

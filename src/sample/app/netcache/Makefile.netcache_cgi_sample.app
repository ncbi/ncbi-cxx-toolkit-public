# $Id$

APP = netcache_cgi_sample.cgi
SRC = netcache_cgi_sample
LIB = ncbi_xblobstorage_netcache xconnserv xconnect xcgi xhtml xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


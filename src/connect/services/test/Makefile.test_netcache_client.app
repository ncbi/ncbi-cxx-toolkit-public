# $Id$

APP = test_netcache_client
SRC = test_netcache_client
LIB = xconnserv$(FORCE_STATIC) xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)


#################################
# $Id$
#################################

APP = netcache_control
SRC = netcache_control



LIB = xconnserv$(FORCE_STATIC) xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

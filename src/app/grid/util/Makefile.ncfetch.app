#################################
# $Id$
#################################

PROJ_TAG = grid

APP = ncfetch.cgi
SRC = ncfetch

LIB = netstorage ncbi_xcache_netcache \
      xcgi xconnserv xthrserv xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = sadyrovr

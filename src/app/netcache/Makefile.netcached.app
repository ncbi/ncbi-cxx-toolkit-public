#################################
# $Id$
#################################

APP = netcached
SRC = netcached

REQUIRES = MT bdb


LIB = bdb_cache bdb xthrserv xconnect xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

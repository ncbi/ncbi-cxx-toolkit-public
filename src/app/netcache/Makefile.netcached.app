#################################
# $Id$
#################################

APP = netcached
SRC = netcached

REQUIRES = MT bdb


LIB = bdb xthrserv xconnect xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

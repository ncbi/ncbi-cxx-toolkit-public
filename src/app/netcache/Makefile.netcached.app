#################################
# $Id$
#################################

APP = netcached
SRC = netcached

REQUIRES = MT bdb


LIB = ncbi_xcache_bdb$(STATIC) $(BDB_LIB) xthrserv xconnserv xconnect \
      xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

#################################
# $Id$
#################################

APP = netcached
SRC = netcached message_handler smng_thread

REQUIRES = MT bdb


LIB = ncbi_xcache_bdb$(STATIC) $(BDB_LIB) $(COMPRESS_LIBS) xconnserv xthrserv \
      xconnect xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

#################################
# $Id$
#################################

APP = netcached
SRC = netcached request_handler message_handler ic_handler nc_handler smng_thread

REQUIRES = MT bdb


LIB = ncbi_xcache_bdb$(STATIC) $(BDB_LIB) $(COMPRESS_LIBS) xconnserv xthrserv \
      xconnect xutil xncbi 
LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

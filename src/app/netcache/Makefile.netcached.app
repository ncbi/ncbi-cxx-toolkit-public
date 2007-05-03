#################################
# $Id$
#################################

APP = netcached
SRC = netcached nc_request_parser process_ic smng_thread

REQUIRES = MT bdb


LIB = ncbi_xcache_bdb$(STATIC) $(BDB_LIB) $(COMPRESS_LIBS) xconnserv xthrserv \
      xconnect xutil xncbi 
LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

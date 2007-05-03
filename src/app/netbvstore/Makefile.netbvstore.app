#################################
# $Id$
#################################

APP = netbvstore
SRC = netbvstore

REQUIRES = MT bdb


LIB = $(BDB_LIB) $(COMPRESS_LIBS) xthrserv xconnserv \
      xconnect xutil xncbi 
LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

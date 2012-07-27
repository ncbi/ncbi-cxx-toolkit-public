# $Id$

WATCHERS = vakatov

APP = bdb_env_keeper
SRC = bdb_env_keeper
LIB = xthrserv xconnect xutil xncbi


LIB =  $(BDB_LIB) $(COMPRESS_LIBS) xconnserv xthrserv xconnect xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


REQUIRES = MT bdb


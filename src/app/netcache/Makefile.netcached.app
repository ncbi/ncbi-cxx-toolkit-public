#################################
# $Id$
#################################

APP = netcached
SRC = netcached

REQUIRES = MT


LIB = bdb xconnect xthrserv xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(DL_LIBS) $(ORIG_LIBS)

#################################
# $Id$
#################################

APP = netcached
SRC = netcached

REQUIRES = MT


LIB = bdb xncbi xutil xconnect xthrserv
LIBS = $(BERKELEYDB_LIBS) $(ORIG_LIBS)

#################################
# $Id$
#################################

APP = netcached
SRC = netcached

LIB = bdb xncbi xutil xconnect xthrserv
LIBS = $(BERKELEYDB_LIBS) $(ORIG_LIBS)

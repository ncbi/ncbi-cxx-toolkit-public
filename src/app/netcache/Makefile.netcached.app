#################################
# $Id$
#################################

APP = netcached
SRC = netcached message_handler \
      nc_storage nc_storage_blob nc_db_files nc_stat nc_memory nc_utils

REQUIRES = MT SQLITE3


LIB = xconnserv xthrserv xconnect xutil xncbi sqlitewrapp
LIBS = $(SQLITE3_STATIC_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(SQLITE3_INCLUDE) $(ORIG_CPPFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)


WATCHERS = ivanovp

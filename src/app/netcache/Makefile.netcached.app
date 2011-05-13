#################################
# $Id$
#################################

APP = netcached
SRC = netcached message_handler sync_log mirroring distribution_conf \
      nc_storage nc_storage_blob nc_db_files nc_stat nc_memory nc_utils \
      periodic_sync

REQUIRES = MT SQLITE3


LIB = xconnserv xthrserv xconnect xutil xncbi sqlitewrapp
LIBS = $(SQLITE3_STATIC_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(SQLITE3_INCLUDE) $(ORIG_CPPFLAGS)
#CXXFLAGS = $(FAST_CXXFLAGS)
#LDFLAGS = $(FAST_LDFLAGS)


WATCHERS = ivanovp

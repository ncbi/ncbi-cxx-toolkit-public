#################################
# $Id$
#################################

APP = netcached
SRC = netcached message_handler sync_log distribution_conf \
      nc_storage nc_storage_blob nc_db_files nc_stat nc_utils \
      periodic_sync active_handler peer_control nc_lib

#REQUIRES = MT SQLITE3 Boost.Test.Included
REQUIRES = MT SQLITE3 Boost.Test.Included Linux GCC


LIB = task_server
LIBS = $(SQLITE3_STATIC_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(NETCACHE_MEMORY_MAN_MODEL) $(SQLITE3_INCLUDE) $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)


WATCHERS = gouriano

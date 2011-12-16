#################################
# $Id$
#################################

APP = netcached
SRC = netcached message_handler sync_log distribution_conf \
      nc_storage nc_storage_blob nc_db_files nc_stat nc_memory nc_utils \
      periodic_sync active_handler peer_control

#REQUIRES = MT SQLITE3 Boost.Test.Included
REQUIRES = MT SQLITE3 Boost.Test.Included Linux


LIB = xconnserv xthrserv xconnect xutil xncbi sqlitewrapp
LIBS = $(SQLITE3_STATIC_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(SQLITE3_INCLUDE) $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)
#CXXFLAGS = $(FAST_CXXFLAGS)
#LDFLAGS = $(FAST_LDFLAGS)


WATCHERS = ivanovp

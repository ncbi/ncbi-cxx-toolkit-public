#################################
# $Id$
#################################

WATCHERS = satskyse

APP = netstoraged
SRC = netstoraged nst_server_parameters nst_exception nst_connection_factory \
      nst_server nst_handler nst_clients nst_protocol_utils nst_database \
      nst_alert nst_config nst_warning nst_dbconnection_thread \
      nst_metadata_options nst_service_thread nst_util nst_service_parameters \
      nst_timing

REQUIRES = MT Linux


LIB =  netstorage ncbi_xcache_netcache xconnserv xthrserv \
       $(SDBAPI_LIB) xconnect connssl xutil xncbi
LIBS = $(SDBAPI_LIBS) $(NETWORK_LIBS) $(GNUTLS_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS)


#################################
# $Id$
#################################

WATCHERS = satskyse

APP = netstoraged
SRC = netstoraged nst_server_parameters nst_exception nst_connection_factory \
      nst_server nst_handler nst_clients nst_protocol_utils nst_dbapp \
      nst_alert nst_config

REQUIRES = MT Linux


LIB =  ncbi_xcache_netcache netstorage xconnserv xthrserv \
       $(SDBAPI_LIB) xconnect connssl xutil xncbi
LIBS = $(SDBAPI_LIBS) $(NETWORK_LIBS) $(GNUTLS_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS)


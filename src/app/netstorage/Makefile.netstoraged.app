#################################
# $Id$
#################################

WATCHERS = satskyse

APP = netstoraged
SRC = netstoraged nst_server_parameters nst_exception nst_connection_factory \
      nst_server nst_handler nst_clients nst_protocol_utils

REQUIRES = MT Linux


LIB =  xconnserv xthrserv xconnect xutil xncbi
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS)


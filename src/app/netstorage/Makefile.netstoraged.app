#################################
# $Id$
#################################

WATCHERS = satskyse

APP = netstoraged
SRC = netstoraged nst_server_parameters

REQUIRES = MT Linux


LIB =  xconnserv xthrserv xconnect xutil xncbi
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS)


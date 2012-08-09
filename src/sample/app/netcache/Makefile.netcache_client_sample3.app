# $Id$

APP = netcache_client_sample3
SRC = netcache_client_sample3

### BEGIN COPIED SETTINGS
LIB = xconnserv xthrserv xconnect \
      xcompress $(CMPRS_LIB) xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(CMPRS_INCLUDE)
### END COPIED SETTINGS


WATCHERS = kazimird

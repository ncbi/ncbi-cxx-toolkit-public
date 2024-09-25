# $Id$

REQUIRES = dbapi

APP = dbcopy
SRC = dbcopy dbcopy_common

### BEGIN COPIED SETTINGS
LIB  = dbapi dbapi_driver xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
### END COPIED SETTINGS

WATCHERS = ucko

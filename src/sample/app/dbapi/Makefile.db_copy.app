# $Id$

REQUIRES = dbapi

APP = db_copy
SRC = db_copy

### BEGIN COPIED SETTINGS
LIB  = dbapi dbapi_driver xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
### END COPIED SETTINGS

WATCHERS = ucko

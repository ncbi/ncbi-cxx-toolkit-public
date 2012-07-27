# $Id$

REQUIRES = dbapi

APP = dbapi_advanced_features
SRC = dbapi_advanced_features

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB  = dbapi dbapi_driver $(XCONNEXT) xconnect xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
### END COPIED SETTINGS

# CHECK_CMD = dbapi_advanced_features.sh
# CHECK_COPY = dbapi_advanced_features.sh

WATCHERS = ucko

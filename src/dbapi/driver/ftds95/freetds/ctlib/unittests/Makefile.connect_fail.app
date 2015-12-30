# $Id$

APP = ct95_connect_fail
SRC = connect_fail common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS95_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = $(FTDS95_CTLIB_LIB)
LIBS     = $(FTDS95_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

# Attempts to log in as sa with invalid credentials; disabled per DBA request.
# CHECK_CMD  = test-ct95 ct95_connect_fail

WATCHERS = ucko

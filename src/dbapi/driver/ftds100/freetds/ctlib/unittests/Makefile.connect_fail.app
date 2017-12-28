# $Id$

APP = ct100_connect_fail
SRC = connect_fail common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS100_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = $(FTDS100_CTLIB_LIB)
LIBS     = $(FTDS100_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

# Attempts to log in as sa with invalid credentials; disabled per DBA request.
# CHECK_CMD  = test-ct100 ct100_connect_fail

WATCHERS = ucko satskyse

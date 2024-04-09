# $Id$

APP = ct14_connect_fail
SRC = connect_fail common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS14_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = $(FTDS14_CTLIB_LIB)
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

# Attempts to log in as sa with invalid credentials; disabled per DBA request.
# CHECK_CMD  = test-ct14 ct14_connect_fail

WATCHERS = ucko satskyse

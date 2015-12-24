# $Id$

APP = ct95_ct_cursor
SRC = ct_cursor common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS95_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = $(FTDS95_CTLIB_LIB)
LIBS     = $(FTDS95_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-ct95.sh ct95_ct_cursor
CHECK_COPY = test-ct95.sh

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko

# $Id$

APP = ct95_get_send_data
SRC = get_send_data common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS95_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = $(FTDS95_CTLIB_LIB)
LIBS     = $(FTDS95_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

# Needs automatic textptr metadata, gone as of TDS 7.2.
CHECK_CMD  = test-ct95.sh --ms-ver 7.1 ct95_get_send_data

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko

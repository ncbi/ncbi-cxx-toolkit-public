# $Id$

APP = ct95_rpc_ct_setparam
SRC = rpc_ct_setparam common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS95_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = $(FTDS95_CTLIB_LIB)
LIBS     = $(FTDS95_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-ct95 --no-auto ct95_rpc_ct_setparam

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko

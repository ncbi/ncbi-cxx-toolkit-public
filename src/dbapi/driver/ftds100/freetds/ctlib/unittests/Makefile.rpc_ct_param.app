# $Id$

APP = ct100_rpc_ct_param
SRC = rpc_ct_param common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS100_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = $(FTDS100_CTLIB_LIB)
LIBS     = $(FTDS100_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-ct100 --no-auto ct100_rpc_ct_param

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse

# $Id$

APP = ct14_rpc_ct_param
SRC = rpc_ct_param common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS14_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = $(FTDS14_CTLIB_LIB)
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-ct14 --no-auto ct14_rpc_ct_param

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse

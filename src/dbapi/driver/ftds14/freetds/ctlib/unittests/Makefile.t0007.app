# $Id$

APP = ct14_t0007
SRC = t0007 common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS14_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = $(FTDS14_CTLIB_LIB)
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-ct14 ct14_t0007

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko elisovdn satskyse
# $Id$

APP = tds100_t0003
SRC = t0003 common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS100_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = tds_ftds100
LIBS     = $(FTDS100_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-tds100 tds100_t0003

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse

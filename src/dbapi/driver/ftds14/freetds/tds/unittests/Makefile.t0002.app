# $Id$

APP = tds14_t0002
SRC = t0002

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS14_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = tds_ut_common_ftds14 tds_ftds14
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-tds14 tds14_t0002

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse

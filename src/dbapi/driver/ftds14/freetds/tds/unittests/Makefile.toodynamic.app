# $Id$

APP = tds14_toodynamic
SRC = toodynamic

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS14_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = tds_ut_common_ftds14 tds_ftds14
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-tds14 tds14_toodynamic

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse

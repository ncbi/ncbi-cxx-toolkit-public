# $Id$

APP = tds95_t0006
SRC = t0006 common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS95_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = tds_ftds95$(STATIC)
LIBS     = $(FTDS95_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-tds95.sh tds95_t0006

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko

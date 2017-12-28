# $Id$

APP = tds100_utf8_1
SRC = utf8_1 common utf8

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS100_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = tds_ftds100$(STATIC)
LIBS     = $(FTDS100_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-tds100 tds100_utf8_1

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse

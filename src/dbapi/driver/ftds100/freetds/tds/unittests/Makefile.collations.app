# $Id$

APP = tds100_collations
SRC = collations common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS100_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = tds_ftds100$(STATIC)
LIBS     = $(FTDS100_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

# Per upstream, really a utility, and as such not worth running as a test.
# CHECK_CMD  = test-tds100 tds100_collations

# CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse
